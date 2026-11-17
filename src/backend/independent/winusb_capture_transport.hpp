#pragma once
#include "usb_capture_transport.hpp"
#include <memory>
#include <string>
#include <chrono>

namespace unkxwyno::usb {

// Opens only an already-exposed Apple FF/2A/FF bulk capture interface.
// Does not switch modes, install drivers, pair a device or claim a video session.
class UXM_API WinUsbCaptureTransport final : public UsbCaptureTransport {
public:
    static std::unique_ptr<WinUsbCaptureTransport> open(const std::wstring& device_interface_path);
    ~WinUsbCaptureTransport() override;
    WinUsbCaptureTransport(const WinUsbCaptureTransport&) = delete;
    WinUsbCaptureTransport& operator=(const WinUsbCaptureTransport&) = delete;
    // One serialized reader; bounded 16 KiB transfers and 200 ms timeout polls.
    // Null means stopped or the bounded poll elapsed. Partial packets are kept.
    [[nodiscard]] std::optional<CapturePacket> read_packet(std::chrono::milliseconds wait = std::chrono::milliseconds(200)) override;
    // A partial/failed write invalidates this session rather than being retried.
    void write_packet(std::span<const std::uint8_t> packet) override;
    void request_stop() noexcept override;
private:
    struct Impl;
    explicit WinUsbCaptureTransport(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};
} // namespace unkxwyno::usb
