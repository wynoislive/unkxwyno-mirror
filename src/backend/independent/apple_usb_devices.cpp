#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <winusb.h>
#include <objbase.h>
#include "apple_usb_devices.hpp"
#include <set>
#include <system_error>

namespace unkxwyno::usb {
namespace {
struct Devices {
    HDEVINFO value{INVALID_HANDLE_VALUE};
    ~Devices() { if (value != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(value); }
};
struct Key { HKEY value{}; ~Key() { if (value && value != INVALID_HANDLE_VALUE) RegCloseKey(value); } };
void add_guid_paths(const GUID& guid, std::set<std::wstring>& paths) {
    Devices list{SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE)};
    if (list.value == INVALID_HANDLE_VALUE) return;
    for (DWORD i = 0;; ++i) {
        SP_DEVICE_INTERFACE_DATA data{};
        data.cbSize = sizeof(data);
        if (!SetupDiEnumDeviceInterfaces(list.value, nullptr, &guid, i, &data)) break;
        DWORD length{};
        SetupDiGetDeviceInterfaceDetailW(list.value, &data, nullptr, 0, &length, nullptr);
        if (length < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || length > 65536) continue;
        std::vector<unsigned char> storage(length);
        auto detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(list.value, &data, detail, length, nullptr, nullptr)) continue;
        std::wstring path(detail->DevicePath);
        std::wstring lower = path;
        for (auto& ch : lower) ch = static_cast<wchar_t>(towlower(ch));
        if (lower.find(L"vid_05ac") != std::wstring::npos) paths.insert(std::move(path));
    }
}
}
std::vector<std::wstring> apple_usb_interfaces() {
    std::set<std::wstring> paths;
    // Apple's UMDF interface and the underlying WinUSB interface have different
    // GUIDs. The former can reject direct CreateFile calls even though PnP says
    // WINUSB. Enumerate the standard WinUSB GUID as well as vendor registrations.
    GUID winusb_guid{0xdee824ef, 0x729b, 0x4a0e, {0x9c, 0x14, 0xb7, 0x11, 0x7d, 0x33, 0xa8, 0x17}};
    add_guid_paths(winusb_guid, paths);
    Devices devices{SetupDiGetClassDevsW(nullptr, L"USB", nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES)};
    if (devices.value == INVALID_HANDLE_VALUE) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Enumerate USB devices");
    for (DWORD i = 0;; ++i) {
        SP_DEVINFO_DATA data{};
        data.cbSize = sizeof(data);
        if (!SetupDiEnumDeviceInfo(devices.value, i, &data)) break;
        wchar_t id[1024]{};
        if (!SetupDiGetDeviceInstanceIdW(devices.value, &data, id, 1024, nullptr)) continue;
        std::wstring instance(id);
        if (instance.find(L"VID_05AC") == std::wstring::npos) continue;
        Key key{SetupDiOpenDevRegKey(devices.value, &data, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ)};
        if (key.value == INVALID_HANDLE_VALUE) continue;
        DWORD type{}, bytes{};
        if (RegQueryValueExW(key.value, L"DeviceInterfaceGUIDs", nullptr, &type, nullptr, &bytes) != ERROR_SUCCESS || bytes > 65536) continue;
        if (type != REG_MULTI_SZ && type != REG_SZ) continue;
        std::vector<wchar_t> text(bytes / sizeof(wchar_t) + 2, L'\0');
        if (RegQueryValueExW(key.value, L"DeviceInterfaceGUIDs", nullptr, &type, reinterpret_cast<BYTE*>(text.data()), &bytes) != ERROR_SUCCESS) continue;
        for (const auto* p = text.data(); *p; p += wcslen(p) + 1) {
            GUID guid{};
            if (SUCCEEDED(CLSIDFromString(p, &guid))) add_guid_paths(guid, paths);
        }
    }
    return {paths.begin(), paths.end()};
}
void enable_usb_capture_mode(const std::wstring& path) {
    HANDLE device = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
    if (device == INVALID_HANDLE_VALUE) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Open Apple USB interface");
    WINUSB_INTERFACE_HANDLE usb{};
    if (!WinUsb_Initialize(device, &usb)) {
        const auto error = GetLastError(); CloseHandle(device);
        throw std::system_error(static_cast<int>(error), std::system_category(), "Initialize Apple WinUSB interface");
    }
    USB_DEVICE_DESCRIPTOR descriptor{};
    ULONG transferred{};
    const bool apple = WinUsb_GetDescriptor(usb, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0,
        reinterpret_cast<PUCHAR>(&descriptor), sizeof(descriptor), &transferred)
        && transferred == sizeof(descriptor) && descriptor.idVendor == 0x05ac;
    WINUSB_SETUP_PACKET setup{0x40, 0x52, 0, 2, 0};
    const bool result = apple && WinUsb_ControlTransfer(usb, setup, nullptr, 0, &transferred, nullptr);
    const auto error = GetLastError();
    WinUsb_Free(usb); CloseHandle(device);
    if (!apple) throw std::runtime_error("Capture activation requires an Apple USB device.");
    // Re-enumeration may interrupt the transfer; callers must rediscover rather than reuse this handle.
    if (!result && error != ERROR_DEVICE_NOT_CONNECTED && error != ERROR_GEN_FAILURE)
        throw std::system_error(static_cast<int>(error), std::system_category(), "Enable iPhone USB capture mode");
}
}
