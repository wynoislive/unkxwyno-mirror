#pragma once
#include "usb_capture_protocol.hpp"
#include <chrono>
#include <stdexcept>
namespace unkxwyno::usb {
class UsbControlError : public std::runtime_error {
public: using std::runtime_error::runtime_error;
};
class UsbDriverRequiredError : public std::runtime_error {
public: using std::runtime_error::runtime_error;
};
class UXM_API UsbCaptureTransport {
public:
    virtual ~UsbCaptureTransport() = default;
    virtual std::optional<CapturePacket> read_packet(std::chrono::milliseconds wait = std::chrono::milliseconds(200)) = 0;
    virtual void write_packet(std::span<const std::uint8_t> packet) = 0;
    virtual void request_stop() noexcept = 0;
    virtual bool has_control_stream() const noexcept { return false; }
    virtual int control_read(std::span<std::uint8_t>,std::chrono::milliseconds) { throw UsbControlError("This USB adapter has no device control stream."); }
    virtual void control_write(std::span<const std::uint8_t>) { throw UsbControlError("This USB adapter has no device control stream."); }
};
}
