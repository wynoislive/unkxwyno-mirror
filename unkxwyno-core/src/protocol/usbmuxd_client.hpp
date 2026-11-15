/**
 * @file usbmuxd_client.hpp
 * @brief Native Apple usbmuxd socket client for zero-dependency iOS USB tunneling.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace unkxwyno::protocol {

struct IosDevice {
    uint32_t device_id{0};
    uint32_t product_id{0};
    std::string serial_number;
    std::string connection_type;
};

using IosDeviceAttachedCb = std::function<void(const IosDevice& device)>;
using IosDeviceDetachedCb = std::function<void(uint32_t device_id)>;

#pragma pack(push, 1)
struct UsbmuxdHeader {
    uint32_t length;     // Total packet length (header + payload)
    uint32_t version;    // 1 = Binary, 2 = XML/Plist
    uint32_t type;       // 1 = Result, 2 = Connect, 3 = Listen, 4 = DeviceAdd, 5 = DeviceRemove
    uint32_t tag;        // Sequence tag
};
#pragma pack(pop)

class UsbmuxdClient {
public:
    UsbmuxdClient();
    ~UsbmuxdClient();

    [[nodiscard]] bool Connect();
    void Disconnect();
    [[nodiscard]] bool IsConnected() const;

    /**
     * @brief Start listening for attached Apple iOS devices over USB.
     */
    bool StartListening(IosDeviceAttachedCb on_attach, IosDeviceDetachedCb on_detach);

    /**
     * @brief Tunnel a local socket connection directly to an iOS device TCP port.
     * @param device_id Target device identifier from DeviceAdd.
     * @param target_port Port to connect to (e.g. 5000 for AirPlay RTSP).
     * @return Raw socket handle or -1 on failure.
     */
    [[nodiscard]] intptr_t ConnectToPort(uint32_t device_id, uint16_t target_port);

private:
    intptr_t socket_handle_{-1};
    uint32_t current_tag_{1};
};

} // namespace unkxwyno::protocol
