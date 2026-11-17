#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <lusb0_usb.h>
#include "libusb_win32_capture_transport.hpp"
#include "usb_mux_control.hpp"
#include <mutex>
#include <thread>
#include <stdexcept>
#include <condition_variable>
#include <deque>

namespace unkxwyno::usb {
namespace {
class DirectApi {
    HMODULE module_{};
    template<class T> void bind(T& fn,const char* name) {fn=reinterpret_cast<T>(GetProcAddress(module_,name));if(!fn)throw std::runtime_error(std::string("Missing installed libusb-win32 API: ")+name);}
public:
#define USB0_API(n) decltype(&usb_##n) n{};
    USB0_API(init) USB0_API(find_busses) USB0_API(find_devices) USB0_API(get_busses) USB0_API(open) USB0_API(close)
    USB0_API(control_msg) USB0_API(set_configuration) USB0_API(claim_interface) USB0_API(release_interface)
    USB0_API(set_altinterface) USB0_API(clear_halt) USB0_API(bulk_read) USB0_API(bulk_write) USB0_API(strerror)
    USB0_API(bulk_setup_async) USB0_API(submit_async) USB0_API(reap_async_nocancel) USB0_API(cancel_async) USB0_API(free_async)
#undef USB0_API
    DirectApi() {
        module_=LoadLibraryExW(L"libusb0.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!module_)throw std::runtime_error("The installed libusb-win32 driver runtime is unavailable.");
        try {
#define USB0_BIND(n) bind(n,"usb_" #n);
            USB0_BIND(init) USB0_BIND(find_busses) USB0_BIND(find_devices) USB0_BIND(get_busses) USB0_BIND(open) USB0_BIND(close)
            USB0_BIND(control_msg) USB0_BIND(set_configuration) USB0_BIND(claim_interface) USB0_BIND(release_interface)
            USB0_BIND(set_altinterface) USB0_BIND(clear_halt) USB0_BIND(bulk_read) USB0_BIND(bulk_write) USB0_BIND(strerror)
            USB0_BIND(bulk_setup_async) USB0_BIND(submit_async) USB0_BIND(reap_async_nocancel) USB0_BIND(cancel_async) USB0_BIND(free_async)
#undef USB0_BIND
        }catch(...){FreeLibrary(module_);module_=nullptr;throw;}
        init();
    }
    ~DirectApi(){if(module_)FreeLibrary(module_);}
    void checked(int code,const char* operation)const{if(code<0)throw std::runtime_error(std::string(operation)+": "+strerror()+" ("+std::to_string(code)+")");}
};
std::mutex api_enumeration_mutex;
std::uint16_t le16(const std::vector<unsigned char>& bytes,std::size_t offset) {return static_cast<std::uint16_t>(bytes[offset]|(unsigned(bytes[offset+1])<<8));}
}
struct LibUsbWin32CaptureTransport::Impl {
    DirectApi api;usb_dev_handle* handle{};
    int interface_number{-1};unsigned char input{},output{};unsigned short packet_size{};
    int mux_interface{-1};unsigned char mux_input{},mux_output{};unsigned short mux_packet{};
    void* video_async{};void* mux_async{};bool video_pending{},mux_pending{};
    std::array<std::uint8_t,16384> mux_buffer{};
    UsbMuxControl mux_control;
    std::thread mux_thread;
    std::atomic_bool mux_stop{},mux_failed{};
    std::mutex mux_error_mutex;std::string mux_error;
    std::mutex control_mutex;std::condition_variable control_ready;
    std::deque<std::uint8_t> control_in;
    std::deque<std::vector<std::uint8_t>> control_out;
    std::size_t control_out_bytes{};
    bool activated_by_us{};std::atomic_bool stopped{};std::mutex read_mutex,write_mutex;
    CapturePacketReader parser;std::array<std::uint8_t,16384> buffer{};std::size_t offset{},length{};
    std::function<void(const std::string&)> progress;
    ~Impl(){
        mux_stop.store(true);control_ready.notify_all();if(mux_thread.joinable())mux_thread.join();
        if(video_async){if(video_pending)api.cancel_async(video_async);api.free_async(&video_async);}
        if(mux_async){if(mux_pending)api.cancel_async(mux_async);api.free_async(&mux_async);}
        if(handle){if(interface_number>=0)api.release_interface(handle,interface_number);if(activated_by_us)api.control_msg(handle,0x40,0x52,0,0,nullptr,0,1000);api.close(handle);}
    }
    void close_for_reopen() {
        if(!handle)return;
        if(interface_number>=0)api.release_interface(handle,interface_number);
        interface_number=-1;api.close(handle);handle=nullptr;
    }
    std::vector<unsigned char> descriptor(unsigned type,unsigned index,unsigned size) {
        std::vector<unsigned char> result(size);
        const auto received=api.control_msg(handle,0x80,6,static_cast<int>((type<<8)|index),0,reinterpret_cast<char*>(result.data()),static_cast<int>(size),1000);
        api.checked(received,"Read live USB descriptor");
        if(received!=static_cast<int>(size))throw std::runtime_error("Truncated live USB descriptor.");return result;
    }
    void select_normal_configuration() {
        const auto device=descriptor(1,0,18);unsigned selected{};
        for(unsigned i=0;i<device[17];++i) {
            auto c=descriptor(2,i,9);const auto size=le16(c,2);
            if(size<9 || size>16384)throw std::runtime_error("Invalid normal USB configuration.");
            c=descriptor(2,i,size);
            for(std::size_t p=0;p<c.size();) {
                if(c.size()-p<2 || c[p]<2 || c[p]>c.size()-p)throw std::runtime_error("Malformed normal USB descriptor chain.");
                if(c[p+1]==4 && c[p]>=9 && c[p+4]==2 && c[p+5]==0xff && c[p+6]==0xfe && c[p+7]==2)
                    selected=std::max(selected,unsigned(c[5]));
                p+=c[p];
            }
        }
        if(!selected)throw std::runtime_error("No USBMux configuration is advertised by the iPhone.");
        api.checked(api.set_configuration(handle,static_cast<int>(selected)),"Select initial USBMux configuration");
        if(progress)progress("Selected initial USBMux configuration " + std::to_string(selected) + " before capture activation.");
    }
    void locate_mux(const std::vector<unsigned char>& config) {
        mux_interface=-1;std::vector<BulkEndpoint> endpoints;bool is_mux=false;int candidate=-1;
        auto finish=[&] {
            if(!is_mux || candidate<0)return;
            const auto pipes=select_capture_endpoints(endpoints);if(!pipes)return;
            mux_interface=candidate;mux_input=pipes->input;mux_output=pipes->output;
            for(const auto& e:endpoints)if(e.address==mux_output)mux_packet=e.maximum_packet_bytes;
        };
        for(std::size_t p=0;p<config.size();) {
            if(config.size()-p<2 || config[p]<2 || config[p]>config.size()-p)throw std::runtime_error("Invalid USBMux interface descriptor chain.");
            const auto n=config[p],type=config[p+1];
            if(type==4 && n>=9){finish();candidate=config[p+2];is_mux=config[p+4]==2 && config[p+5]==0xff && config[p+6]==0xfe && config[p+7]==2;endpoints.clear();}
            if(type==5 && n>=7 && is_mux)endpoints.push_back({config[p+2],le16(config,p+4),(config[p+3]&3)==2});
            p+=n;
        }
        finish();
    }
    int poll_async(void*& context,bool& submitted,unsigned char endpoint,std::span<std::uint8_t> bytes) {
        if(!context)api.checked(api.bulk_setup_async(handle,&context,endpoint),"Prepare persistent USB read");
        if(!submitted){api.checked(api.submit_async(context,reinterpret_cast<char*>(bytes.data()),static_cast<int>(bytes.size())),"Submit persistent USB read");submitted=true;}
        const auto read=api.reap_async_nocancel(context,200);
        if(read==-116)return 0; // Keep the URB alive; polling must not cancel other endpoints.
        api.checked(read,"Complete persistent USB read");submitted=false;
        if(read>static_cast<int>(bytes.size()))throw std::runtime_error("USB read exceeds its submitted buffer.");
        return read;
    }
    void send_mux(std::span<const std::uint8_t> bytes) {
        const auto sent=api.bulk_write(handle,mux_output,reinterpret_cast<char*>(const_cast<std::uint8_t*>(bytes.data())),static_cast<int>(bytes.size()),1000);
        api.checked(sent,"Write USBMux control");if(sent!=static_cast<int>(bytes.size()))throw std::runtime_error("Partial USBMux control write; session stopped.");
        if(needs_zero_length_transfer(bytes.size(),mux_packet))api.checked(api.bulk_write(handle,mux_output,nullptr,0,1000),"Terminate USBMux control write");
    }
    void pump_mux() {
        std::deque<std::vector<std::uint8_t>> outbound;
        {std::lock_guard lock(control_mutex);outbound.swap(control_out);control_out_bytes=0;}
        for(const auto& data:outbound)for(const auto& framed:mux_control.write_control_data(data))send_mux(framed);
        const auto received=poll_async(mux_async,mux_pending,mux_input,mux_buffer);if(!received)return;
        auto actions=mux_control.receive(std::span(mux_buffer).first(static_cast<std::size_t>(received)));
        for(const auto& p:actions.outgoing)send_mux(p);
        if(!actions.event.empty() && progress)progress(actions.event);
        auto control=mux_control.take_control_data();
        if(!control.empty()) {
            {std::lock_guard lock(control_mutex);
                if(control_in.size()+control.size()>1024U*1024U)throw std::runtime_error("USB control receive queue exceeded its bound.");
                control_in.insert(control_in.end(),control.begin(),control.end());}
            control_ready.notify_all();
        }
    }
    void initialize_mux(const std::atomic_bool& stop) {
        if(mux_interface<0)throw std::runtime_error("The capture configuration has no USBMux control interface.");
        send_mux(mux_control.start());
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);
        while(!stop.load() && !mux_control.ready()) {
            pump_mux();
            if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("The iPhone's USBMux/lockdownd initialization did not complete within 8 seconds.");
        }
        if(stop.load())return;
        mux_thread=std::thread([this] {
            try {while(!mux_stop.load())pump_mux();}
            catch(const std::exception& e){if(!mux_stop.load()){std::lock_guard lock(mux_error_mutex);mux_error=e.what();mux_failed.store(true);control_ready.notify_all();}}
        });
    }
    bool claim_capture() {
        const auto device=descriptor(1,0,18);const auto configurations=device[17];
        if(progress)progress("Direct libusb-win32: live device has " + std::to_string(configurations) + " configurations.");
        for(unsigned i=0;i<configurations;++i) {
            auto config=descriptor(2,i,9);const auto size=le16(config,2);
            if(size<9 || size>16384)throw std::runtime_error("Invalid USB configuration length.");
            config=descriptor(2,i,size);const auto config_id=config[5];
            locate_mux(config);
            bool capture=false;int candidate=-1,alternate=0;std::vector<BulkEndpoint> endpoints;
            auto finish=[&] {
                if(!capture || candidate<0)return false;
                auto selected=select_capture_endpoints(endpoints);if(!selected)return false;
                unsigned char active{};const auto got=api.control_msg(handle,0x80,8,0,0,reinterpret_cast<char*>(&active),1,1000);
                api.checked(got,"Read current USB configuration");
                if(active!=config_id)api.checked(api.set_configuration(handle,config_id),"Select direct USB capture configuration");
                // The observed driver path claims USBMux once and accesses both
                // pairs of endpoints through that owned device handle.
                const auto claim=mux_interface>=0?mux_interface:candidate;
                api.checked(api.claim_interface(handle,claim),"Claim combined USBMux/capture handle");interface_number=claim;
                if(claim==candidate && alternate)api.checked(api.set_altinterface(handle,alternate),"Select direct USB alternate interface");
                input=selected->input;output=selected->output;for(const auto& e:endpoints)if(e.address==output)packet_size=e.maximum_packet_bytes;
                api.checked(api.clear_halt(handle,input),"Clear capture input halt");api.checked(api.clear_halt(handle,output),"Clear capture output halt");
                if(progress)progress("Direct libusb-win32 capture interface claimed; configuration " + std::to_string(config_id) + ", interface " + std::to_string(candidate) + ".");
                // Original recorder 6519aa84...aae0a2, VA 0x1003dc90:
                // usb_control_msg(handle,0x40,0x40,0x6400,0x6400,null,0,1000).
                // This is separate from revealing the hidden USB configuration.
                const auto start=api.control_msg(handle,0x40,0x40,0x6400,0x6400,nullptr,0,1000);
                if(progress)progress("Original recorder USB startup control returned " + std::to_string(start) + ".");
                return true;
            };
            for(std::size_t p=0;p<config.size();) {
                if(config.size()-p<2 || config[p]<2 || config[p]>config.size()-p)throw std::runtime_error("Invalid USB descriptor chain.");
                const auto n=config[p],type=config[p+1];
                if(type==4 && n>=9) {
                    if(finish())return true;
                    candidate=config[p+2];alternate=config[p+3];capture=config[p+4]==2 && config[p+5]==0xff && config[p+6]==0x2a && config[p+7]==0xff;endpoints.clear();
                }
                if(type==5 && n>=7 && capture)endpoints.push_back({config[p+2],le16(config,p+4),(config[p+3]&3)==2});
                p+=n;
            }
            if(finish())return true;
        }
        return false;
    }
    void open_device() {
        std::lock_guard lock(api_enumeration_mutex);
        api.checked(api.find_busses(),"Find direct USB buses");api.checked(api.find_devices(),"Find direct USB devices");
        struct usb_device* chosen{};
        for(auto* bus=api.get_busses();bus;bus=bus->next)for(auto* device=bus->devices;device;device=device->next) {
            if(device->descriptor.idVendor!=0x05ac)continue;
            if(chosen)throw std::runtime_error("Connect one iPhone for this capture session.");chosen=device;
        }
        if(!chosen)throw std::runtime_error("No iPhone is bound to the installed libusb-win32 capture driver.");
        handle=api.open(chosen);if(!handle)throw std::runtime_error(std::string("Open direct iPhone USB: ")+api.strerror());
    }
};
LibUsbWin32CaptureTransport::LibUsbWin32CaptureTransport(std::unique_ptr<Impl> impl):impl_(std::move(impl)){}
std::unique_ptr<LibUsbWin32CaptureTransport> LibUsbWin32CaptureTransport::open_auto(const std::atomic_bool& stop,std::function<void(const std::string&)> progress) {
    auto impl=std::make_unique<Impl>();impl->progress=std::move(progress);impl->open_device();
    if(!impl->claim_capture()) {
        if(stop.load())return {};
        impl->select_normal_configuration();
        unsigned char active{};const auto got=impl->api.control_msg(impl->handle,0x80,8,0,0,reinterpret_cast<char*>(&active),1,1000);
        if(impl->progress)impl->progress("Direct USB configuration before activation: " + std::to_string(got==1?active:0) + ".");
        if(stop.load())return {};
        const auto result=impl->api.control_msg(impl->handle,0x40,0x52,0,2,nullptr,0,1000);
        if(impl->progress)impl->progress("Direct capture-mode control transfer returned " + std::to_string(result) + ".");
        impl->activated_by_us=true;
        impl->close_for_reopen();
        bool ready=false;
        for(unsigned i=0;i<20 && !stop.load();++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            try {impl->open_device();if(impl->claim_capture()){ready=true;break;}}catch(const std::exception& e){if(impl->progress)impl->progress(e.what());}
            impl->close_for_reopen();
        }
        if(stop.load())return {};
        if(!ready)throw std::runtime_error("Direct capture activation did not expose an FF/2A/FF interface.");
    }
    try {impl->initialize_mux(stop);}
    catch(const std::exception& e) {throw UsbControlError(std::string("iPhone USB control initialization failed: ")+e.what());}
    if(stop.load())return {};
    return std::unique_ptr<LibUsbWin32CaptureTransport>(new LibUsbWin32CaptureTransport(std::move(impl)));
}
LibUsbWin32CaptureTransport::~LibUsbWin32CaptureTransport(){request_stop();std::scoped_lock lock(impl_->read_mutex,impl_->write_mutex);}
void LibUsbWin32CaptureTransport::request_stop() noexcept{impl_->stopped.store(true);impl_->mux_stop.store(true);impl_->control_ready.notify_all();}
int LibUsbWin32CaptureTransport::control_read(std::span<std::uint8_t> bytes,std::chrono::milliseconds timeout) {
    std::unique_lock lock(impl_->control_mutex);
    impl_->control_ready.wait_for(lock,timeout,[&]{return !impl_->control_in.empty() || impl_->mux_stop.load() || impl_->mux_failed.load();});
    if(impl_->mux_failed.load()){std::lock_guard error_lock(impl_->mux_error_mutex);throw UsbControlError("USBMux control: "+impl_->mux_error);}
    if(impl_->mux_stop.load())return -1;
    const auto count=std::min(bytes.size(),impl_->control_in.size());
    for(std::size_t i=0;i<count;++i){bytes[i]=impl_->control_in.front();impl_->control_in.pop_front();}
    return static_cast<int>(count);
}
void LibUsbWin32CaptureTransport::control_write(std::span<const std::uint8_t> bytes) {
    std::lock_guard lock(impl_->control_mutex);
    if(impl_->mux_failed.load()){std::lock_guard error_lock(impl_->mux_error_mutex);throw UsbControlError("USBMux control: "+impl_->mux_error);}
    if(impl_->mux_stop.load())throw UsbControlError("The iPhone USB control connection is closed.");
    if(bytes.size()+impl_->control_out_bytes>1024U*1024U)throw UsbControlError("USB control transmit queue exceeds its bound.");
    impl_->control_out.emplace_back(bytes.begin(),bytes.end());impl_->control_out_bytes+=bytes.size();
}
std::optional<CapturePacket> LibUsbWin32CaptureTransport::read_packet(std::chrono::milliseconds wait) {
    std::lock_guard lock(impl_->read_mutex);const auto deadline=std::chrono::steady_clock::now()+wait;
    while(!impl_->stopped.load()) {
        if(impl_->mux_failed.load()){std::lock_guard error_lock(impl_->mux_error_mutex);throw std::runtime_error(impl_->mux_error);}
        if(impl_->length) {
            auto result=impl_->parser.consume(std::span(impl_->buffer).subspan(impl_->offset,impl_->length));impl_->offset+=result.consumed;impl_->length-=result.consumed;
            if(result.status==ParseStatus::PacketReady)return std::move(result.packet);
            if(result.status!=ParseStatus::NeedMore)throw std::runtime_error("Malformed direct USB capture packet.");
        }
        const auto received=impl_->poll_async(impl_->video_async,impl_->video_pending,impl_->input,impl_->buffer);
        impl_->offset=0;impl_->length=received>0?static_cast<std::size_t>(received):0;
        if(!impl_->length && std::chrono::steady_clock::now()>=deadline)return std::nullopt;
    }
    return std::nullopt;
}
void LibUsbWin32CaptureTransport::write_packet(std::span<const std::uint8_t> bytes) {
    if(bytes.empty() || bytes.size()>4U*1024U*1024U)throw std::invalid_argument("Invalid direct USB write size.");
    std::lock_guard lock(impl_->write_mutex);if(impl_->stopped.load())throw std::runtime_error("USB capture stopped.");
    const auto written=impl_->api.bulk_write(impl_->handle,impl_->output,reinterpret_cast<char*>(const_cast<std::uint8_t*>(bytes.data())),static_cast<int>(bytes.size()),1000);
    if(written!=static_cast<int>(bytes.size())){impl_->stopped.store(true);impl_->api.checked(written,"Write direct iPhone USB");throw std::runtime_error("Partial direct USB write; session stopped without replay.");}
    if(needs_zero_length_transfer(bytes.size(),impl_->packet_size))impl_->api.checked(impl_->api.bulk_write(impl_->handle,impl_->output,nullptr,0,1000),"Terminate direct USB write");
}
}
