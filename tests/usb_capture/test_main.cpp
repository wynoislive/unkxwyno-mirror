#include "usb_capture_protocol.hpp"
#include "usb_capture_session.hpp"
#include "usb_mux_control.hpp"
#include "usb_video_converter.hpp"
#include <bit>
#include <chrono>
#include <fstream>
#ifdef _WIN32
#include "winusb_capture_transport.hpp"
#include "usb_mirror.h"
#endif
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace unkxwyno::usb;
namespace {
int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
void word(std::span<std::uint8_t> bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8U));
}
void video_conversion() {
    constexpr unsigned width = 4, height = 2, stride = 4;
    const std::array<std::uint8_t, 12> nv12{
        16,235,80,100, 16,235,80,100, 128,128,128,128};
    std::array<std::uint8_t, width * height * 4> bgra{};
    nv12_to_bgra(nv12, width, height, stride, bgra);
    constexpr std::array<std::uint8_t, 16> expected_row{
        0,0,0,255, 255,255,255,255, 75,75,75,255, 98,98,98,255};
    check(std::equal(expected_row.begin(), expected_row.end(), bgra.begin()),
          "SSE2 and scalar conversion must preserve neutral-range BT.601 pixels");
    check(std::equal(expected_row.begin(), expected_row.end(), bgra.begin() + 16),
          "NV12 chroma rows must be shared by the paired luma rows");

    constexpr unsigned padded_width = 16, padded_height = 8, padded_stride = 20;
    std::vector<std::uint8_t> padded_nv12(padded_stride * padded_height * 3 / 2);
    std::uint32_t seed = 0x12345678U;
    for (auto& value : padded_nv12) {
        seed = seed * 1664525U + 1013904223U;
        value = static_cast<std::uint8_t>(seed >> 24U);
    }
    std::vector<std::uint8_t> actual(padded_width * padded_height * 4);
    std::vector<std::uint8_t> reference(actual.size());
    nv12_to_bgra(padded_nv12, padded_width, padded_height, padded_stride, actual);
    const auto y_bytes = static_cast<std::size_t>(padded_stride) * padded_height;
    for (unsigned row = 0; row < padded_height; ++row) {
        for (unsigned x = 0; x < padded_width; ++x) {
            const int c = static_cast<int>(padded_nv12[static_cast<std::size_t>(row) * padded_stride + x]) - 16;
            const auto chroma = y_bytes + static_cast<std::size_t>(row / 2) * padded_stride + (x & ~1U);
            const int d = static_cast<int>(padded_nv12[chroma]) - 128;
            const int e = static_cast<int>(padded_nv12[chroma + 1]) - 128;
            const auto out = (static_cast<std::size_t>(row) * padded_width + x) * 4;
            const auto clamp = [](int value) { return static_cast<std::uint8_t>(std::clamp(value, 0, 255)); };
            reference[out] = clamp((298 * c + 516 * d + 128) >> 8);
            reference[out + 1] = clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
            reference[out + 2] = clamp((298 * c + 409 * e + 128) >> 8);
            reference[out + 3] = 255;
        }
    }
    if (actual != reference) {
        for (std::size_t i = 0; i < actual.size(); ++i) {
            if (actual[i] != reference[i]) {
                std::cerr << "NV12 mismatch at byte " << i << ": actual " << static_cast<unsigned>(actual[i])
                          << ", expected " << static_cast<unsigned>(reference[i]) << '\n';
                break;
            }
        }
    }
    check(actual == reference, "vectorized output must match the reference equations for padded NV12 surfaces");

    try {
        nv12_to_bgra(nv12, 3, height, stride, bgra);
        check(false, "odd NV12 width must be rejected");
    } catch (const std::invalid_argument&) { }
    try {
        nv12_to_bgra(std::span<const std::uint8_t>(nv12).first(8), width, height, stride, bgra);
        check(false, "short NV12 input must be rejected before reading a chroma row");
    } catch (const std::invalid_argument&) { }

    const auto scalar_convert = [](std::span<const std::uint8_t> input, unsigned w, unsigned h,
                                   unsigned row_stride, std::span<std::uint8_t> output) {
        const auto input_y_bytes = static_cast<std::size_t>(row_stride) * h;
        for (unsigned row = 0; row < h; ++row) {
            for (unsigned x = 0; x < w; ++x) {
                const int c = static_cast<int>(input[static_cast<std::size_t>(row) * row_stride + x]) - 16;
                const auto uv = input_y_bytes + static_cast<std::size_t>(row / 2) * row_stride + (x & ~1U);
                const int d = static_cast<int>(input[uv]) - 128;
                const int e = static_cast<int>(input[uv + 1]) - 128;
                const auto out = (static_cast<std::size_t>(row) * w + x) * 4;
                const auto clamp = [](int value) { return static_cast<std::uint8_t>(std::clamp(value, 0, 255)); };
                output[out] = clamp((298 * c + 516 * d + 128) >> 8);
                output[out + 1] = clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
                output[out + 2] = clamp((298 * c + 409 * e + 128) >> 8);
                output[out + 3] = 255;
            }
        }
    };

    constexpr unsigned bench_width = 1920, bench_height = 1080;
    constexpr unsigned bench_stride = bench_width;
    std::vector<std::uint8_t> bench_input(static_cast<std::size_t>(bench_stride) * bench_height * 3 / 2, 128);
    std::vector<std::uint8_t> bench_output(static_cast<std::size_t>(bench_width) * bench_height * 4);
    const auto optimized_started = std::chrono::steady_clock::now();
    for (unsigned frame = 0; frame < 8; ++frame)
        nv12_to_bgra(bench_input, bench_width, bench_height, bench_stride, bench_output);
    const auto optimized_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - optimized_started).count() / 8.0;
    std::vector<std::uint8_t> scalar_output(bench_output.size());
    const auto scalar_started = std::chrono::steady_clock::now();
    for (unsigned frame = 0; frame < 4; ++frame)
        scalar_convert(bench_input, bench_width, bench_height, bench_stride, scalar_output);
    const auto scalar_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - scalar_started).count() / 4.0;
    check(bench_output == scalar_output, "optimized benchmark output must remain pixel-identical to the scalar pipeline");
    std::cout << "NV12 conversion benchmark 1920x1080: " << nv12_converter_name() << ' '
              << optimized_ms << " ms/frame, scalar " << scalar_ms << " ms/frame, speedup "
              << scalar_ms / optimized_ms << "x\n";

    constexpr unsigned uhd_width = 3840, uhd_height = 2160;
    std::vector<std::uint8_t> uhd_input(static_cast<std::size_t>(uhd_width) * uhd_height * 3 / 2, 128);
    std::vector<std::uint8_t> uhd_output(static_cast<std::size_t>(uhd_width) * uhd_height * 4);
    const auto uhd_started = std::chrono::steady_clock::now();
    for (unsigned frame = 0; frame < 3; ++frame)
        nv12_to_bgra(uhd_input, uhd_width, uhd_height, uhd_width, uhd_output);
    const auto uhd_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - uhd_started).count() / 3.0;
    std::cout << "NV12 conversion benchmark 3840x2160: " << nv12_converter_name() << ' '
              << uhd_ms << " ms/frame\n";
}
void framing() {
    constexpr std::array<std::uint8_t, 16> golden{
        0x10,0,0,0,0x67,0x6e,0x69,0x70,1,0,0,0,1,0,0,0};
    const auto ping = encode_capture_ping();
    check(ping == golden, "ping must match recovered native wire bytes, not ASCII tag order");
    for (std::size_t split = 0; split <= ping.size(); ++split) {
        CapturePacketReader reader;
        auto first = reader.consume(std::span(ping).first(split));
        check(first.consumed == split, "fragment prefix must consume only supplied bytes");
        if (split != ping.size()) {
            check(first.status == ParseStatus::NeedMore, "every partial ping must remain incomplete");
            first = reader.consume(std::span(ping).subspan(split));
        }
        check(first.status == ParseStatus::PacketReady && first.packet && first.packet->family == PacketFamily::Ping,
            "every fragmented ping must reconstruct one packet");
        check(reader.buffered_bytes() == 0, "completed packet must not leave stale input");
    }
    CapturePacketReader reader;
    std::vector<std::uint8_t> combined;
    for (int i = 0; i < 4096; ++i) combined.insert(combined.end(), ping.begin(), ping.end());
    std::size_t offset = 0;
    std::size_t packets = 0;
    while (offset < combined.size()) {
        auto result = reader.consume(std::span(combined).subspan(offset));
        if (result.status != ParseStatus::PacketReady || result.consumed != ping.size()) {
            check(false, "coalesced packets must be separated without loss or overconsumption"); break;
        }
        offset += result.consumed;
        ++packets;
    }
    check(packets == 4096, "all 4096 coalesced packets must be delivered");
    const auto need = encode_capture_need(0x12345678U, 0x90abcdefU);
    auto decoded = reader.consume(need);
    check(decoded.packet && decoded.packet->subtype() == 0x6e656564U,
        "need envelope subtype must be at offset 16");
    check(need[8] == 0x78 && need[11] == 0x12 && need[12] == 0xef && need[15] == 0x90,
        "need envelope must preserve both opaque words in little-endian order");
}
void invalid_input() {
    for (std::uint32_t length = 0; length < 8; ++length) {
        auto bytes = encode_capture_ping(); word(bytes, 0, length);
        CapturePacketReader reader;
        auto bad = reader.consume(bytes);
        check(bad.status == ParseStatus::InvalidLength && bad.consumed == 4,
            "impossible length must be rejected after four bytes");
        auto next = reader.consume(encode_capture_ping());
        check(next.status == ParseStatus::InvalidLength && next.consumed == 0,
            "malformed session must remain failed until explicit reset");
        reader.reset();
        check(reader.consume(encode_capture_ping()).status == ParseStatus::PacketReady,
            "reset must permit a fresh capture session");
    }
    CapturePacketReader reader(128);
    auto huge = encode_capture_ping(); word(huge, 0, 0xffffffffU);
    check(reader.consume(huge).status == ParseStatus::InvalidLength && reader.buffered_bytes() == 4,
        "oversized payload declaration must not allocate its body");
    reader.reset();
    auto unknown = encode_capture_ping(); word(unknown, 4, 0x01020304U);
    check(reader.consume(unknown).status == ParseStatus::UnsupportedFamily,
        "unknown families must fail explicitly");
    reader.reset();
    auto short_sync = encode_capture_need(1, 2); word(short_sync, 4, static_cast<std::uint32_t>(PacketFamily::Sync));
    check(reader.consume(short_sync).status == ParseStatus::InvalidLength,
        "sync request must include both correlation words");
    reader.reset();
    const auto ping = encode_capture_ping();
    check(reader.consume(std::span(ping).first(7)).status == ParseStatus::NeedMore,
        "partial pre-disconnect packet must remain buffered");
    reader.reset();
    check(reader.consume(ping).status == ParseStatus::PacketReady,
        "post-disconnect reset must discard previous-generation bytes");
    try { CapturePacketReader invalid(8); check(false, "invalid memory bound must throw"); }
    catch (const std::invalid_argument&) { }
}
void nested_samples() {
    std::array<std::uint8_t, 32> bytes{};
    word(bytes, 0, 32); word(bytes, 4, static_cast<std::uint32_t>(PacketFamily::Async));
    word(bytes, 16, 0x66656564U); word(bytes, 20, 12); word(bytes, 24, 0x73627566U);
    bytes[28] = 0x31; bytes[29] = 0x32; bytes[30] = 0x33; bytes[31] = 0x34;
    CapturePacketReader reader;
    auto result = reader.consume(bytes);
    auto sample = result.packet->sample_buffer();
    check(sample && sample->size() == 4 && (*sample)[0] == 0x31 && (*sample)[3] == 0x34,
        "nested sbuf must preserve exact bounded sample bytes");
    for (const auto length : {0U, 7U, 13U, 0xffffffffU}) {
        word(bytes, 20, length);
        result = reader.consume(bytes);
        check(!result.packet->sample_buffer(), "invalid nested length must not expose a sample span");
    }
    word(bytes, 20, 12); word(bytes, 24, 0);
    result = reader.consume(bytes);
    check(!result.packet->sample_buffer(), "wrong nested type must not be accepted as sbuf");
    check(needs_zero_length_transfer(512, 512), "packet-sized write needs terminating ZLP");
    check(!needs_zero_length_transfer(513, 512), "nonmultiple write must not append ZLP");
    check(!needs_zero_length_transfer(0, 512) && !needs_zero_length_transfer(512, 0),
        "zero lengths and invalid endpoint sizes must not request repeated ZLPs");
}
void endpoints() {
    std::array<BulkEndpoint, 2> pipes{{{0x81, 512, true}, {0x02, 512, true}}};
    auto selected = select_capture_endpoints(pipes);
    check(selected && selected->input == 0x81 && selected->output == 0x02,
        "descriptor selection must identify endpoint direction");
    std::reverse(pipes.begin(), pipes.end());
    selected = select_capture_endpoints(pipes);
    check(selected && selected->input == 0x81 && selected->output == 0x02,
        "descriptor selection must support either endpoint order");
    pipes[1].address = 0x03;
    check(!select_capture_endpoints(pipes), "two output endpoints must be rejected");
    pipes[1] = {0x81, 0, true};
    check(!select_capture_endpoints(pipes), "unknown endpoint packet size must fail closed");
    pipes[1] = {0x81, 512, false};
    check(!select_capture_endpoints(pipes), "interrupt endpoint cannot stand in for bulk capture");
    pipes[1] = {0xf1, 512, true};
    check(!select_capture_endpoints(pipes), "reserved endpoint address bits must be rejected");
    check(!select_capture_endpoints(std::span(pipes).first(1)), "capture requires two endpoints");
}
void session_negotiation() {
    UsbCaptureSession session;
    auto ping=encode_capture_ping();
    auto acknowledged=session.receive({PacketFamily::Ping,{ping.begin(),ping.end()}});
    check(acknowledged.outgoing.size()==1 && acknowledged.outgoing[0].size()==16 && acknowledged.outgoing[0][8]==0 && acknowledged.outgoing[0][12]==1,
        "the live QuickTime session must use the documented 0/1 host acknowledgement");
    std::vector<std::uint8_t> request(36);
    word(request,0,36);word(request,4,0x73796e63);word(request,16,0x63777061);
    word(request,20,0x12345678);word(request,24,0x89abcdef);word(request,28,0x87654321);word(request,32,0x01020304);
    const auto actions=session.receive({PacketFamily::Sync,request});
    check(actions.outgoing.size()==3,"CWPA must announce display, acknowledge correlation, and announce PCM audio");
    if(actions.outgoing.size()==3) {
        const auto& reply=actions.outgoing[1];
        check(reply.size()==28 && reply[4]==0x79 && reply[8]==0x78 && reply[12]==0xef && reply[20]==1,
            "CWPA response must echo request correlation and return a local clock ID");
        const auto& hpa=actions.outgoing[2];
        check(hpa[8]==0x21 && hpa[15]==1,"audio announcement must use device clock, not correlation or local clock");
    }
    std::vector<std::uint8_t> cvrp(36);
    word(cvrp,0,36);word(cvrp,4,0x73796e63);word(cvrp,16,0x63767270);
    word(cvrp,28,0x12345678);word(cvrp,32,0x90abcdef);
    CapturePacket cvrp_packet{PacketFamily::Sync,cvrp};
    const auto video_start=session.receive(cvrp_packet);
    const auto need=session.video_need();
    check(cvrp_packet.subtype()==0x63767270U,"CVRP subtype must match the little-endian dispatch value");
    check(video_start.outgoing.size()==2 && video_start.outgoing[0]==need,
        "CVRP must start video with the same reusable NEED packet used by the paced request pump");
    check(need.size()==20 && need[8]==0x78 && need[11]==0x12 && need[12]==0xef && need[15]==0x90,
        "video NEED cadence packet must retain the phone-provided CVRP clock reference");
    for(std::size_t length=28;length<36;++length) {
        auto truncated=request;truncated.resize(length);word(truncated,0,static_cast<std::uint32_t>(length));
        try{UsbCaptureSession fresh;fresh.receive({PacketFamily::Sync,truncated});check(false,"truncated CWPA clock must be rejected");}catch(const std::runtime_error&){}
    }
    std::vector<std::uint8_t> feed(40);
    word(feed,0,40);word(feed,4,0x6173796e);word(feed,16,0x66656564);word(feed,20,20);word(feed,24,0x73627566);
    word(feed,28,12);word(feed,32,0x73646174);feed[39]=1;
    // SDAT claims one byte of NAL payload but contains only its length prefix.
    try{session.receive({PacketFamily::Async,feed});check(false,"NAL overrun must be rejected before decoder access");}catch(const std::runtime_error&){}
    auto stop=session.shutdown_packets();
    check(stop.size()==2 && stop[0].size()==20 && stop[1].size()==20,"shutdown must release audio and video streams");
}
void network_word(std::vector<std::uint8_t>& bytes,std::size_t offset,std::uint32_t value) {
    for(unsigned i=0;i<4;++i)bytes[offset+i]=static_cast<std::uint8_t>(value>>(24-i*8));
}
std::vector<std::uint8_t> mux_tcp(std::uint8_t flags,std::uint32_t sequence,std::span<const std::uint8_t> data={}) {
    std::vector<std::uint8_t> b(36+data.size());network_word(b,0,6);network_word(b,4,static_cast<std::uint32_t>(b.size()));network_word(b,8,0xfaceface);
    b[16]=0xf2;b[17]=0x7e;b[19]=1;network_word(b,20,sequence);network_word(b,24,1);b[28]=0x50;b[29]=flags;b[30]=2;
    std::copy(data.begin(),data.end(),b.begin()+36);return b;
}
void usb_mux_initialization() {
    const std::array<std::uint8_t,20> greeting{0,0,0,0,0,0,0,20,0,0,0,2,0,0,0,0,0,0,0,0};
    for(std::size_t split=0;split<=greeting.size();++split) {
        UsbMuxControl mux;check(std::equal(greeting.begin(),greeting.end(),mux.start().begin()),"USBMux version greeting must use network byte order");
        auto a=mux.receive(std::span(greeting).first(split));
        if(split<greeting.size()){check(a.outgoing.empty(),"partial USBMux version cannot initialize a tunnel");a=mux.receive(std::span(greeting).subspan(split));}
        check(a.outgoing.size()==2 && a.outgoing[0].size()==17 && a.outgoing[1].size()==36,"v2 USBMux must emit SETUP followed by TCP SYN");
        if(a.outgoing.size()==2) {
            const auto& setup=a.outgoing[0];const auto& syn=a.outgoing[1];
            check(setup[3]==2 && setup[7]==17 && setup[8]==0xfe && setup[15]==0xff && setup[16]==7,"USBMux SETUP must match the documented link marker and setup byte");
            check(syn[17]==1 && syn[18]==0xf2 && syn[19]==0x7e && syn[29]==2,"control SYN must target network-order lockdownd port 62078");
        }
    }
    UsbMuxControl mux;mux.receive(greeting);
    auto connected=mux.receive(mux_tcp(0x12,0));
    check(connected.outgoing.size()==2 && !mux.ready(),"control TCP connect must ACK and query service type without claiming authentication");
    const std::string type="<plist><dict><key>Type</key><string>com.apple.mobile.lockdown</string></dict></plist>";
    std::vector<std::uint8_t> body(type.size()+4);network_word(body,0,static_cast<std::uint32_t>(type.size()));std::copy(type.begin(),type.end(),body.begin()+4);
    auto response=mux_tcp(0x10,1,body);auto first=mux.receive(std::span(response).first(17));
    check(first.outgoing.empty(),"fragmented USBMux TCP replies must retain their partial header");
    auto typed=mux.receive(std::span(response).subspan(17));
    check(typed.outgoing.size()==2 && !mux.ready(),"verified service type must trigger the read-only OS query");
    const std::string version="<plist><dict><key>Value</key><string>18.0</string></dict></plist>";
    std::vector<std::uint8_t> version_body(version.size()+4);network_word(version_body,0,static_cast<std::uint32_t>(version.size()));std::copy(version.begin(),version.end(),version_body.begin()+4);
    mux.receive(mux_tcp(0x10,static_cast<std::uint32_t>(1+body.size()),version_body));
    check(mux.ready(),"read-only lockdownd readiness requires both request responses");
    auto malformed=std::vector<std::uint8_t>(8);network_word(malformed,4,65537);
    UsbMuxControl bad;
    try{bad.receive(malformed);check(false,"oversized USBMux frame must fail before body allocation");}catch(const std::runtime_error&){}
    try{bad.receive(greeting);check(false,"malformed USBMux session must remain failed");}catch(const std::runtime_error&){}
}
#ifdef _WIN32
void recorded_video_decode() {
    std::ifstream input(USB_VIDEO_FIXTURE,std::ios::binary|std::ios::ate);
    check(bool(input),"the pinned upstream USB video fixture must exist");if(!input)return;
    const auto length=input.tellg();
    check(length==91607,"recorded USB fixture must retain its exact size");if(length!=91607)return;
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));input.seekg(0);input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    uxm_usb_decode_report report{};
    check(uxm_usb_decode_recording(bytes.data(),static_cast<unsigned>(bytes.size()),&report)==0,"production session parser and Windows decoder must turn the capture fixture into pixels");
    check(report.decoded_frames==1 && (report.width==1136 || report.width==1126) && (report.height==2448 || report.height==2436) && report.pixel_checksum!=0,
        "the recorded fixture must decode one valid image (1136x2448 uncropped or 1126x2436 aperture-compacted), not just parse a header");
    bytes.pop_back();
    check(uxm_usb_decode_recording(bytes.data(),static_cast<unsigned>(bytes.size()),&report)!=0,"truncated recording cannot be a decode pass");
}
#endif
} // namespace

int main() {
    framing(); invalid_input(); nested_samples(); endpoints(); session_negotiation(); usb_mux_initialization(); video_conversion();
#ifdef _WIN32
    recorded_video_decode();
    check(uxm_usb_mirror_create(3840,2160,0)==nullptr,"USB mirror must reject a zero frame-request cadence");
    auto* valid_mirror=uxm_usb_mirror_create(3840,2160,60);
    check(valid_mirror!=nullptr,"USB mirror must accept a 60 FPS best-effort request target");
    if(valid_mirror)uxm_usb_mirror_destroy(valid_mirror);
    try {
        auto transport = WinUsbCaptureTransport::open(L"");
        check(false, "empty device path must be rejected before device I/O");
    } catch (const std::invalid_argument&) { }
#endif
    if (failures) return EXIT_FAILURE;
    std::cout << "Recovered USB packet tests passed; no hardware capture claim\n";
    return EXIT_SUCCESS;
}
