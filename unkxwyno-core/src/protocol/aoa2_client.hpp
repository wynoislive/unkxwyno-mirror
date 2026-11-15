/**
 * @file aoa2_client.hpp
 * @brief Android Open Accessory 2.0 (AOA2) protocol state machine and HID injector.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <span>
#include "../usb/usb_transport.hpp"
#include "../../../unkxwyno-protocol/include/unkxwyno_protocol.h"

namespace unkxwyno::protocol {

class Aoa2Client {
public:
    explicit Aoa2Client(std::shared_ptr<usb::IUsbDeviceSession> session);
    ~Aoa2Client();

    /**
     * @brief Execute the full AOA 2.0 handshake sequence:
     * 1. Check protocol version (GET_PROTOCOL).
     * 2. Send unkXwyno brand identity strings.
     * 3. Switch device into accessory mode (START_ACCESSORY).
     */
    [[nodiscard]] bool PerformHandshake();

    /**
     * @brief Register the PC as an AOA 2.0 hardware HID multi-touch digitizer on Android.
     */
    [[nodiscard]] bool RegisterHidTouchscreen(uint16_t touch_hid_id = 1);

    /**
     * @brief Send real-time multi-touch report over the USB control pipe.
     */
    bool SendTouchReport(uint16_t touch_hid_id, const unkxwyno_touch_event_t& touch_event);

    /**
     * @brief Send raw bulk packet data to the Android companion app.
     */
    bool SendPacket(const unkxwyno_packet_header_t& header, std::span<const uint8_t> payload);

    /**
     * @brief Read raw bulk stream data from the Android companion app.
     */
    bool ReadStream(uint8_t* buffer, size_t max_len, size_t& bytes_read, uint32_t timeout_ms = 1000);

private:
    bool SendAoaString(uint16_t index, const std::string& str);

    std::shared_ptr<usb::IUsbDeviceSession> session_;
    uint8_t bulk_in_endpoint_{0x81};
    uint8_t bulk_out_endpoint_{0x01};
    bool is_accessory_mode_{false};
    bool hid_registered_{false};
};

} // namespace unkxwyno::protocol
