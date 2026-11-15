/**
 * @file usb_transport.hpp
 * @brief Cross-platform USB transport interface for unkXwyno Mirror Core.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <span>

namespace unkxwyno::usb {

struct UsbDeviceDescriptor {
    std::string device_id;
    std::string device_path;
    uint16_t vendor_id{0};
    uint16_t product_id{0};
    std::string serial_number;
    std::string manufacturer;
    std::string product_name;
    bool is_android_aoa{false};
    bool is_apple_ios{false};
};

struct UsbControlSetup {
    uint8_t  request_type{0}; // bmRequestType
    uint8_t  request{0};      // bRequest
    uint16_t value{0};        // wValue
    uint16_t index{0};        // wIndex
    uint16_t length{0};       // wLength
};

enum class UsbStatus {
    Success = 0,
    ErrorIo = -1,
    ErrorTimeout = -2,
    ErrorNotFound = -3,
    ErrorAccessDenied = -4,
    ErrorNotSupported = -5,
    ErrorCancelled = -6
};

using DataCallback = std::function<void(std::span<const uint8_t> data)>;
using DeviceAttachCallback = std::function<void(const UsbDeviceDescriptor& device)>;
using DeviceDetachCallback = std::function<void(const std::string& device_id)>;

/**
 * @brief Abstract raw USB device session.
 */
class IUsbDeviceSession {
public:
    virtual ~IUsbDeviceSession() = default;

    [[nodiscard]] virtual const UsbDeviceDescriptor& GetDescriptor() const = 0;
    [[nodiscard]] virtual bool IsOpen() const = 0;

    virtual UsbStatus ControlTransfer(const UsbControlSetup& setup,
                                      uint8_t* buffer,
                                      uint16_t length,
                                      uint32_t timeout_ms = 1000) = 0;

    virtual UsbStatus BulkRead(uint8_t endpoint_address,
                               uint8_t* buffer,
                               size_t length,
                               size_t& bytes_transferred,
                               uint32_t timeout_ms = 2000) = 0;

    virtual UsbStatus BulkWrite(uint8_t endpoint_address,
                                const uint8_t* data,
                                size_t length,
                                size_t& bytes_transferred,
                                uint32_t timeout_ms = 2000) = 0;

    virtual void Close() = 0;
};

/**
 * @brief Abstract USB host controller driver interface.
 */
class IUsbTransport {
public:
    virtual ~IUsbTransport() = default;

    virtual UsbStatus Initialize() = 0;
    virtual void Shutdown() = 0;

    [[nodiscard]] virtual std::vector<UsbDeviceDescriptor> EnumerateDevices() = 0;
    virtual std::shared_ptr<IUsbDeviceSession> OpenDevice(const std::string& device_id) = 0;

    virtual void SetDeviceCallbacks(DeviceAttachCallback on_attach,
                                    DeviceDetachCallback on_detach) = 0;
};

} // namespace unkxwyno::usb
