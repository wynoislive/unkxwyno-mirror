/**
 * @file tcp_server.hpp
 * @brief Zero-Dependency TCP & UDP Discovery Server for Android Wireless Mirroring.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <span>
#include <mutex>
#include <cstdint>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace unkxwyno::network {

using DataReceivedCallback = std::function<void(std::span<const uint8_t> data)>;
using ClientStateCallback = std::function<void(bool connected, const std::string& remote_ip)>;

class TcpServer {
public:
    TcpServer();
    ~TcpServer();

    bool Start(uint16_t tcp_port = 8099, uint16_t udp_discovery_port = 8098);
    void Stop();

    void SetDataCallback(DataReceivedCallback callback) {
        data_callback_ = std::move(callback);
    }

    void SetClientStateCallback(ClientStateCallback callback) {
        client_state_callback_ = std::move(callback);
    }

    bool SendPacket(std::span<const uint8_t> packet);
    [[nodiscard]] bool IsClientConnected() const { return is_client_connected_.load(); }
    [[nodiscard]] std::string GetConnectedClientIp() const { return client_ip_; }

private:
    void TcpListenThreadProc(uint16_t port);
    void UdpDiscoveryThreadProc(uint16_t port);

    std::atomic<bool> is_running_{false};
    std::atomic<bool> is_client_connected_{false};
    std::string client_ip_;

    std::thread tcp_thread_;
    std::thread udp_thread_;

    DataReceivedCallback data_callback_;
    ClientStateCallback client_state_callback_;

    SOCKET listen_socket_{INVALID_SOCKET};
    SOCKET client_socket_{INVALID_SOCKET};
    SOCKET udp_socket_{INVALID_SOCKET};
    std::mutex socket_mutex_;
};

} // namespace unkxwyno::network
