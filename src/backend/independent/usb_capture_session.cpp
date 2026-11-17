// Protocol cross-check: danielpaulus/quicktime_video_hack (MIT, 2019).
// See THIRD_PARTY_NOTICES.md. Serialization and validation are implemented here.
#include "usb_capture_session.hpp"
#include <bit>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <string_view>

namespace unkxwyno::usb {
namespace {
using Bytes = std::vector<std::uint8_t>;
using View = std::span<const std::uint8_t>;
constexpr std::uint32_t tag(std::string_view v) {
    return (std::uint32_t(v[0]) << 24) | (std::uint32_t(v[1]) << 16) | (std::uint32_t(v[2]) << 8) | std::uint32_t(v[3]);
}
std::uint32_t u32(View s, std::size_t offset = 0) {
    if (offset > s.size() || s.size() - offset < 4) throw std::runtime_error("Truncated USB media integer.");
    return std::uint32_t(s[offset]) | (std::uint32_t(s[offset+1]) << 8) | (std::uint32_t(s[offset+2]) << 16) | (std::uint32_t(s[offset+3]) << 24);
}
std::uint64_t u64(View s, std::size_t offset = 0) { return u32(s, offset) | (std::uint64_t(u32(s, offset+4)) << 32); }
void put32(Bytes& s, std::uint32_t v) { for (unsigned i=0;i<4;++i) s.push_back(static_cast<std::uint8_t>(v >> (8*i))); }
void put64(Bytes& s, std::uint64_t v) { put32(s, static_cast<std::uint32_t>(v)); put32(s, static_cast<std::uint32_t>(v >> 32)); }
void append(Bytes& s, View data) { s.insert(s.end(), data.begin(), data.end()); }
Bytes atom(std::uint32_t kind, View payload) {
    Bytes b; put32(b, static_cast<std::uint32_t>(payload.size()+8)); put32(b, kind); append(b,payload); return b;
}
Bytes text_atom(std::uint32_t kind, std::string_view text) { return atom(kind, View(reinterpret_cast<const std::uint8_t*>(text.data()), text.size())); }
Bytes number(double number) { Bytes b{6}; put64(b,std::bit_cast<std::uint64_t>(number)); return atom(tag("nmbv"),b); }
Bytes integer(std::uint32_t number) { Bytes b{3}; put32(b,number); return atom(tag("nmbv"),b); }
Bytes boolean(bool value) { return atom(tag("bulv"), Bytes{static_cast<std::uint8_t>(value)}); }
Bytes dict(std::initializer_list<std::pair<std::string_view,Bytes>> fields) {
    Bytes body;
    for (const auto& [name,value] : fields) { Bytes pair = text_atom(tag("strk"),name); append(pair,value); append(body,atom(tag("keyv"),pair)); }
    return atom(tag("dict"),body);
}
Bytes asyn(std::uint32_t subtype, std::uint64_t clock, View body = {}) {
    Bytes b; put32(b,static_cast<std::uint32_t>(20+body.size())); put32(b,tag("asyn")); put64(b,clock); put32(b,subtype); append(b,body); return b;
}
Bytes reply(const CapturePacket& p, View body) {
    Bytes b; put32(b,static_cast<std::uint32_t>(20+body.size())); put32(b,tag("rply")); put64(b,u64(p.bytes,20)); put32(b,0); append(b,body); return b;
}
Bytes clock_reply(const CapturePacket& p, std::uint64_t clock) { Bytes body; put64(body,clock); return reply(p,body); }
struct Atom { std::uint32_t kind; View body; };
void atoms(View bytes, const std::function<void(Atom)>& visit) {
    std::size_t count{};
    while (!bytes.empty()) {
        if (++count > 4096 || bytes.size() < 8) throw std::runtime_error("Malformed CoreMedia atom list.");
        const auto length = u32(bytes);
        if (length < 8 || length > bytes.size()) throw std::runtime_error("CoreMedia atom length exceeds its container.");
        visit({u32(bytes,4),bytes.subspan(8,length-8)});
        bytes = bytes.subspan(length);
    }
}
void walk(View bytes, const std::function<void(Atom)>& visit, unsigned depth = 0) {
    if (depth > 16) throw std::runtime_error("CoreMedia dictionary nesting limit exceeded.");
    atoms(bytes,[&](Atom a) {
        visit(a);
        if (a.kind == tag("dict") || a.kind == tag("keyv") || a.kind == tag("fdsc") || a.kind == tag("extn")) walk(a.body,visit,depth+1);
    });
}
void parse_avcc(View data, Bytes& parameters, std::uint32_t& length_size) {
    if (data.size() < 7 || data[0] != 1) return;
    std::size_t offset=6;
    Bytes result;
    auto read_units = [&](unsigned count, unsigned expected) {
        for (unsigned i=0;i<count;++i) {
            if (offset+2 > data.size()) throw std::runtime_error("Truncated AVC configuration length.");
            auto length = (std::size_t(data[offset]) << 8) | data[offset+1]; offset+=2;
            if (!length || length > data.size()-offset || (data[offset]&31) != expected) throw std::runtime_error("Invalid AVC parameter set.");
            append(result,Bytes{0,0,0,1}); append(result,data.subspan(offset,length)); offset+=length;
        }
    };
    const auto sps_count = unsigned(data[5]&31);
    if (!sps_count) return;
    read_units(sps_count,7);
    if (offset >= data.size()) throw std::runtime_error("Missing AVC picture parameter sets.");
    const auto pps_count = unsigned(data[offset++]);
    if (!pps_count) throw std::runtime_error("Empty AVC picture parameter sets.");
    read_units(pps_count,8);
    length_size = (data[4]&3)+1;
    parameters = std::move(result);
}
Bytes pcm_description() {
    Bytes body; put64(body,std::bit_cast<std::uint64_t>(48000.0));
    for (auto v : {tag("lpcm"),12U,4U,1U,4U,2U,16U,0U}) put32(body,v);
    put64(body,std::bit_cast<std::uint64_t>(48000.0)); put64(body,std::bit_cast<std::uint64_t>(48000.0)); return body;
}
std::int64_t time_100ns(View body) {
    if (body.size()<24) throw std::runtime_error("Truncated CoreMedia timestamp.");
    const auto scale=u32(body,8);
    if (!scale || !(u32(body,12)&1)) return -1;
    const auto value=std::bit_cast<std::int64_t>(u64(body));
    // Quotient/remainder avoid overflowing a long-running nanosecond clock.
    return (value/scale)*10000000LL + (value%scale)*10000000LL/scale;
}
}
UsbCaptureSession::UsbCaptureSession(std::uint32_t width, std::uint32_t height)
    : display_width_(width), display_height_(height) {
    if (!width || !height || width>8192 || height>8192) throw std::invalid_argument("Invalid requested USB display dimensions.");
}
CaptureActions UsbCaptureSession::receive(const CapturePacket& p) {
    CaptureActions a;
    if (p.bytes.size()<8 || u32(p.bytes)!=p.bytes.size() || u32(p.bytes,4)!=static_cast<std::uint32_t>(p.family))
        throw std::runtime_error("Invalid complete USB capture packet.");
    if (p.family == PacketFamily::Ping) {
        auto ping=encode_capture_ping();
        // The old regular-package recorder uses 1/1. Current QuickTime hosts
        // acknowledge 0/1. This connected iPhone repeats PING after 1/1.
        // Retain the old encoder as RE evidence; use the negotiated protocol's
        // host acknowledgement here rather than substituting its legacy value.
        ping[8]=0;
        a.outgoing.emplace_back(ping.begin(),ping.end());
        a.event="iPhone USB PING received; replying with QuickTime host acknowledgement 0/1.";
        return a;
    }
    const auto subtype=p.subtype();
    if (!subtype) return a;
    if (p.family == PacketFamily::Sync) {
        if (p.bytes.size()<28) throw std::runtime_error("Truncated USB sync request.");
        switch (*subtype) {
        case tag("cwpa"):
            audio_clock_=u64(p.bytes,28);
            if (!announced_) {
                const auto dimensions=dict({{"Width",number(display_width_)},{"Height",number(display_height_)}});
                const auto video=dict({{"Valeria",boolean(true)},{"HEVCDecoderSupports444",boolean(false)},{"DisplaySize",dimensions}});
                a.outgoing.push_back(asyn(tag("hpd1"),1,video));
                announced_=true;
            }
            a.outgoing.push_back(clock_reply(p,0x10001));
            a.outgoing.push_back(asyn(tag("hpa1"),audio_clock_,dict({
                {"BufferAheadInterval",number(0.073)},{"deviceUID",text_atom(tag("strv"),"UNKXWYNO USB")},
                {"ScreenLatency",number(0.04)},{"formats",atom(tag("datv"),pcm_description())},
                {"EDIDAC3Support",integer(0)},{"deviceName",text_atom(tag("strv"),"UNKXWYNO Mirror")}})));
            a.event="Negotiating iPhone video and audio.";
            break;
        case tag("cvrp"):
            video_clock_=u64(p.bytes,28);
            if (p.bytes.size()>36) walk(View(p.bytes).subspan(36),[&](Atom item) {
                if(item.kind==tag("vdim") && item.body.size()==8) { width_=u32(item.body); height_=u32(item.body,4); }
                if(item.kind==tag("datv")) parse_avcc(item.body,parameter_sets_,nalu_length_);
            });
            a.outgoing.push_back(asyn(tag("need"),video_clock_));
            a.outgoing.push_back(clock_reply(p,0x20001));
            a.event="iPhone accepted USB video capture; waiting for samples.";
            break;
        case tag("clok"): a.outgoing.push_back(clock_reply(p,0x30001)); break;
        case tag("time"): {
            const auto now=std::chrono::steady_clock::now().time_since_epoch();
            Bytes body; put64(body,static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count()));
            put32(body,1000000000); put32(body,1); put64(body,0); a.outgoing.push_back(reply(p,body)); break;
        }
        case tag("afmt"): {
            View body=View(p.bytes).subspan(28);
            if(body.size()<40 || u32(body,8)!=tag("lpcm")) throw std::runtime_error("Unsupported iPhone USB audio format; PCM is required.");
            const auto rate=std::bit_cast<double>(u64(body));
            if(rate<8000 || rate>192000 || u32(body,28)>8 || u32(body,28)==0 || u32(body,32)!=16 || (u32(body,12)&3))
                throw std::runtime_error("Unsupported USB PCM sample layout.");
            audio_rate_=static_cast<std::uint32_t>(rate); audio_channels_=u32(body,28); audio_bits_=16;
            a.outgoing.push_back(reply(p,dict({{"Error",integer(0)}}))); break;
        }
        case tag("skew"): { Bytes body; put64(body,std::bit_cast<std::uint64_t>(1.0)); a.outgoing.push_back(reply(p,body)); break; }
        case tag("go! "): a.outgoing.push_back(reply(p,Bytes(4))); break;
        case tag("stop"): a.outgoing.push_back(reply(p,Bytes(8))); break;
        case tag("stat"): { Bytes body; put32(body,4); a.outgoing.push_back(reply(p,body)); break; }
        case tag("cepm"): { Bytes body; put64(body,0x00b1c650); a.outgoing.push_back(reply(p,body)); break; }
        default: throw std::runtime_error("Unsupported iPhone USB sync request: " + std::to_string(*subtype));
        }
        return a;
    }
    if (p.family != PacketFamily::Async || (*subtype!=tag("feed") && *subtype!=tag("eat!"))) return a;
    const auto sample=p.sample_buffer();
    if(!sample) throw std::runtime_error("Invalid CoreMedia sample buffer envelope.");
    View sample_data;
    std::int64_t timestamp=-1;
    walk(*sample,[&](Atom item) {
        if(item.kind==tag("sdat")) sample_data=item.body;
        if(item.kind==tag("opts")) timestamp=time_100ns(item.body);
        if(*subtype==tag("feed")) {
            if(item.kind==tag("vdim") && item.body.size()==8) {
                const auto w=u32(item.body), h=u32(item.body,4);
                if(width_!=w || height_!=h) format_sent_=false;
                width_=w; height_=h;
            }
            if(item.kind==tag("codc") && u32(item.body)!=tag("avc1")) throw std::runtime_error("The iPhone selected a codec other than H.264.");
            if(item.kind==tag("datv")) { auto previous=parameter_sets_; parse_avcc(item.body,parameter_sets_,nalu_length_); if(previous!=parameter_sets_) format_sent_=false; }
        }
    });
    if (*subtype==tag("eat!")) {
        a.pcm.assign(sample_data.begin(),sample_data.end()); a.audio_rate=audio_rate_; a.audio_channels=audio_channels_; a.audio_bits=audio_bits_; return a;
    }
    if(sample_data.empty()) return a;
    CapturedVideo v; v.width=width_; v.height=height_; v.timestamp_valid=timestamp>=0;
    v.time_100ns=timestamp<0 ? frame_index_*166667 : timestamp; ++frame_index_;
    if(!format_sent_) { append(v.annex_b,parameter_sets_); format_sent_=true; }
    while(!sample_data.empty()) {
        if(sample_data.size()<nalu_length_) throw std::runtime_error("Truncated video NAL length.");
        std::size_t length{};
        for(std::uint32_t i=0;i<nalu_length_;++i) length=(length<<8)|sample_data[i];
        sample_data=sample_data.subspan(nalu_length_);
        if(length==0 || length>sample_data.size()) throw std::runtime_error("Video NAL exceeds its sample buffer.");
        append(v.annex_b,Bytes{0,0,0,1}); append(v.annex_b,sample_data.first(length)); sample_data=sample_data.subspan(length);
    }
    a.video=std::move(v); return a;
}
std::vector<std::uint8_t> UsbCaptureSession::video_need() const { return asyn(tag("need"),video_clock_); }
std::vector<std::vector<std::uint8_t>> UsbCaptureSession::shutdown_packets() const {
    return {asyn(tag("hpa0"),audio_clock_),asyn(tag("hpd0"),1)};
}
}
