/**
 * @file winusb_transport.hpp
 * @brief Windows WinUSB implementation for raw asynchronous USB communication.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#pragma once

#include "usb_transport.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winusb.h>
#include <setupapi.h>
#include <initguid.h>
#include <usbiodef.h>
#include <atomic>
#include <mutex>
#include <thread>

namespace unkxwyno::usb {

class WinUsbDeviceSession final : public IUsbDeviceSession {
public:
    WinUsbDeviceSession(UsbDeviceDescriptor desc, HANDLE device_handle, WINUSB_INTERFACE_HANDLE winusb_handle);
    ~WinUsbDeviceSession() override;

    [[nodiscard]] const UsbDeviceDescriptor& GetDescriptor() const override { return descriptor_; }
    [[nodiscard]] bool IsOpen() const override;

    UsbStatus ControlTransfer(const UsbControlSetup& setup,
                              uint8_t* buffer,
                              uint16_t length,
                              uint32_t timeout_ms = 1000) override;

    UsbStatus BulkRead(uint8_t endpoint_address,
                       uint8_t* buffer,
                       size_t length,
                       size_t& bytes_transferred,
                       uint32_t timeout_ms = 2000) override;

    UsbStatus BulkWrite(uint8_t endpoint_address,
                        const uint8_t* data,
                        size_t length,
                        size_t& bytes_transferred,
                        uint32_t timeout_ms = 2000) override;

    void Close() override;

private:
    UsbDeviceDescriptor descriptor_;
    HANDLE device_handle_{INVALID_HANDLE_VALUE};
    WINUSB_INTERFACE_HANDLE winusb_handle_{nullptr};
    std::mutex io_mutex_;
    std::mutex read_mutex_;
    std::mutex write_mutex_;
    HANDLE read_event_{nullptr};
    HANDLE write_event_{nullptr};
};

class WinUsbTransport final : public IUsbTransport {
public:
    WinUsbTransport();
    ~WinUsbTransport() override;

    UsbStatus Initialize() override;
    void Shutdown() override;

    [[nodiscard]] std::vector<UsbDeviceDescriptor> EnumerateDevices() override;
    std::shared_ptr<IUsbDeviceSession> OpenDevice(const std::string& device_id) override;

    void SetDeviceCallbacks(DeviceAttachCallback on_attach,
                            DeviceDetachCallback on_detach) override;

private:
    std::atomic<bool> running_{false};
    DeviceAttachCallback on_attach_;
    DeviceDetachCallback on_detach_;
    std::thread monitor_thread_;
};

} // namespace unkxwyno::usb
#endif // _WIN32
