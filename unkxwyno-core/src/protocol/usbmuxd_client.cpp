/**
 * @file usbmuxd_client.cpp
 * @brief Implementation of UsbmuxdClient for native iOS USB tunneling.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#include "usbmuxd_client.hpp"
#include <iostream>
#include <sstream>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

namespace unkxwyno::protocol {

UsbmuxdClient::UsbmuxdClient() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
}

UsbmuxdClient::~UsbmuxdClient() {
    Disconnect();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool UsbmuxdClient::Connect() {
    if (IsConnected()) return true;

#ifdef _WIN32
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(27015); // Standard usbmuxd port on Windows
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closesocket(s);
        return false;
    }

    socket_handle_ = static_cast<intptr_t>(s);
    return true;
#else
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0) return false;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, "/var/run/usbmuxd", sizeof(addr.sun_path) - 1);

    if (connect(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(s);
        return false;
    }

    socket_handle_ = static_cast<intptr_t>(s);
    return true;
#endif
}

void UsbmuxdClient::Disconnect() {
    if (IsConnected()) {
#ifdef _WIN32
        closesocket(static_cast<SOCKET>(socket_handle_));
#else
        close(static_cast<int>(socket_handle_));
#endif
        socket_handle_ = -1;
    }
}

bool UsbmuxdClient::IsConnected() const {
    return socket_handle_ != -1;
}

bool UsbmuxdClient::StartListening(IosDeviceAttachedCb on_attach, IosDeviceDetachedCb on_detach) {
    if (!Connect()) return false;

    // Send usbmuxd 'Listen' XML plist packet
    std::string plist_body = 
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "    <key>MessageType</key>\n"
        "    <string>Listen</string>\n"
        "    <key>ClientVersionString</key>\n"
        "    <string>unkXwyno-Mirror-1.0.0</string>\n"
        "    <key>ProgName</key>\n"
        "    <string>unkXwyno</string>\n"
        "</dict>\n"
        "</plist>";

    UsbmuxdHeader hdr{};
    hdr.length  = static_cast<uint32_t>(sizeof(UsbmuxdHeader) + plist_body.size());
    hdr.version = 1; // Plist version
    hdr.type    = 8; // Plist message type
    hdr.tag     = ++current_tag_;

    std::vector<uint8_t> packet(hdr.length);
    std::memcpy(packet.data(), &hdr, sizeof(UsbmuxdHeader));
    std::memcpy(packet.data() + sizeof(UsbmuxdHeader), plist_body.data(), plist_body.size());

#ifdef _WIN32
    int sent = send(static_cast<SOCKET>(socket_handle_), reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0);
    return sent == static_cast<int>(packet.size());
#else
    ssize_t sent = send(static_cast<int>(socket_handle_), packet.data(), packet.size(), 0);
    return sent == static_cast<ssize_t>(packet.size());
#endif
}

intptr_t UsbmuxdClient::ConnectToPort(uint32_t device_id, uint16_t target_port) {
    if (!Connect()) return -1;

    // Convert target port to network byte order
    uint16_t net_port = ((target_port & 0xFF) << 8) | ((target_port >> 8) & 0xFF);

    std::ostringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
       << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
       << "<plist version=\"1.0\">\n"
       << "<dict>\n"
       << "    <key>MessageType</key>\n"
       << "    <string>Connect</string>\n"
       << "    <key>DeviceID</key>\n"
       << "    <integer>" << device_id << "</integer>\n"
       << "    <key>PortNumber</key>\n"
       << "    <integer>" << net_port << "</integer>\n"
       << "    <key>ClientVersionString</key>\n"
       << "    <string>unkXwyno-Mirror-1.0.0</string>\n"
       << "</dict>\n"
       << "</plist>";

    std::string plist = ss.str();

    UsbmuxdHeader hdr{};
    hdr.length  = static_cast<uint32_t>(sizeof(UsbmuxdHeader) + plist.size());
    hdr.version = 1;
    hdr.type    = 8;
    hdr.tag     = ++current_tag_;

    std::vector<uint8_t> packet(hdr.length);
    std::memcpy(packet.data(), &hdr, sizeof(UsbmuxdHeader));
    std::memcpy(packet.data() + sizeof(UsbmuxdHeader), plist.data(), plist.size());

#ifdef _WIN32
    SOCKET s = static_cast<SOCKET>(socket_handle_);
    send(s, reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0);

    // Read result
    UsbmuxdHeader res_hdr{};
    int received = recv(s, reinterpret_cast<char*>(&res_hdr), sizeof(res_hdr), 0);
    if (received >= static_cast<int>(sizeof(res_hdr))) {
        if (res_hdr.length > sizeof(UsbmuxdHeader)) {
            size_t payload_len = res_hdr.length - sizeof(UsbmuxdHeader);
            std::vector<char> payload(payload_len + 1, 0);
            int pl_recvd = recv(s, payload.data(), static_cast<int>(payload_len), 0);
            if (pl_recvd > 0) {
                std::string resp(payload.data(), pl_recvd);
                if (resp.find("<integer>0</integer>") != std::string::npos) {
                    intptr_t tunnel = socket_handle_;
                    socket_handle_ = -1; // Detach socket so caller owns the tunnel
                    return tunnel;
                }
            }
        } else {
            intptr_t tunnel = socket_handle_;
            socket_handle_ = -1;
            return tunnel;
        }
    }
#endif

    return -1;
}

} // namespace unkxwyno::protocol
