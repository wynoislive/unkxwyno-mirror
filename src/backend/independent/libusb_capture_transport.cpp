#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <libusb.h>
#include "libusb_capture_transport.hpp"
#include <mutex>
#include <thread>
#include <stdexcept>
#include <string>
#include <sstream>

namespace unkxwyno::usb {
namespace {
class Api {
    HMODULE module_{};
    template<class T> void bind(T& fn,const char* name) {
        fn=reinterpret_cast<T>(GetProcAddress(module_,name));
        if(!fn) throw std::runtime_error(std::string("Missing libusb export: ")+name);
    }
public:
#define USB_API(name) decltype(&libusb_##name) name{};
    USB_API(init) USB_API(exit) USB_API(get_device_list) USB_API(free_device_list)
    USB_API(get_device_descriptor) USB_API(open) USB_API(close) USB_API(get_config_descriptor)
    USB_API(free_config_descriptor) USB_API(get_configuration) USB_API(set_configuration)
    USB_API(claim_interface) USB_API(release_interface) USB_API(set_interface_alt_setting)
    USB_API(control_transfer) USB_API(bulk_transfer) USB_API(error_name)
    USB_API(get_bus_number) USB_API(get_port_numbers)
#undef USB_API
    Api() {
        // Resolve only beside this DLL, never via PATH or the current directory.
        HMODULE self{};
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&module_anchor),&self)) throw std::runtime_error("Cannot locate the USB core module.");
        wchar_t path[32768]{};const auto count=GetModuleFileNameW(self,path,32768);
        if(!count || count>=32768) throw std::runtime_error("USB core module path is too long.");
        std::wstring library(path,count);library.resize(library.find_last_of(L"\\/")+1);library+=L"libusb-1.0.dll";
        module_=LoadLibraryExW(library.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!module_) throw std::runtime_error("The verified libusb-1.0.dll dependency is missing or cannot load.");
        try {
#define USB_BIND(name) bind(name,"libusb_" #name);
            USB_BIND(init) USB_BIND(exit) USB_BIND(get_device_list) USB_BIND(free_device_list)
            USB_BIND(get_device_descriptor) USB_BIND(open) USB_BIND(close) USB_BIND(get_config_descriptor)
            USB_BIND(free_config_descriptor) USB_BIND(get_configuration) USB_BIND(set_configuration)
            USB_BIND(claim_interface) USB_BIND(release_interface) USB_BIND(set_interface_alt_setting)
            USB_BIND(control_transfer) USB_BIND(bulk_transfer) USB_BIND(error_name)
            USB_BIND(get_bus_number) USB_BIND(get_port_numbers)
#undef USB_BIND
        }catch(...){FreeLibrary(module_);module_=nullptr;throw;}
    }
    ~Api(){if(module_)FreeLibrary(module_);}
    static void module_anchor() {}
    void checked(int result,const char* operation) const {if(result<0)throw std::runtime_error(std::string(operation)+": "+error_name(result));}
};
struct DeviceList {
    Api& api; libusb_device** devices{};
    ~DeviceList(){if(devices)api.free_device_list(devices,1);}
};
}
struct LibUsbCaptureTransport::Impl {
    Api api;
    libusb_context* context{};
    libusb_device_handle* handle{};
    int interface_number{-1};
    unsigned char input{},output{};
    std::uint16_t output_packet{};
    std::atomic_bool stopped{};
    std::mutex read_mutex,write_mutex;
    CapturePacketReader parser;
    std::array<std::uint8_t,16384> buffer{};
    std::size_t pending_offset{},pending_length{};
    unsigned char bus{};std::array<std::uint8_t,8> ports{};int port_count{};bool identity_set{};
    bool activated_by_us{};
    std::function<void(const std::string&)> progress;
    unsigned reported_configurations{};
    Impl(){api.checked(api.init(&context),"Initialize libusb");}
    ~Impl(){
        if(handle){
            if(interface_number>=0)api.release_interface(handle,interface_number);
            if(activated_by_us)api.control_transfer(handle,0x40,0x52,0,0,nullptr,0,1000);
            api.close(handle);
        }
        if(context)api.exit(context);
    }
    // Return the capture configuration without assuming configuration/interface/pipe numbers.
    bool find_capture(libusb_device* device,libusb_device_descriptor descriptor,int& config,int& interface,int& alternate) {
        for(std::uint8_t i=0;i<descriptor.bNumConfigurations;++i) {
            libusb_config_descriptor* c{};api.checked(api.get_config_descriptor(device,i,&c),"Read Apple USB configuration");
            struct Free{Api& a;libusb_config_descriptor* c;~Free(){a.free_config_descriptor(c);}} free{api,c};
            for(unsigned j=0;j<c->bNumInterfaces;++j) for(int k=0;k<c->interface[j].num_altsetting;++k) {
                const auto& alt=c->interface[j].altsetting[k];
                if(alt.bInterfaceClass!=0xff || alt.bInterfaceSubClass!=0x2a || alt.bInterfaceProtocol!=0xff || alt.bNumEndpoints!=2)continue;
                std::array<BulkEndpoint,2> endpoints{};
                for(unsigned e=0;e<2;++e){const auto& p=alt.endpoint[e];endpoints[e]={p.bEndpointAddress,p.wMaxPacketSize,(p.bmAttributes&3)==LIBUSB_TRANSFER_TYPE_BULK};}
                const auto selected=select_capture_endpoints(endpoints);if(!selected)continue;
                input=selected->input;output=selected->output;
                for(const auto& e:endpoints)if(e.address==output)output_packet=e.maximum_packet_bytes;
                config=c->bConfigurationValue;interface=alt.bInterfaceNumber;alternate=alt.bAlternateSetting;return true;
            }
        }
        return false;
    }
    bool open_device(bool activate) {
        DeviceList list{api};const auto count=api.get_device_list(context,&list.devices);
        if(count<0)api.checked(static_cast<int>(count),"Enumerate libusb devices");
        libusb_device* chosen{};libusb_device_descriptor descriptor{};
        for(std::ptrdiff_t i=0;i<count;++i) {
            libusb_device_descriptor d{};
            if(api.get_device_descriptor(list.devices[i],&d)<0 || d.idVendor!=0x05ac)continue;
            std::array<std::uint8_t,8> current_ports{};const auto pc=api.get_port_numbers(list.devices[i],current_ports.data(),8);
            if(identity_set && (pc!=port_count || current_ports!=ports || api.get_bus_number(list.devices[i])!=bus))continue;
            if(chosen)throw std::runtime_error("Connect one iPhone for USB capture; multiple Apple USB devices are present.");
            chosen=list.devices[i];descriptor=d;
        }
        if(!chosen)return false;
        if(descriptor.bNumConfigurations!=reported_configurations) {
            reported_configurations=descriptor.bNumConfigurations;
            if(progress)progress("Apple USB descriptor reports " + std::to_string(reported_configurations) + " configurations.");
        }
        if(!identity_set){bus=api.get_bus_number(chosen);port_count=api.get_port_numbers(chosen,ports.data(),8);if(port_count<1)throw std::runtime_error("Cannot identify the iPhone USB port for safe re-enumeration.");identity_set=true;}
        const auto opened=api.open(chosen,&handle);
        if(opened==LIBUSB_ERROR_ACCESS || opened==LIBUSB_ERROR_NOT_SUPPORTED || opened==LIBUSB_ERROR_NOT_FOUND || opened==LIBUSB_ERROR_IO)
            throw UsbDriverRequiredError(std::string("The detected Apple USB device cannot be opened for capture: ")+api.error_name(opened));
        api.checked(opened,"Open iPhone capture device with libusb");
        int config{},interface{},alternate{};
        if(find_capture(chosen,descriptor,config,interface,alternate)) {
            if(progress)progress("Found FF/2A/FF capture: configuration " + std::to_string(config) + ", interface " + std::to_string(interface) + ".");
            int active{};api.checked(api.get_configuration(handle,&active),"Read active USB configuration");
            if(active!=config)api.checked(api.set_configuration(handle,config),"Select iPhone capture configuration (requires a compatible capture driver)");
            api.checked(api.claim_interface(handle,interface),"Claim iPhone capture interface");interface_number=interface;
            api.checked(api.set_interface_alt_setting(handle,interface,alternate),"Select capture alternate setting");
            const auto start=api.control_transfer(handle,0x40,0x40,0x6400,0x6400,nullptr,0,1000);
            if(progress)progress("Recorder USB startup control result: " + std::to_string(start) + ".");
            return true;
        }
        if(activate) {
            const auto result=api.control_transfer(handle,0x40,0x52,0,2,nullptr,0,1000);
            if(progress)progress("QuickTime capture-mode control transfer result: " + std::to_string(result) + (result<0 ? std::string(" (")+api.error_name(result)+")" : ""));
            if(result<0 && result!=LIBUSB_ERROR_NO_DEVICE && result!=LIBUSB_ERROR_IO)api.checked(result,"Activate QuickTime USB capture");
            activated_by_us=true;
        }
        api.close(handle);handle=nullptr;return false;
    }
};
LibUsbCaptureTransport::LibUsbCaptureTransport(std::unique_ptr<Impl> impl):impl_(std::move(impl)){}
std::unique_ptr<LibUsbCaptureTransport> LibUsbCaptureTransport::open_auto(const std::atomic_bool& stop,std::function<void(const std::string&)> progress) {
    auto impl=std::make_unique<Impl>();
    impl->progress=std::move(progress);
    if(impl->open_device(true))return std::unique_ptr<LibUsbCaptureTransport>(new LibUsbCaptureTransport(std::move(impl)));
    for(unsigned i=0;i<40 && !stop.load();++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        // libusb caches device descriptors for a context. The vendor request
        // adds a configuration without necessarily generating a Windows PnP
        // arrival, so a new context is required to observe that configuration.
        impl->api.exit(impl->context);impl->context=nullptr;
        impl->api.checked(impl->api.init(&impl->context),"Refresh capture USB descriptors");
        if(impl->open_device(false))return std::unique_ptr<LibUsbCaptureTransport>(new LibUsbCaptureTransport(std::move(impl)));
    }
    if(stop.load())return {};
    throw std::runtime_error("The iPhone did not expose its USB capture configuration. A compatible driver is required.");
}
LibUsbCaptureTransport::~LibUsbCaptureTransport(){request_stop();std::scoped_lock lock(impl_->read_mutex,impl_->write_mutex);}
void LibUsbCaptureTransport::request_stop() noexcept {impl_->stopped.store(true);}
std::optional<CapturePacket> LibUsbCaptureTransport::read_packet(std::chrono::milliseconds wait) {
    std::lock_guard lock(impl_->read_mutex);const auto deadline=std::chrono::steady_clock::now()+wait;
    while(!impl_->stopped.load()) {
        if(impl_->pending_length) {
            auto parsed=impl_->parser.consume(std::span(impl_->buffer).subspan(impl_->pending_offset,impl_->pending_length));
            impl_->pending_offset+=parsed.consumed;impl_->pending_length-=parsed.consumed;
            if(parsed.status==ParseStatus::PacketReady)return std::move(parsed.packet);
            if(parsed.status!=ParseStatus::NeedMore)throw std::runtime_error("Malformed USB capture packet.");
        }
        int transferred{};
        const auto result=impl_->api.bulk_transfer(impl_->handle,impl_->input,impl_->buffer.data(),static_cast<int>(impl_->buffer.size()),&transferred,200);
        // libusb can return a timeout WITH bytes: preserve them before polling again.
        if(result<0 && result!=LIBUSB_ERROR_TIMEOUT)impl_->api.checked(result,"Read iPhone capture data");
        impl_->pending_offset=0;impl_->pending_length=static_cast<std::size_t>(transferred);
        if(!transferred && std::chrono::steady_clock::now()>=deadline)return std::nullopt;
    }
    return std::nullopt;
}
void LibUsbCaptureTransport::write_packet(std::span<const std::uint8_t> packet) {
    if(packet.empty() || packet.size()>4U*1024U*1024U)throw std::invalid_argument("USB capture packet exceeds write bounds.");
    std::lock_guard lock(impl_->write_mutex);if(impl_->stopped.load())throw std::runtime_error("USB capture stopped.");
    int transferred{};const auto result=impl_->api.bulk_transfer(impl_->handle,impl_->output,const_cast<unsigned char*>(packet.data()),static_cast<int>(packet.size()),&transferred,1000);
    if(result<0 || transferred!=static_cast<int>(packet.size())){impl_->stopped.store(true);impl_->api.checked(result,"Write iPhone capture packet");throw std::runtime_error("Partial USB capture write; packet was not replayed.");}
    if(needs_zero_length_transfer(packet.size(),impl_->output_packet))impl_->api.checked(impl_->api.bulk_transfer(impl_->handle,impl_->output,nullptr,0,&transferred,1000),"Terminate USB capture write");
}
}
