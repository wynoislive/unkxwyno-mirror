#pragma once
#include "usb_capture_transport.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
namespace unkxwyno::usb {
class LibUsbWin32CaptureTransport final : public UsbCaptureTransport {
public:
    static std::unique_ptr<LibUsbWin32CaptureTransport> open_auto(const std::atomic_bool& stop,
        std::function<void(const std::string&)> progress);
    ~LibUsbWin32CaptureTransport() override;
    std::optional<CapturePacket> read_packet(std::chrono::milliseconds wait) override;
    void write_packet(std::span<const std::uint8_t> packet) override;
    void request_stop() noexcept override;
    bool has_control_stream() const noexcept override { return true; }
    int control_read(std::span<std::uint8_t> bytes,std::chrono::milliseconds timeout) override;
    void control_write(std::span<const std::uint8_t> bytes) override;
private:
    struct Impl;
    explicit LibUsbWin32CaptureTransport(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};
}
