#pragma once
#include "usb_capture_protocol.hpp"
#include <string>
namespace unkxwyno::usb {
struct UsbMuxActions {
    std::vector<std::vector<std::uint8_t>> outgoing;
    std::string event;
};
// The USB-level multiplexor, distinct from the host's plist usbmux service.
// Owns one local TCP-like tunnel to lockdownd, for bounded read-only checks.
// Does not request pair records, keys, device data, or authenticated services.
class UXM_API UsbMuxControl {
public:
    [[nodiscard]] std::vector<std::uint8_t> start() const;
    UsbMuxActions receive(std::span<const std::uint8_t> bytes);
    [[nodiscard]] bool ready() const noexcept { return ready_; }
    std::vector<std::uint8_t> take_control_data();
    std::vector<std::vector<std::uint8_t>> write_control_data(std::span<const std::uint8_t> bytes);
private:
    unsigned version_{};
    std::uint16_t link_tx_{}, link_rx_{0xffff};
    std::uint32_t tcp_tx_{}, tcp_rx_{};
    bool connected_{}, ready_{}, failed_{};
    unsigned query_stage_{};
    std::vector<std::uint8_t> incoming_, plist_, control_data_;
    std::vector<std::uint8_t> packet(std::uint32_t protocol, std::span<const std::uint8_t> payload);
    std::vector<std::uint8_t> tcp(std::uint8_t flags, std::span<const std::uint8_t> payload = {});
    std::vector<std::uint8_t> query(bool version);
    void process(std::span<const std::uint8_t> packet, UsbMuxActions& actions);
};
}
