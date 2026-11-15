/**
 * @file aoa2_client.cpp
 * @brief Implementation of Android Open Accessory 2.0 Client.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#include "aoa2_client.hpp"
#include <iostream>
#include <cstring>

namespace unkxwyno::protocol {

// AOA 2.0 Standard Control Request Codes
constexpr uint8_t AOA_GET_PROTOCOL    = 51;
constexpr uint8_t AOA_SEND_STRING     = 52;
constexpr uint8_t AOA_START_ACCESSORY = 53;
constexpr uint8_t AOA_REGISTER_HID    = 54;
constexpr uint8_t AOA_UNREGISTER_HID  = 55;
constexpr uint8_t AOA_SET_HID_REPORT  = 56;

// USB Request Types
constexpr uint8_t USB_REQ_IN_VENDOR  = 0xC0;
constexpr uint8_t USB_REQ_OUT_VENDOR = 0x40;

// Standard USB Multi-touch Digitizer HID Report Descriptor (Usage Page 0x0D, Usage 0x04)
static const uint8_t kDigitizerHidDescriptor[] = {
    0x05, 0x0D,        // Usage Page (Digitizer)
    0x09, 0x04,        // Usage (Touch Screen)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x09, 0x22,        //   Usage (Finger)
    0xA1, 0x00,        //   Collection (Physical)
    0x09, 0x42,        //     Usage (Tip Switch)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x02,        //     Input (Data, Var, Abs)
    0x09, 0x32,        //     Usage (In Range)
    0x81, 0x02,        //     Input (Data, Var, Abs)
    0x95, 0x06,        //     Report Count (6) - Padding
    0x81, 0x03,        //     Input (Const, Var, Abs)
    0x09, 0x51,        //     Usage (Contact Identifier)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x25, 0x0A,        //     Logical Maximum (10)
    0x81, 0x02,        //     Input (Data, Var, Abs)
    0x05, 0x01,        //     Usage Page (Generic Desktop)
    0x09, 0x30,        //     Usage (X)
    0x26, 0xFF, 0x7F,  //     Logical Maximum (32767)
    0x75, 0x10,        //     Report Size (16)
    0x81, 0x02,        //     Input (Data, Var, Abs)
    0x09, 0x31,        //     Usage (Y)
    0x26, 0xFF, 0x7F,  //     Logical Maximum (32767)
    0x81, 0x02,        //     Input (Data, Var, Abs)
    0xC0,              //   End Collection
    0xC0               // End Collection
};

Aoa2Client::Aoa2Client(std::shared_ptr<usb::IUsbDeviceSession> session)
    : session_(std::move(session)) {}

Aoa2Client::~Aoa2Client() = default;

bool Aoa2Client::PerformHandshake() {
    if (!session_ || !session_->IsOpen()) {
        return false;
    }

    // Step 1: Query AOA Protocol Version
    uint16_t protocol_version = 0;
    usb::UsbControlSetup get_proto_setup{
        .request_type = USB_REQ_IN_VENDOR,
        .request      = AOA_GET_PROTOCOL,
        .value        = 0,
        .index        = 0,
        .length       = sizeof(protocol_version)
    };

    auto status = session_->ControlTransfer(get_proto_setup,
                                           reinterpret_cast<uint8_t*>(&protocol_version),
                                           sizeof(protocol_version));

    if (status != usb::UsbStatus::Success || protocol_version < 2) {
        std::cerr << "[AOA2] Device does not support AOA 2.0 (protocol=" << protocol_version << ")\n";
        return false;
    }

    std::cout << "[AOA2] Device supports AOA protocol version: " << protocol_version << "\n";

    // Step 2: Transmit unkXwyno Brand Identity Strings
    if (!SendAoaString(0, UNKXWYNO_AOA_MANUFACTURER)) return false;
    if (!SendAoaString(1, UNKXWYNO_AOA_MODEL))        return false;
    if (!SendAoaString(2, UNKXWYNO_AOA_DESCRIPTION))  return false;
    if (!SendAoaString(3, UNKXWYNO_AOA_VERSION))      return false;
    if (!SendAoaString(4, UNKXWYNO_AOA_URI))          return false;
    if (!SendAoaString(5, UNKXWYNO_AOA_SERIAL))       return false;

    // Step 3: Trigger Accessory Mode Switch
    usb::UsbControlSetup start_setup{
        .request_type = USB_REQ_OUT_VENDOR,
        .request      = AOA_START_ACCESSORY,
        .value        = 0,
        .index        = 0,
        .length       = 0
    };

    status = session_->ControlTransfer(start_setup, nullptr, 0);
    if (status != usb::UsbStatus::Success) {
        std::cerr << "[AOA2] Failed to execute START_ACCESSORY\n";
        return false;
    }

    is_accessory_mode_ = true;
    std::cout << "[AOA2] START_ACCESSORY signaled. Device switching to unkXwyno mode.\n";
    return true;
}

bool Aoa2Client::SendAoaString(uint16_t index, const std::string& str) {
    // Null-terminated string buffer
    std::vector<uint8_t> buffer(str.begin(), str.end());
    buffer.push_back(0x00);

    usb::UsbControlSetup str_setup{
        .request_type = USB_REQ_OUT_VENDOR,
        .request      = AOA_SEND_STRING,
        .value        = 0,
        .index        = index,
        .length       = static_cast<uint16_t>(buffer.size())
    };

    auto status = session_->ControlTransfer(str_setup, buffer.data(), static_cast<uint16_t>(buffer.size()));
    return status == usb::UsbStatus::Success;
}

bool Aoa2Client::RegisterHidTouchscreen(uint16_t touch_hid_id) {
    if (!session_ || !session_->IsOpen()) return false;

    // 1. Register HID Device
    usb::UsbControlSetup reg_setup{
        .request_type = USB_REQ_OUT_VENDOR,
        .request      = AOA_REGISTER_HID,
        .value        = touch_hid_id,
        .index        = static_cast<uint16_t>(sizeof(kDigitizerHidDescriptor)),
        .length       = 0
    };

    auto status = session_->ControlTransfer(reg_setup, nullptr, 0);
    if (status != usb::UsbStatus::Success) return false;

    // 2. Set HID Report Descriptor
    usb::UsbControlSetup desc_setup{
        .request_type = USB_REQ_OUT_VENDOR,
        .request      = AOA_SET_HID_REPORT,
        .value        = touch_hid_id,
        .index        = 0,
        .length       = static_cast<uint16_t>(sizeof(kDigitizerHidDescriptor))
    };

    status = session_->ControlTransfer(desc_setup,
                                      const_cast<uint8_t*>(kDigitizerHidDescriptor),
                                      sizeof(kDigitizerHidDescriptor));

    if (status == usb::UsbStatus::Success) {
        hid_registered_ = true;
        std::cout << "[AOA2] Registered AOAv2 HID Multi-touch Digitizer on Android kernel (ID=" << touch_hid_id << ")\n";
        return true;
    }
    return false;
}

bool Aoa2Client::SendTouchReport(uint16_t touch_hid_id, const unkxwyno_touch_event_t& touch_event) {
    if (!session_ || !hid_registered_) return false;

    // Pack HID Report matching kDigitizerHidDescriptor:
    // Byte 0: Report ID (1)
    // Byte 1: Bits [Tip Switch (1), In Range (1), Padding (6)]
    // Byte 2: Contact ID (0..9)
    // Byte 3..4: X Coordinate (uint16_t LE)
    // Byte 5..6: Y Coordinate (uint16_t LE)
    uint8_t hid_report[7];
    hid_report[0] = 0x01; // Report ID
    
    uint8_t tip_switch = (touch_event.action != UNKXWYNO_TOUCH_UP && touch_event.action != UNKXWYNO_TOUCH_CANCEL) ? 0x03 : 0x00;
    hid_report[1] = tip_switch; // Bit 0: Tip switch, Bit 1: In range
    hid_report[2] = touch_event.pointer_id & 0x0F;

    // Scale 0..65535 to 0..32767
    uint16_t scaled_x = touch_event.x >> 1;
    uint16_t scaled_y = touch_event.y >> 1;
    hid_report[3] = static_cast<uint8_t>(scaled_x & 0xFF);
    hid_report[4] = static_cast<uint8_t>((scaled_x >> 8) & 0xFF);
    hid_report[5] = static_cast<uint8_t>(scaled_y & 0xFF);
    hid_report[6] = static_cast<uint8_t>((scaled_y >> 8) & 0xFF);

    usb::UsbControlSetup report_setup{
        .request_type = USB_REQ_OUT_VENDOR,
        .request      = AOA_SET_HID_REPORT,
        .value        = touch_hid_id,
        .index        = 0,
        .length       = sizeof(hid_report)
    };

    auto status = session_->ControlTransfer(report_setup, hid_report, sizeof(hid_report), 100);
    return status == usb::UsbStatus::Success;
}

bool Aoa2Client::SendPacket(const unkxwyno_packet_header_t& header, std::span<const uint8_t> payload) {
    if (!session_) return false;

    std::vector<uint8_t> packet_buffer;
    packet_buffer.resize(sizeof(unkxwyno_packet_header_t) + payload.size());

    std::memcpy(packet_buffer.data(), &header, sizeof(unkxwyno_packet_header_t));
    if (!payload.empty()) {
        std::memcpy(packet_buffer.data() + sizeof(unkxwyno_packet_header_t), payload.data(), payload.size());
    }

    size_t written = 0;
    auto status = session_->BulkWrite(bulk_out_endpoint_, packet_buffer.data(), packet_buffer.size(), written, 1000);
    return status == usb::UsbStatus::Success && written == packet_buffer.size();
}

bool Aoa2Client::ReadStream(uint8_t* buffer, size_t max_len, size_t& bytes_read, uint32_t timeout_ms) {
    if (!session_) return false;
    auto status = session_->BulkRead(bulk_in_endpoint_, buffer, max_len, bytes_read, timeout_ms);
    return status == usb::UsbStatus::Success;
}

} // namespace unkxwyno::protocol
