#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace unkxwyno::usb {

// Convert a tightly described 8-bit NV12 surface to top-down BGRA using the
// same limited-range BT.601 equations on every CPU architecture.
void nv12_to_bgra(std::span<const std::uint8_t> nv12, unsigned width,
                  unsigned height, unsigned stride,
                  std::span<std::uint8_t> bgra);
std::string_view nv12_converter_name() noexcept;

} // namespace unkxwyno::usb
