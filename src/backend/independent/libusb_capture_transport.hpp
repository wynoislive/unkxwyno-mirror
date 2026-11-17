#pragma once
#include "usb_capture_transport.hpp"
#include <atomic>
#include <memory>
#include <functional>
#include <string>
namespace unkxwyno::usb {
class LibUsbCaptureTransport final : public UsbCaptureTransport {
public:
    static std::unique_ptr<LibUsbCaptureTransport> open_auto(const std::atomic_bool& stop,
        std::function<void(const std::string&)> progress = {});
    ~LibUsbCaptureTransport() override;
    std::optional<CapturePacket> read_packet(std::chrono::milliseconds wait) override;
    void write_packet(std::span<const std::uint8_t> packet) override;
    void request_stop() noexcept override;
private:
    struct Impl;
    explicit LibUsbCaptureTransport(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};
}
