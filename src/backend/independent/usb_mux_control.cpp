#include "usb_mux_control.hpp"
#include <algorithm>
#include <stdexcept>
#include <string_view>
namespace unkxwyno::usb {
namespace {
using Bytes=std::vector<std::uint8_t>;
using View=std::span<const std::uint8_t>;
std::uint32_t be32(View b,std::size_t p=0) {
    if(p>b.size() || b.size()-p<4)throw std::runtime_error("Truncated USBMux integer.");
    return (std::uint32_t(b[p])<<24)|(std::uint32_t(b[p+1])<<16)|(std::uint32_t(b[p+2])<<8)|b[p+3];
}
std::uint16_t be16(View b,std::size_t p) {
    if(p>b.size() || b.size()-p<2)throw std::runtime_error("Truncated USBMux port.");
    return static_cast<std::uint16_t>((unsigned(b[p])<<8)|b[p+1]);
}
void put32(Bytes& b,std::uint32_t n){for(int i=3;i>=0;--i)b.push_back(static_cast<std::uint8_t>(n>>(i*8)));}
void put16(Bytes& b,std::uint16_t n){b.push_back(static_cast<std::uint8_t>(n>>8));b.push_back(static_cast<std::uint8_t>(n));}
void append(Bytes& b,View data){b.insert(b.end(),data.begin(),data.end());}
}
Bytes UsbMuxControl::start() const {
    Bytes b;put32(b,0);put32(b,20);put32(b,2);put32(b,0);put32(b,0);return b;
}
Bytes UsbMuxControl::packet(std::uint32_t protocol,View payload) {
    Bytes b;put32(b,protocol);put32(b,static_cast<std::uint32_t>(payload.size()+(version_>=2?16:8)));
    if(version_>=2){put32(b,0xfeedface);put16(b,link_tx_++);put16(b,link_rx_);}
    append(b,payload);return b;
}
Bytes UsbMuxControl::tcp(std::uint8_t flags,View data) {
    Bytes b;put16(b,1);put16(b,62078);put32(b,tcp_tx_);put32(b,tcp_rx_);
    b.push_back(0x50);b.push_back(flags);put16(b,512);put16(b,0);put16(b,0);append(b,data);
    return packet(6,b);
}
Bytes UsbMuxControl::query(bool product_version) {
    const std::string body="<?xml version=\"1.0\" encoding=\"UTF-8\"?><!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\"><plist version=\"1.0\"><dict><key>Label</key><string>UNKXWYNO Mirror</string><key>Request</key><string>"+
        std::string(product_version?"GetValue":"QueryType")+"</string>"+(product_version?"<key>Key</key><string>ProductVersion</string>":"")+"</dict></plist>";
    Bytes b;put32(b,static_cast<std::uint32_t>(body.size()));b.insert(b.end(),body.begin(),body.end());
    auto result=tcp(0x10,b);tcp_tx_+=static_cast<std::uint32_t>(b.size());return result;
}
void UsbMuxControl::process(View p,UsbMuxActions& a) {
    const auto protocol=be32(p);
    if(version_==0) {
        if(protocol!=0 || p.size()!=20 || (be32(p,8)!=1 && be32(p,8)!=2))throw std::runtime_error("Unsupported USBMux version negotiation.");
        version_=be32(p,8);
        if(version_==2){const std::array<std::uint8_t,1> setup{7};a.outgoing.push_back(packet(2,setup));}
        a.outgoing.push_back(tcp(2));a.event="USBMux negotiated; connecting to the iPhone control port.";return;
    }
    const std::size_t header=version_==2?16:8;
    if(p.size()<header)throw std::runtime_error("Truncated USBMux link header.");
    if(version_==2){
        // The host emits FEEDFACE; this connected iPhone emits FACEFACE.
        // Upstream usbmuxd does not require incoming magic to equal host magic.
        const auto marker=be32(p,8);
        if(marker!=0xfeedface && marker!=0xfaceface)throw std::runtime_error("Unsupported USBMux device link marker.");
        link_rx_=be16(p,14);
    }
    if(protocol==1 || protocol==2)return;
    if(protocol!=6)throw std::runtime_error("Unsupported USBMux link packet.");
    auto t=p.subspan(header);
    if(t.size()<20 || be16(t,0)!=62078 || be16(t,2)!=1)throw std::runtime_error("USBMux control tunnel port mismatch.");
    const auto tcp_header=std::size_t(t[12]>>4)*4;
    if(tcp_header<20 || tcp_header>t.size())throw std::runtime_error("Invalid USBMux TCP header length.");
    const auto flags=t[13];const auto sequence=be32(t,4),ack=be32(t,8);
    if(flags&4)throw std::runtime_error("The iPhone refused or reset the control connection. Unlock it and verify computer trust.");
    if(!connected_) {
        if(flags!=0x12 || ack!=1)throw std::runtime_error("Invalid USBMux control SYN acknowledgement.");
        connected_=true;tcp_tx_=1;tcp_rx_=sequence+1;a.outgoing.push_back(tcp(0x10));a.outgoing.push_back(query(false));
        a.event="USBMux control port connected; checking lockdownd.";return;
    }
    if((flags&0x10)==0 || (flags&3)!=0 || ack>tcp_tx_)throw std::runtime_error("Invalid USBMux control stream state.");
    const auto data=t.subspan(tcp_header);
    if(data.empty())return;
    if(sequence!=tcp_rx_) {
        if(sequence<tcp_rx_ && data.size()<=tcp_rx_-sequence){a.outgoing.push_back(tcp(0x10));return;}
        throw std::runtime_error("Out-of-order USBMux control data.");
    }
    if(ready_) {
        if(control_data_.size()+data.size()>1024U*1024U)throw std::runtime_error("USB control stream receive bound exceeded.");
        append(control_data_,data);tcp_rx_+=static_cast<std::uint32_t>(data.size());a.outgoing.push_back(tcp(0x10));return;
    }
    if(plist_.size()+data.size()>1024U*1024U+4)throw std::runtime_error("USBMux control reply exceeds the 1 MiB bound.");
    append(plist_,data);tcp_rx_+=static_cast<std::uint32_t>(data.size());a.outgoing.push_back(tcp(0x10));
    while(plist_.size()>=4) {
        const auto length=be32(plist_);
        if(!length || length>1024U*1024U)throw std::runtime_error("Invalid lockdownd reply length.");
        if(plist_.size()<length+4)return;
        const std::string xml(plist_.begin()+4,plist_.begin()+4+length);
        // Inspect only well-known read-only result markers; never log a reply.
        if(xml.find("<key>Error</key>")!=std::string::npos)
            throw std::runtime_error("The iPhone rejected its read-only control check. Verify unlock/computer trust; no pairing records were accessed.");
        if(query_stage_==0) {
            if(xml.find("com.apple.mobile.lockdown")==std::string::npos)throw std::runtime_error("Unexpected iPhone control service response.");
            ++query_stage_;a.outgoing.push_back(query(true));
        } else if(query_stage_==1) {
            if(xml.find("<key>Value</key>")==std::string::npos)throw std::runtime_error("The iPhone did not answer its OS capability query.");
            std::string os;
            const auto value=xml.find("<key>Value</key>"),begin=xml.find("<string>",value),end=begin==std::string::npos?std::string::npos:xml.find("</string>",begin);
            if(begin!=std::string::npos && end!=std::string::npos && end-begin<=40) {
                os=xml.substr(begin+8,end-begin-8);
                if(os.empty() || !std::all_of(os.begin(),os.end(),[](char c){return (c>='0' && c<='9') || c=='.';}))os.clear();
            }
            ++query_stage_;ready_=true;a.event="iPhone USBMux and read-only lockdownd communication are ready"+(os.empty()?std::string{}:" (iOS "+os+")")+". Authenticated capture session is not yet verified.";
        }
        plist_.erase(plist_.begin(),plist_.begin()+4+length);
    }
}
UsbMuxActions UsbMuxControl::receive(View bytes) {
    if(failed_)throw std::runtime_error("USBMux control session has already failed.");
    UsbMuxActions a;
    try {
        if(incoming_.size()+bytes.size()>128U*1024U)throw std::runtime_error("USBMux input exceeds its buffer bound.");
        append(incoming_,bytes);
        while(incoming_.size()>=8) {
            const auto size=be32(incoming_,4);
            if(size<8 || size>65536)throw std::runtime_error("Invalid USBMux frame size.");
            if(incoming_.size()<size)break;
            process(View(incoming_).first(size),a);
            incoming_.erase(incoming_.begin(),incoming_.begin()+size);
        }
        return a;
    }catch(...){failed_=true;throw;}
}
Bytes UsbMuxControl::take_control_data() {Bytes data;data.swap(control_data_);return data;}
std::vector<Bytes> UsbMuxControl::write_control_data(View bytes) {
    if(!ready_ || failed_)throw std::runtime_error("USB control tunnel is not ready.");
    if(bytes.size()>1024U*1024U)throw std::runtime_error("USB control write bound exceeded.");
    std::vector<Bytes> result;
    while(!bytes.empty()) {
        const auto count=std::min<std::size_t>(bytes.size(),16000);
        result.push_back(tcp(0x10,bytes.first(count)));tcp_tx_+=static_cast<std::uint32_t>(count);bytes=bytes.subspan(count);
    }
    return result;
}
}
