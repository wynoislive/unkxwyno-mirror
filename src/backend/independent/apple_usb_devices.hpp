#pragma once
#include "usb_capture_protocol.hpp"
#include <string>
#include <vector>

namespace unkxwyno::usb {
// Enumerate registered WinUSB interfaces belonging to currently present Apple devices.
[[nodiscard]] UXM_API std::vector<std::wstring> apple_usb_interfaces();
// Sends the public QuickTime capture-mode vendor request over the existing driver.
// No driver installation, registry changes, or pairing credentials are involved.
UXM_API void enable_usb_capture_mode(const std::wstring& path);
}
