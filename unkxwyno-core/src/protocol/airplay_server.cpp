/**
 * @file airplay_server.cpp
 * @brief Implementation of Zero-Dependency AirPlay RTSP/RTP receiver with full RFC-compliant mDNS.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#include "airplay_server.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <bcrypt.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <chrono>
#include <vector>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "bcrypt.lib")

namespace unkxwyno::protocol {

// Hardware-accelerated / Standard CNG AES-128-CTR Decryption implementation (Zero OpenSSL dependency)
static void Aes128CtrCrypt(const uint8_t key[16], uint8_t iv[16], uint8_t* data, size_t length) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0) == 0) {
        BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_ECB, sizeof(BCRYPT_CHAIN_MODE_ECB), 0);
        BCRYPT_KEY_HANDLE hKey = nullptr;
        if (BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, (PUCHAR)key, 16, 0) == 0) {
            uint8_t counter[16];
            std::memcpy(counter, iv, 16);
            uint8_t encrypted_counter[16];
            ULONG result = 0;

            size_t offset = 0;
            while (offset < length) {
                BCryptEncrypt(hKey, counter, 16, nullptr, nullptr, 0, encrypted_counter, 16, &result, 0);
                size_t block_bytes = std::min(static_cast<size_t>(16), length - offset);
                for (size_t b = 0; b < block_bytes; ++b) {
                    data[offset + b] ^= encrypted_counter[b];
                }
                offset += block_bytes;
                // Increment 128-bit big-endian counter
                for (int c = 15; c >= 0; --c) {
                    if (++counter[c] != 0) break;
                }
            }
            BCryptDestroyKey(hKey);
        }
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }
}

// Helper to get local LAN IPv4 address as uint32_t network byte order
static uint32_t GetLocalIpv4Address() {
    ULONG out_buf_len = 15000;
    std::vector<uint8_t> buffer(out_buf_len);
    auto* addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, addresses, &out_buf_len) == NO_ERROR) {
        for (auto* curr = addresses; curr != nullptr; curr = curr->Next) {
            if (curr->OperStatus != IfOperStatusUp) continue;
            if (curr->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

            for (auto* unicast = curr->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next) {
                if (unicast->Address.lpSockaddr->sa_family == AF_INET) {
                    auto* sin = reinterpret_cast<sockaddr_in*>(unicast->Address.lpSockaddr);
                    uint32_t ip = sin->sin_addr.s_addr;
                    uint8_t b1 = static_cast<uint8_t>(ip & 0xFF);
                    // Skip APIPA 169.254 and Radmin VPN 26.x
                    if (b1 != 169 && b1 != 26 && b1 != 127) {
                        return ip;
                    }
                }
            }
        }
    }
    return inet_addr("192.168.1.4");
}

AirPlayServer::AirPlayServer() {
    WSADATA wsa_data{};
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
}

AirPlayServer::~AirPlayServer() {
    Stop();
    WSACleanup();
}

bool AirPlayServer::Start(uint16_t rtsp_port) {
    if (is_running_) return true;
    is_running_ = true;

    // 1. Start raw UDP multicast mDNS beacon (224.0.0.251:5353)
    mdns_thread_ = std::thread(&AirPlayServer::MdnsThreadProc, this);

    // 2. Start usbmuxd background listener for wired Lightning / USB-C
    usbmuxd_thread_ = std::thread(&AirPlayServer::UsbmuxdThreadProc, this);

    // 3. Start RTSP server on port 7000
    rtsp_thread_ = std::thread(&AirPlayServer::RtspServerThreadProc, this, rtsp_port);

    // 4. Start RTP video receiver on port 7010
    rtp_thread_ = std::thread(&AirPlayServer::RtpVideoThreadProc, this, static_cast<uint16_t>(rtsp_port + 10));

    std::cout << "[AirPlay Server] Zero-dependency AirPlay engine active on port " << rtsp_port << "\n";
    return true;
}

void AirPlayServer::Stop() {
    if (!is_running_.exchange(false)) return;

    if (mdns_thread_.joinable()) mdns_thread_.join();
    if (usbmuxd_thread_.joinable()) usbmuxd_thread_.join();
    if (rtsp_thread_.joinable()) rtsp_thread_.join();
    if (rtp_thread_.joinable()) rtp_thread_.join();

    std::cout << "[AirPlay Server] Stopped.\n";
}

// Builds a complete RFC-compliant mDNS Response advertising _airplay._tcp and _raop._tcp
static std::vector<uint8_t> BuildMdnsAnnouncement(uint32_t local_ip_net) {
    std::vector<uint8_t> pkt;

    // DNS Header: ID=0, Flags=0x8400 (Response, Authoritative), Questions=0, Answers=4, Auth=0, Add=1
    const uint8_t hdr[] = {
        0x00, 0x00, // ID
        0x84, 0x00, // Flags (Response + Authoritative)
        0x00, 0x00, // Questions: 0
        0x00, 0x04, // Answer RRs: 4
        0x00, 0x00, // Authority RRs: 0
        0x00, 0x01  // Additional RRs: 1 (A Record)
    };
    pkt.insert(pkt.end(), hdr, hdr + sizeof(hdr));

    // Helper lambda to append DNS name labels
    auto append_label = [&](const std::string& name) {
        size_t start = 0;
        while (start < name.size()) {
            size_t dot = name.find('.', start);
            if (dot == std::string::npos) dot = name.size();
            uint8_t len = static_cast<uint8_t>(dot - start);
            pkt.push_back(len);
            for (size_t i = start; i < dot; ++i) pkt.push_back(static_cast<uint8_t>(name[i]));
            start = dot + 1;
        }
        pkt.push_back(0x00);
    };

    // 1. PTR Record: _airplay._tcp.local -> unkXwyno Mirror._airplay._tcp.local
    append_label("_airplay._tcp.local");
    pkt.push_back(0x00); pkt.push_back(0x0C); // Type: PTR (12)
    pkt.push_back(0x00); pkt.push_back(0x01); // Class: IN (1)
    pkt.push_back(0x00); pkt.push_back(0x00); pkt.push_back(0x11); pkt.push_back(0x94); // TTL: 4500s

    // Target: "unkXwyno Mirror._airplay._tcp.local"
    std::vector<uint8_t> ptr_target;
    {
        std::string sname = "unkXwyno Mirror._airplay._tcp.local";
        size_t st = 0;
        while (st < sname.size()) {
            size_t dot = sname.find('.', st);
            if (dot == std::string::npos) dot = sname.size();
            ptr_target.push_back(static_cast<uint8_t>(dot - st));
            for (size_t i = st; i < dot; ++i) ptr_target.push_back(static_cast<uint8_t>(sname[i]));
            st = dot + 1;
        }
        ptr_target.push_back(0x00);
    }
    pkt.push_back(static_cast<uint8_t>(ptr_target.size() >> 8));
    pkt.push_back(static_cast<uint8_t>(ptr_target.size() & 0xFF));
    pkt.insert(pkt.end(), ptr_target.begin(), ptr_target.end());

    // 2. SRV Record: unkXwyno Mirror._airplay._tcp.local -> port 7000, target unkxwyno.local
    append_label("unkXwyno Mirror._airplay._tcp.local");
    pkt.push_back(0x00); pkt.push_back(0x21); // Type: SRV (33)
    pkt.push_back(0x80); pkt.push_back(0x01); // Class: IN + Cache-Flush
    pkt.push_back(0x00); pkt.push_back(0x00); pkt.push_back(0x00); pkt.push_back(0x78); // TTL: 120s

    std::vector<uint8_t> srv_data;
    srv_data.push_back(0x00); srv_data.push_back(0x00); // Priority: 0
    srv_data.push_back(0x00); srv_data.push_back(0x00); // Weight: 0
    srv_data.push_back(0x1B); srv_data.push_back(0x58); // Port: 7000 (0x1B58)
    // Target host: "unkxwyno.local"
    {
        std::string host = "unkxwyno.local";
        size_t st = 0;
        while (st < host.size()) {
            size_t dot = host.find('.', st);
            if (dot == std::string::npos) dot = host.size();
            srv_data.push_back(static_cast<uint8_t>(dot - st));
            for (size_t i = st; i < dot; ++i) srv_data.push_back(static_cast<uint8_t>(host[i]));
            st = dot + 1;
        }
        srv_data.push_back(0x00);
    }
    pkt.push_back(static_cast<uint8_t>(srv_data.size() >> 8));
    pkt.push_back(static_cast<uint8_t>(srv_data.size() & 0xFF));
    pkt.insert(pkt.end(), srv_data.begin(), srv_data.end());

    // 3. TXT Record: unkXwyno Mirror._airplay._tcp.local
    append_label("unkXwyno Mirror._airplay._tcp.local");
    pkt.push_back(0x00); pkt.push_back(0x10); // Type: TXT (16)
    pkt.push_back(0x80); pkt.push_back(0x01); // Class: IN + Cache-Flush
    pkt.push_back(0x00); pkt.push_back(0x00); pkt.push_back(0x11); pkt.push_back(0x94); // TTL: 4500s

    std::vector<std::string> txt_strings = {
        "deviceid=48:A6:B8:22:33:44",
        "features=0x5A7FFFF7,0x1E",
        "flags=0x4",
        "model=AppleTV3,2",
        "pk=b07727d6f6cd5e08b58ede525ec3cdeaa252ad9f683feb212ef8a205246554e7",
        "srcvers=220.68",
        "vv=2"
    };
    std::vector<uint8_t> txt_data;
    for (const auto& str : txt_strings) {
        txt_data.push_back(static_cast<uint8_t>(str.size()));
        txt_data.insert(txt_data.end(), str.begin(), str.end());
    }
    pkt.push_back(static_cast<uint8_t>(txt_data.size() >> 8));
    pkt.push_back(static_cast<uint8_t>(txt_data.size() & 0xFF));
    pkt.insert(pkt.end(), txt_data.begin(), txt_data.end());

    // 4. PTR Record: _raop._tcp.local -> 48A6B8223344@unkXwyno Mirror._raop._tcp.local
    append_label("_raop._tcp.local");
    pkt.push_back(0x00); pkt.push_back(0x0C); // Type: PTR
    pkt.push_back(0x00); pkt.push_back(0x01); // Class: IN
    pkt.push_back(0x00); pkt.push_back(0x00); pkt.push_back(0x11); pkt.push_back(0x94); // TTL: 4500s

    std::vector<uint8_t> raop_target;
    {
        std::string rname = "48A6B8223344@unkXwyno Mirror._raop._tcp.local";
        size_t st = 0;
        while (st < rname.size()) {
            size_t dot = rname.find('.', st);
            if (dot == std::string::npos) dot = rname.size();
            raop_target.push_back(static_cast<uint8_t>(dot - st));
            for (size_t i = st; i < dot; ++i) raop_target.push_back(static_cast<uint8_t>(rname[i]));
            st = dot + 1;
        }
        raop_target.push_back(0x00);
    }
    pkt.push_back(static_cast<uint8_t>(raop_target.size() >> 8));
    pkt.push_back(static_cast<uint8_t>(raop_target.size() & 0xFF));
    pkt.insert(pkt.end(), raop_target.begin(), raop_target.end());

    // 5. Additional Record: unkxwyno.local A Record (IPv4)
    append_label("unkxwyno.local");
    pkt.push_back(0x00); pkt.push_back(0x01); // Type: A (1)
    pkt.push_back(0x80); pkt.push_back(0x01); // Class: IN + Cache-Flush
    pkt.push_back(0x00); pkt.push_back(0x00); pkt.push_back(0x00); pkt.push_back(0x78); // TTL: 120s
    pkt.push_back(0x00); pkt.push_back(0x04); // Data length: 4 bytes
    pkt.push_back(static_cast<uint8_t>(local_ip_net & 0xFF));
    pkt.push_back(static_cast<uint8_t>((local_ip_net >> 8) & 0xFF));
    pkt.push_back(static_cast<uint8_t>((local_ip_net >> 16) & 0xFF));
    pkt.push_back(static_cast<uint8_t>((local_ip_net >> 24) & 0xFF));

    return pkt;
}

void AirPlayServer::MdnsThreadProc() {
    uint32_t local_ip = GetLocalIpv4Address();
    std::vector<uint8_t> announcement = BuildMdnsAnnouncement(local_ip);

    // Multicast Destination 224.0.0.251:5353
    sockaddr_in mcast_dst{};
    mcast_dst.sin_family = AF_INET;
    inet_pton(AF_INET, "224.0.0.251", &mcast_dst.sin_addr);
    mcast_dst.sin_port = htons(5353);

    // Broadcaster Socket (sends announcements even if port 5353 is already bound by Windows svchost/Bonjour)
    SOCKET bcast_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (bcast_sock != INVALID_SOCKET) {
        unsigned char ttl = 255;
        setsockopt(bcast_sock, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char*>(&ttl), sizeof(ttl));

        // Explicitly bind multicast traffic to the physical LAN interface IP (e.g. 192.168.1.4)
        // so packets reach the local Wi-Fi router rather than virtual VPN/WSL adapters!
        in_addr if_addr{};
        if_addr.s_addr = local_ip;
        setsockopt(bcast_sock, IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char*>(&if_addr), sizeof(if_addr));

        unsigned char loop = 1;
        setsockopt(bcast_sock, IPPROTO_IP, IP_MULTICAST_LOOP, reinterpret_cast<const char*>(&loop), sizeof(loop));
    }

    // Listener Socket (to answer active queries from iOS Control Center)
    SOCKET listen_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (listen_sock != INVALID_SOCKET) {
        int reuse = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        sockaddr_in bind_addr{};
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        bind_addr.sin_port = htons(5353);

        if (bind(listen_sock, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) == 0) {
            ip_mreq mreq{};
            inet_pton(AF_INET, "224.0.0.251", &mreq.imr_multiaddr);
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);
            setsockopt(listen_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq));
        } else {
            closesocket(listen_sock);
            listen_sock = INVALID_SOCKET;
        }
    }

    std::cout << "[AirPlay mDNS] Multicast broadcaster active for 'unkXwyno Mirror' (224.0.0.251:5353)\n";

    std::vector<uint8_t> in_buf(2048);

    while (is_running_) {
        // Send unsolicited announcement
        if (bcast_sock != INVALID_SOCKET) {
            sendto(bcast_sock, reinterpret_cast<const char*>(announcement.data()),
                   static_cast<int>(announcement.size()), 0,
                   reinterpret_cast<sockaddr*>(&mcast_dst), sizeof(mcast_dst));
        }

        // Listen for queries
        for (int i = 0; i < 15 && is_running_; ++i) {
            if (listen_sock != INVALID_SOCKET) {
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(listen_sock, &fds);
                timeval tv{ 0, 100000 }; // 100ms
                if (select(0, &fds, nullptr, nullptr, &tv) > 0) {
                    sockaddr_in src_addr{};
                    int slen = sizeof(src_addr);
                    int n = recvfrom(listen_sock, reinterpret_cast<char*>(in_buf.data()), static_cast<int>(in_buf.size()), 0,
                                     reinterpret_cast<sockaddr*>(&src_addr), &slen);
                    if (n > 12) {
                        std::string q(reinterpret_cast<char*>(in_buf.data()), n);
                        if (q.find("_airplay") != std::string::npos || q.find("_raop") != std::string::npos) {
                            sendto(listen_sock, reinterpret_cast<const char*>(announcement.data()),
                                   static_cast<int>(announcement.size()), 0,
                                   reinterpret_cast<sockaddr*>(&src_addr), sizeof(src_addr));
                        }
                    }
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    if (bcast_sock != INVALID_SOCKET) closesocket(bcast_sock);
    if (listen_sock != INVALID_SOCKET) closesocket(listen_sock);
}

void AirPlayServer::UsbmuxdThreadProc() {
    // Polls local usbmuxd port 27015 to detect Apple iPhone/iPad (VID 0x05AC)
    while (is_running_) {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock != INVALID_SOCKET) {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
            addr.sin_port = htons(27015);

            if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
                std::string listen_plist = 
                    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                    "<plist version=\"1.0\"><dict>"
                    "<key>MessageType</key><string>Listen</string>"
                    "<key>ClientVersionString</key><string>unkXwyno 1.0</string>"
                    "<key>ProgName</key><string>unkxwyno-core</string>"
                    "</dict></plist>";

                uint32_t len = static_cast<uint32_t>(16 + listen_plist.size());
                uint32_t hdr[4] = { len, 1, 8, 1 };
                send(sock, reinterpret_cast<char*>(hdr), 16, 0);
                send(sock, listen_plist.c_str(), static_cast<int>(listen_plist.size()), 0);

                char buf[2048];
                int r = recv(sock, buf, sizeof(buf), 0);
                if (r > 0 && std::string(buf, r).find("Attached") != std::string::npos) {
                    is_connected_ = true;
                    std::cout << "[AirPlay usbmuxd] Apple iOS device connected over USB.\n";
                }
            }
            closesocket(sock);
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void AirPlayServer::RtspServerThreadProc(uint16_t port) {
    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_sock == INVALID_SOCKET) return;

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(server_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closesocket(server_sock);
        return;
    }

    listen(server_sock, 8);

    while (is_running_) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_sock, &read_fds);

        timeval tv{ 1, 0 };
        int sel = select(0, &read_fds, nullptr, nullptr, &tv);
        if (sel > 0 && FD_ISSET(server_sock, &read_fds)) {
            SOCKET client_sock = accept(server_sock, nullptr, nullptr);
            if (client_sock != INVALID_SOCKET) {
                is_connected_ = true;
                std::cout << "[AirPlay RTSP] iOS client session connected.\n";

                char req_buf[8192];
                while (is_running_) {
                    int n = recv(client_sock, req_buf, sizeof(req_buf) - 1, 0);
                    if (n <= 0) break;
                    req_buf[n] = '\0';
                    std::string req(req_buf, n);

                    // Extract CSeq safely
                    std::string cseq = "1";
                    size_t cseq_pos = req.find("CSeq: ");
                    if (cseq_pos != std::string::npos) {
                        size_t end_line = req.find("\r\n", cseq_pos);
                        if (end_line != std::string::npos && end_line > cseq_pos + 6) {
                            cseq = req.substr(cseq_pos + 6, end_line - (cseq_pos + 6));
                        }
                    }

                    std::ostringstream resp;
                    if (req.find("GET /info") != std::string::npos) {
                        // Standard Apple TV info plist response required by iOS Control Center
                        std::string info_plist =
                            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                            "<plist version=\"1.0\"><dict>\n"
                            "<key>features</key><integer>1518338039</integer>\n"
                            "<key>model</key><string>AppleTV3,2</string>\n"
                            "<key>name</key><string>unkXwyno Mirror</string>\n"
                            "<key>pi</key><string>2e3880b6-9bb3-4377-846a-7f093e08cb77</string>\n"
                            "<key>srcvers</key><string>220.68</string>\n"
                            "<key>vv</key><integer>2</integer>\n"
                            "</dict></plist>";

                        resp << "HTTP/1.1 200 OK\r\n"
                             << "Server: AirTunes/220.68\r\n"
                             << "Content-Type: text/x-apple-plist+xml\r\n"
                             << "Content-Length: " << info_plist.size() << "\r\n\r\n"
                             << info_plist;
                    } else if (req.rfind("OPTIONS", 0) == 0) {
                        resp << "RTSP/1.0 200 OK\r\n"
                             << "CSeq: " << cseq << "\r\n"
                             << "Server: AirTunes/220.68\r\n"
                             << "Public: ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, TEARDOWN, OPTIONS, GET_PARAMETER, SET_PARAMETER\r\n\r\n";
                    } else if (req.rfind("SETUP", 0) == 0) {
                        resp << "RTSP/1.0 200 OK\r\n"
                             << "CSeq: " << cseq << "\r\n"
                             << "Server: AirTunes/220.68\r\n"
                             << "Session: 1337\r\n"
                             << "Transport: RTP/AVP/TCP;unicast;interleaved=0-1;server_port=7010;client_port=7010\r\n\r\n";
                    } else if (req.rfind("RECORD", 0) == 0) {
                        resp << "RTSP/1.0 200 OK\r\n"
                             << "CSeq: " << cseq << "\r\n"
                             << "Server: AirTunes/220.68\r\n"
                             << "Session: 1337\r\n\r\n";
                    } else if (req.rfind("TEARDOWN", 0) == 0) {
                        resp << "RTSP/1.0 200 OK\r\n"
                             << "CSeq: " << cseq << "\r\n"
                             << "Server: AirTunes/220.68\r\n\r\n";
                        send(client_sock, resp.str().c_str(), static_cast<int>(resp.str().size()), 0);
                        break;
                    } else if (req.find("pair-setup") != std::string::npos || req.find("pair-verify") != std::string::npos) {
                        // Return 96 dummy verification bytes for pair-verify stage 1
                        std::vector<uint8_t> pair_data(96, 0x01);
                        resp << "HTTP/1.1 200 OK\r\n"
                             << "Server: AirTunes/220.68\r\n"
                             << "Content-Type: application/octet-stream\r\n"
                             << "Content-Length: " << pair_data.size() << "\r\n\r\n";
                        std::string head = resp.str();
                        send(client_sock, head.c_str(), static_cast<int>(head.size()), 0);
                        send(client_sock, reinterpret_cast<const char*>(pair_data.data()), static_cast<int>(pair_data.size()), 0);
                        continue;
                    } else {
                        resp << "RTSP/1.0 200 OK\r\nCSeq: " << cseq << "\r\nServer: AirTunes/220.68\r\n\r\n";
                    }

                    send(client_sock, resp.str().c_str(), static_cast<int>(resp.str().size()), 0);
                }
                closesocket(client_sock);
                is_connected_ = false;
            }
        }
    }

    closesocket(server_sock);
}

void AirPlayServer::RtpVideoThreadProc(uint16_t port) {
    SOCKET rtp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (rtp_sock == INVALID_SOCKET) return;

    int opt = 1;
    setsockopt(rtp_sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(rtp_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closesocket(rtp_sock);
        return;
    }

    std::vector<uint8_t> buffer(65536);

    while (is_running_) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(rtp_sock, &read_fds);

        timeval tv{ 1, 0 };
        int sel = select(0, &read_fds, nullptr, nullptr, &tv);
        if (sel > 0 && FD_ISSET(rtp_sock, &read_fds)) {
            int len = recv(rtp_sock, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
            if (len > 12) {
                size_t nalu_offset = 12;
                size_t nalu_len = len - nalu_offset;

                if (is_encrypted_) {
                    Aes128CtrCrypt(session_aes_key_, session_aes_iv_, buffer.data() + nalu_offset, nalu_len);
                }

                if (video_callback_) {
                    uint64_t pts = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
                    video_callback_(std::span<const uint8_t>(buffer.data() + nalu_offset, nalu_len), pts);
                }
            }
        }
    }

    closesocket(rtp_sock);
}

} // namespace unkxwyno::protocol

#endif // _WIN32
