#include "usb_capture_protocol.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace unkxwyno::usb {
namespace {
std::uint32_t read_word(std::span<const std::uint8_t> bytes, std::size_t offset) noexcept {
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}
void write_word(std::span<std::uint8_t> bytes, std::size_t offset, std::uint32_t value) noexcept {
    for (std::size_t index = 0; index < 4; ++index)
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8U));
}
std::size_t minimum_length(PacketFamily family) noexcept {
    switch (family) {
    case PacketFamily::Ping: return 8; // Receive dispatch reads only the family.
    case PacketFamily::Sync: return 28; // Includes both request correlation words.
    case PacketFamily::Async: return 20;
    case PacketFamily::Reply: return 16;
    }
    return 0;
}
} // namespace

CapturePacketReader::CapturePacketReader(std::size_t maximum_packet_bytes) : limit_(maximum_packet_bytes) {
    if (limit_ < 28 || limit_ > std::numeric_limits<std::uint32_t>::max())
        throw std::invalid_argument("USB packet bound must be between 28 and UINT32_MAX.");
}

ParseResult CapturePacketReader::consume(std::span<const std::uint8_t> input) {
    if (error_ != ParseStatus::NeedMore) return {error_, 0, std::nullopt};
    std::size_t consumed = 0;
    for (;;) {
        const auto count = std::min(expected_ - buffer_.size(), input.size() - consumed);
        buffer_.insert(buffer_.end(), input.begin() + static_cast<std::ptrdiff_t>(consumed),
            input.begin() + static_cast<std::ptrdiff_t>(consumed + count));
        consumed += count;
        if (buffer_.size() < expected_) return {ParseStatus::NeedMore, consumed, std::nullopt};
        if (expected_ == 4) {
            const auto length = static_cast<std::size_t>(read_word(buffer_, 0));
            if (length < 8 || length > limit_) {
                error_ = ParseStatus::InvalidLength;
                return {error_, consumed, std::nullopt};
            }
            expected_ = 8;
        } else if (expected_ == 8) {
            const auto family = static_cast<PacketFamily>(read_word(buffer_, 4));
            const auto minimum = minimum_length(family);
            if (minimum == 0) {
                error_ = ParseStatus::UnsupportedFamily;
                return {error_, consumed, std::nullopt};
            }
            expected_ = read_word(buffer_, 0);
            if (expected_ < minimum) {
                error_ = ParseStatus::InvalidLength;
                return {error_, consumed, std::nullopt};
            }
            // An eight-byte ping has no remaining body.
            if (buffer_.size() < expected_) continue;
            CapturePacket packet{family, std::move(buffer_)};
            buffer_ = {};
            expected_ = 4;
            return {ParseStatus::PacketReady, consumed, std::move(packet)};
        } else {
            CapturePacket packet{static_cast<PacketFamily>(read_word(buffer_, 4)), std::move(buffer_)};
            buffer_ = {};
            expected_ = 4;
            return {ParseStatus::PacketReady, consumed, std::move(packet)};
        }
    }
}

void CapturePacketReader::reset() noexcept {
    buffer_.clear();
    expected_ = 4;
    error_ = ParseStatus::NeedMore;
}

std::optional<std::uint32_t> CapturePacket::subtype() const noexcept {
    if ((family != PacketFamily::Sync && family != PacketFamily::Async) || bytes.size() < 20)
        return std::nullopt;
    return read_word(bytes, 16);
}

std::optional<std::span<const std::uint8_t>> CapturePacket::sample_buffer() const noexcept {
    const auto kind = subtype();
    if (family != PacketFamily::Async || !kind || (*kind != 0x66656564U && *kind != 0x65617421U)
        || bytes.size() < 28) return std::nullopt;
    const auto length = static_cast<std::size_t>(read_word(bytes, 20));
    if (length < 8 || length > bytes.size() - 20 || read_word(bytes, 24) != 0x73627566U)
        return std::nullopt;
    return std::span<const std::uint8_t>(bytes).subspan(28, length - 8);
}

std::array<std::uint8_t, 16> encode_capture_ping() noexcept {
    std::array<std::uint8_t, 16> bytes{};
    write_word(bytes, 0, 16);
    write_word(bytes, 4, static_cast<std::uint32_t>(PacketFamily::Ping));
    write_word(bytes, 8, 1);
    write_word(bytes, 12, 1);
    return bytes;
}

std::array<std::uint8_t, 20> encode_capture_need(std::uint32_t first, std::uint32_t second) noexcept {
    std::array<std::uint8_t, 20> bytes{};
    write_word(bytes, 0, 20);
    write_word(bytes, 4, static_cast<std::uint32_t>(PacketFamily::Async));
    write_word(bytes, 8, first);
    write_word(bytes, 12, second);
    write_word(bytes, 16, 0x6e656564U);
    return bytes;
}

bool needs_zero_length_transfer(std::size_t bytes, std::size_t packet_size) noexcept {
    return bytes != 0 && packet_size != 0 && bytes % packet_size == 0;
}

std::optional<CaptureEndpoints> select_capture_endpoints(std::span<const BulkEndpoint> endpoints) noexcept {
    if (endpoints.size() != 2) return std::nullopt;
    CaptureEndpoints selected{};
    for (const auto& pipe : endpoints) {
        if (!pipe.bulk || pipe.maximum_packet_bytes == 0 || (pipe.address & 0x0fU) == 0
            || (pipe.address & 0x70U) != 0) return std::nullopt;
        auto& endpoint = (pipe.address & 0x80U) ? selected.input : selected.output;
        if (endpoint != 0) return std::nullopt;
        endpoint = pipe.address;
    }
    return selected;
}
} // namespace unkxwyno::usb
