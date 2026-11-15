/**
 * @file winusb_transport.cpp
 * @brief Implementation of WinUsbTransport and WinUsbDeviceSession.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#include "winusb_transport.hpp"

#ifdef _WIN32
#include <iostream>
#include <chrono>
#include <algorithm>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "winusb.lib")

namespace unkxwyno::usb {

WinUsbDeviceSession::WinUsbDeviceSession(UsbDeviceDescriptor desc,
                                         HANDLE device_handle,
                                         WINUSB_INTERFACE_HANDLE winusb_handle)
    : descriptor_(std::move(desc)),
      device_handle_(device_handle),
      winusb_handle_(winusb_handle) {
    if (winusb_handle_ != nullptr) {
        // Optimize pipe policies: Enable raw I/O and auto flush
        UCHAR raw_io = TRUE;
        WinUsb_SetPipePolicy(winusb_handle_, 0x81, RAW_IO, sizeof(raw_io), &raw_io);
        WinUsb_SetPipePolicy(winusb_handle_, 0x01, RAW_IO, sizeof(raw_io), &raw_io);
    }
}

WinUsbDeviceSession::~WinUsbDeviceSession() {
    Close();
}

bool WinUsbDeviceSession::IsOpen() const {
    return winusb_handle_ != nullptr && device_handle_ != INVALID_HANDLE_VALUE;
}

void WinUsbDeviceSession::Close() {
    std::lock_guard<std::mutex> lock(io_mutex_);
    if (winusb_handle_) {
        WinUsb_Free(winusb_handle_);
        winusb_handle_ = nullptr;
    }
    if (device_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(device_handle_);
        device_handle_ = INVALID_HANDLE_VALUE;
    }
}

UsbStatus WinUsbDeviceSession::ControlTransfer(const UsbControlSetup& setup,
                                              uint8_t* buffer,
                                              uint16_t length,
                                              uint32_t timeout_ms) {
    std::lock_guard<std::mutex> lock(io_mutex_);
    if (!IsOpen()) return UsbStatus::ErrorNotFound;

    WINUSB_SETUP_PACKET packet;
    packet.RequestType = setup.request_type;
    packet.Request     = setup.request;
    packet.Value       = setup.value;
    packet.Index       = setup.index;
    packet.Length      = length;

    ULONG transferred = 0;
    BOOL ok = WinUsb_ControlTransfer(winusb_handle_, packet, buffer, length, &transferred, nullptr);
    if (!ok) {
        DWORD err = GetLastError();
        if (err == ERROR_TIMEOUT) return UsbStatus::ErrorTimeout;
        return UsbStatus::ErrorIo;
    }

    return UsbStatus::Success;
}

UsbStatus WinUsbDeviceSession::BulkRead(uint8_t endpoint_address,
                                       uint8_t* buffer,
                                       size_t length,
                                       size_t& bytes_transferred,
                                       uint32_t timeout_ms) {
    bytes_transferred = 0;
    if (!IsOpen()) return UsbStatus::ErrorNotFound;

    ULONG to_read = static_cast<ULONG>(length);
    ULONG transferred = 0;

    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent) return UsbStatus::ErrorIo;

    BOOL ok = WinUsb_ReadPipe(winusb_handle_, endpoint_address, buffer, to_read, &transferred, &ov);
    if (!ok) {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING) {
            DWORD wait_res = WaitForSingleObject(ov.hEvent, timeout_ms);
            if (wait_res == WAIT_OBJECT_0) {
                if (GetOverlappedResult(device_handle_, &ov, &transferred, FALSE)) {
                    bytes_transferred = transferred;
                    CloseHandle(ov.hEvent);
                    return UsbStatus::Success;
                }
            } else if (wait_res == WAIT_TIMEOUT) {
                CancelIoEx(device_handle_, &ov);
                CloseHandle(ov.hEvent);
                return UsbStatus::ErrorTimeout;
            }
        }
        CloseHandle(ov.hEvent);
        return UsbStatus::ErrorIo;
    }

    bytes_transferred = transferred;
    CloseHandle(ov.hEvent);
    return UsbStatus::Success;
}

UsbStatus WinUsbDeviceSession::BulkWrite(uint8_t endpoint_address,
                                        const uint8_t* data,
                                        size_t length,
                                        size_t& bytes_transferred,
                                        uint32_t timeout_ms) {
    bytes_transferred = 0;
    if (!IsOpen()) return UsbStatus::ErrorNotFound;

    ULONG to_write = static_cast<ULONG>(length);
    ULONG transferred = 0;

    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent) return UsbStatus::ErrorIo;

    // Const cast is safe here because WinUsb_WritePipe does not modify outgoing buffers
    BOOL ok = WinUsb_WritePipe(winusb_handle_, endpoint_address, const_cast<PUCHAR>(data), to_write, &transferred, &ov);
    if (!ok) {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING) {
            DWORD wait_res = WaitForSingleObject(ov.hEvent, timeout_ms);
            if (wait_res == WAIT_OBJECT_0) {
                if (GetOverlappedResult(device_handle_, &ov, &transferred, FALSE)) {
                    bytes_transferred = transferred;
                    CloseHandle(ov.hEvent);
                    return UsbStatus::Success;
                }
            } else {
                CancelIoEx(device_handle_, &ov);
                CloseHandle(ov.hEvent);
                return UsbStatus::ErrorTimeout;
            }
        }
        CloseHandle(ov.hEvent);
        return UsbStatus::ErrorIo;
    }

    bytes_transferred = transferred;
    CloseHandle(ov.hEvent);
    return UsbStatus::Success;
}

// ============================================================================
// WinUsbTransport implementation
// ============================================================================

WinUsbTransport::WinUsbTransport() = default;

WinUsbTransport::~WinUsbTransport() {
    Shutdown();
}

UsbStatus WinUsbTransport::Initialize() {
    running_ = true;
    return UsbStatus::Success;
}

void WinUsbTransport::Shutdown() {
    running_ = false;
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
}

void WinUsbTransport::SetDeviceCallbacks(DeviceAttachCallback on_attach,
                                         DeviceDetachCallback on_detach) {
    on_attach_ = std::move(on_attach);
    on_detach_ = std::move(on_detach);
}

std::vector<UsbDeviceDescriptor> WinUsbTransport::EnumerateDevices() {
    std::vector<UsbDeviceDescriptor> devices;

    // Query all USB device interface arrivals using Win32 SetupAPI
    HDEVINFO dev_info = SetupDiGetClassDevsW(&GUID_DEVINTERFACE_USB_DEVICE, nullptr, nullptr,
                                            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (dev_info == INVALID_HANDLE_VALUE) {
        return devices;
    }

    SP_DEVICE_INTERFACE_DATA if_data{};
    if_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(dev_info, nullptr, &GUID_DEVINTERFACE_USB_DEVICE, i, &if_data); ++i) {
        DWORD required_size = 0;
        SetupDiGetDeviceInterfaceDetailW(dev_info, &if_data, nullptr, 0, &required_size, nullptr);
        if (required_size == 0) continue;

        auto detail_data_buffer = std::make_unique<uint8_t[]>(required_size);
        auto* detail_data = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(detail_data_buffer.get());
        detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        SP_DEVINFO_DATA dev_info_data{};
        dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

        if (SetupDiGetDeviceInterfaceDetailW(dev_info, &if_data, detail_data, required_size, nullptr, &dev_info_data)) {
            std::wstring path_w = detail_data->DevicePath;
            std::string path(path_w.begin(), path_w.end());

            UsbDeviceDescriptor desc{};
            desc.device_path = path;
            desc.device_id   = path;

            // Check if VID/PID indicates Google AOA (0x18D1) or Apple (0x05AC)
            std::string lower_path = path;
            std::transform(lower_path.begin(), lower_path.end(), lower_path.begin(), ::tolower);

            // Support all major Android OEMs & Apple iOS devices
            if (lower_path.find("vid_18d1") != std::string::npos) {
                desc.vendor_id = 0x18D1;
                desc.is_android_aoa = true;
                desc.manufacturer = "Google / Android";
                desc.product_name = "Pixel / AOA Device";
            } else if (lower_path.find("vid_04e8") != std::string::npos) {
                desc.vendor_id = 0x04E8;
                desc.is_android_aoa = true;
                desc.manufacturer = "Samsung Electronics";
                desc.product_name = "Galaxy Smartphone";
            } else if (lower_path.find("vid_2717") != std::string::npos) {
                desc.vendor_id = 0x2717;
                desc.is_android_aoa = true;
                desc.manufacturer = "Xiaomi Communications";
                desc.product_name = "Xiaomi / Redmi Device";
            } else if (lower_path.find("vid_2a70") != std::string::npos || lower_path.find("vid_22d9") != std::string::npos) {
                desc.vendor_id = 0x2A70;
                desc.is_android_aoa = true;
                desc.manufacturer = "OnePlus / Oppo";
                desc.product_name = "OnePlus Smartphone";
            } else if (lower_path.find("vid_2d95") != std::string::npos) {
                desc.vendor_id = 0x2D95;
                desc.is_android_aoa = true;
                desc.manufacturer = "Vivo Mobile";
                desc.product_name = "Vivo Smartphone";
            } else if (lower_path.find("vid_05ac") != std::string::npos) {
                desc.vendor_id = 0x05AC;
                desc.is_apple_ios = true;
                desc.manufacturer = "Apple Inc.";
                desc.product_name = "Apple iPhone / iPad";
            } else {
                desc.vendor_id = 0x18D1;
                desc.is_android_aoa = true;
                desc.manufacturer = "Android Mobile";
                desc.product_name = "USB Device";
            }

            devices.push_back(desc);
        }
    }

    SetupDiDestroyDeviceInfoList(dev_info);
    return devices;
}

std::shared_ptr<IUsbDeviceSession> WinUsbTransport::OpenDevice(const std::string& device_id) {
    std::wstring path_w(device_id.begin(), device_id.end());

    HANDLE dev_handle = CreateFileW(path_w.c_str(),
                                    GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr,
                                    OPEN_EXISTING,
                                    FILE_FLAG_OVERLAPPED,
                                    nullptr);

    if (dev_handle == INVALID_HANDLE_VALUE) {
        return nullptr;
    }

    WINUSB_INTERFACE_HANDLE winusb_handle = nullptr;
    if (!WinUsb_Initialize(dev_handle, &winusb_handle)) {
        CloseHandle(dev_handle);
        return nullptr;
    }

    UsbDeviceDescriptor desc{};
    desc.device_id = device_id;
    desc.device_path = device_id;

    return std::make_shared<WinUsbDeviceSession>(std::move(desc), dev_handle, winusb_handle);
}

} // namespace unkxwyno::usb
#endif // _WIN32
