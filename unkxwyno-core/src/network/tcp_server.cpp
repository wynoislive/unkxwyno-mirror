/**
 * @file tcp_server.cpp
 * @brief Zero-Dependency TCP & UDP Discovery Server implementation for Wireless Mirroring.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#include "tcp_server.hpp"
#include <iostream>
#include <cstring>

namespace unkxwyno::network {

TcpServer::TcpServer() = default;

TcpServer::~TcpServer() {
    Stop();
}

bool TcpServer::Start(uint16_t tcp_port, uint16_t udp_discovery_port) {
    if (is_running_) return true;

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return false;
    }
#endif

    is_running_ = true;
    tcp_thread_ = std::thread(&TcpServer::TcpListenThreadProc, this, tcp_port);
    udp_thread_ = std::thread(&TcpServer::UdpDiscoveryThreadProc, this, udp_discovery_port);

    return true;
}

void TcpServer::Stop() {
    if (!is_running_) return;
    is_running_ = false;

    {
        std::lock_guard<std::mutex> lock(socket_mutex_);
        if (client_socket_ != INVALID_SOCKET) {
            closesocket(client_socket_);
            client_socket_ = INVALID_SOCKET;
        }
        if (listen_socket_ != INVALID_SOCKET) {
            closesocket(listen_socket_);
            listen_socket_ = INVALID_SOCKET;
        }
        if (udp_socket_ != INVALID_SOCKET) {
            closesocket(udp_socket_);
            udp_socket_ = INVALID_SOCKET;
        }
    }

    if (tcp_thread_.joinable()) tcp_thread_.join();
    if (udp_thread_.joinable()) udp_thread_.join();

    is_client_connected_ = false;
    client_ip_.clear();

#ifdef _WIN32
    WSACleanup();
#endif
}

bool TcpServer::SendPacket(std::span<const uint8_t> packet) {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (client_socket_ == INVALID_SOCKET) return false;

    int total_sent = 0;
    int to_send = static_cast<int>(packet.size());
    const char* buf = reinterpret_cast<const char*>(packet.data());

    while (total_sent < to_send) {
        int sent = send(client_socket_, buf + total_sent, to_send - total_sent, 0);
        if (sent <= 0) {
            return false;
        }
        total_sent += sent;
    }
    return true;
}

void TcpServer::TcpListenThreadProc(uint16_t port) {
    listen_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket_ == INVALID_SOCKET) return;

    int opt = 1;
    setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(listen_socket_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(listen_socket_);
        listen_socket_ = INVALID_SOCKET;
        return;
    }

    if (listen(listen_socket_, 2) == SOCKET_ERROR) {
        closesocket(listen_socket_);
        listen_socket_ = INVALID_SOCKET;
        return;
    }

    std::vector<uint8_t> recv_buf(256 * 1024); // 256 KiB buffer

    while (is_running_) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(listen_socket_, &read_fds);

        timeval tv{1, 0}; // 1 sec timeout
        int sel = select(0, &read_fds, nullptr, nullptr, &tv);
        if (sel <= 0 || !is_running_) continue;

        sockaddr_in client_addr{};
        int client_addr_len = sizeof(client_addr);
        SOCKET accepted = accept(listen_socket_, reinterpret_cast<sockaddr*>(&client_addr), &client_addr_len);
        if (accepted == INVALID_SOCKET) continue;

        // Set TCP_NODELAY and large receive buffer for lowest streaming latency
        int nodelay = 1;
        setsockopt(accepted, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));
        int rcv_buf_size = 1024 * 1024; // 1 MB
        setsockopt(accepted, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcv_buf_size), sizeof(rcv_buf_size));

        char ip_str[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN);

        {
            std::lock_guard<std::mutex> lock(socket_mutex_);
            if (client_socket_ != INVALID_SOCKET) {
                closesocket(client_socket_);
            }
            client_socket_ = accepted;
            client_ip_ = ip_str;
            is_client_connected_ = true;
        }

        if (client_state_callback_) {
            client_state_callback_(true, client_ip_);
        }

        // Streaming read loop for this client
        while (is_running_) {
            int received = recv(accepted, reinterpret_cast<char*>(recv_buf.data()), static_cast<int>(recv_buf.size()), 0);
            if (received <= 0) {
                break; // Disconnected or error
            }
            if (data_callback_) {
                data_callback_(std::span<const uint8_t>(recv_buf.data(), static_cast<size_t>(received)));
            }
        }

        {
            std::lock_guard<std::mutex> lock(socket_mutex_);
            if (client_socket_ == accepted) {
                closesocket(client_socket_);
                client_socket_ = INVALID_SOCKET;
                is_client_connected_ = false;
            }
        }

        if (client_state_callback_) {
            client_state_callback_(false, ip_str);
        }
    }
}

void TcpServer::UdpDiscoveryThreadProc(uint16_t port) {
    udp_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket_ == INVALID_SOCKET) return;

    int opt = 1;
    setsockopt(udp_socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
    setsockopt(udp_socket_, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(udp_socket_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(udp_socket_);
        udp_socket_ = INVALID_SOCKET;
        return;
    }

    char buffer[512]{};

    while (is_running_) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(udp_socket_, &read_fds);

        timeval tv{1, 0};
        int sel = select(0, &read_fds, nullptr, nullptr, &tv);
        if (sel <= 0 || !is_running_) continue;

        sockaddr_in client_addr{};
        int client_len = sizeof(client_addr);
        int recvd = recvfrom(udp_socket_, buffer, sizeof(buffer) - 1, 0,
                             reinterpret_cast<sockaddr*>(&client_addr), &client_len);

        if (recvd > 0) {
            buffer[recvd] = '\0';
            if (std::strstr(buffer, "UNKXWYNO_DISCOVERY_PING") != nullptr) {
                const char* reply = "UNKXWYNO_DISCOVERY_PONG:8099";
                sendto(udp_socket_, reply, static_cast<int>(strlen(reply)), 0,
                       reinterpret_cast<sockaddr*>(&client_addr), client_len);
            }
        }
    }
}

} // namespace unkxwyno::network
