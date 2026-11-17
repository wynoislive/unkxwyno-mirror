#pragma once

#include "unkxwyno/core/c_api.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace unkxwyno::usb {

enum class PacketFamily : std::uint32_t {
    Ping = 0x70696e67U,
    Sync = 0x73796e63U,
    Async = 0x6173796eU,
    Reply = 0x72706c79U,
};

enum class ParseStatus { NeedMore, PacketReady, InvalidLength, UnsupportedFamily };

struct CapturePacket {
    PacketFamily family{};
    std::vector<std::uint8_t> bytes;
    [[nodiscard]] UXM_API std::optional<std::uint32_t> subtype() const noexcept;
    // No audio/video identity is assigned until the nested sample schema is known.
    [[nodiscard]] UXM_API std::optional<std::span<const std::uint8_t>> sample_buffer() const noexcept;
};

struct ParseResult {
    ParseStatus status{ParseStatus::NeedMore};
    std::size_t consumed{};
    std::optional<CapturePacket> packet;
};

struct BulkEndpoint {
    std::uint8_t address{};
    std::uint16_t maximum_packet_bytes{};
    bool bulk{};
};
struct CaptureEndpoints {
    std::uint8_t input{}, output{};
};
[[nodiscard]] UXM_API std::optional<CaptureEndpoints> select_capture_endpoints(
    std::span<const BulkEndpoint> endpoints) noexcept;

class UXM_API CapturePacketReader final {
public:
    explicit CapturePacketReader(std::size_t maximum_packet_bytes = 4U * 1024U * 1024U);
    // Consumes at most one frame; the caller retains any following bytes.
    // Invalid input is sticky until reset, preventing accidental resynchronization.
    [[nodiscard]] ParseResult consume(std::span<const std::uint8_t> input);
    void reset() noexcept;
    [[nodiscard]] std::size_t buffered_bytes() const noexcept { return buffer_.size(); }
private:
    std::size_t limit_;
    std::size_t expected_{4};
    ParseStatus error_{ParseStatus::NeedMore};
    std::vector<std::uint8_t> buffer_;
};

[[nodiscard]] UXM_API std::array<std::uint8_t, 16> encode_capture_ping() noexcept;
[[nodiscard]] UXM_API std::array<std::uint8_t, 20> encode_capture_need(
    std::uint32_t opaque_word_1, std::uint32_t opaque_word_2) noexcept;
[[nodiscard]] UXM_API bool needs_zero_length_transfer(
    std::size_t payload_bytes, std::size_t endpoint_packet_bytes) noexcept;

} // namespace unkxwyno::usb
