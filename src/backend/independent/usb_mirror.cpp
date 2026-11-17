#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <mftransform.h>
#include <wmcodecdsp.h>
#include <mmsystem.h>
#include <dwmapi.h>
#include "usb_mirror.h"
#include "apple_usb_devices.hpp"
#include "usb_capture_session.hpp"
#include "usb_video_converter.hpp"
#include "winusb_capture_transport.hpp"
#include "libusb_capture_transport.hpp"
#include "libusb_win32_capture_transport.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <functional>
#include <iomanip>
#include <mutex>
#include <memory>
#include <sstream>
#include <set>
#include <thread>

namespace {
using namespace unkxwyno::usb;
using namespace std::chrono_literals;
// CapturePacket::subtype() reads the little-endian wire word. This is the
// numeric value produced by tag("cvrp") in UsbCaptureSession.
constexpr std::uint32_t cvrp_tag=0x63767270U;
std::mutex capture_owner_mutex;
void checked(HRESULT result, const char* operation) {
    if (FAILED(result)) { std::ostringstream message; message << operation << " (0x" << std::hex << static_cast<unsigned long>(result) << ")"; throw std::runtime_error(message.str()); }
}
template<class T> class Com {
    T* value_{};
public:
    ~Com() { if(value_) value_->Release(); }
    Com() = default;
    Com(const Com&) = delete; Com& operator=(const Com&) = delete;
    T* operator->() const { return value_; }
    T* get() const { return value_; }
    T** put() { if(value_) value_->Release(); value_=nullptr; return &value_; }
};
class Decoder {
    Com<IMFTransform> decoder_;
    std::uint32_t width_{}, height_{};
    unsigned visible_x_{},visible_y_{},visible_width_{},visible_height_{};
    LONG stride_{};
    bool ready_{};
    void output_type() {
        for(DWORD i=0;;++i) {
            Com<IMFMediaType> type;
            checked(decoder_->GetOutputAvailableType(0,i,type.put()),"Enumerate decoder output");
            GUID subtype{}; checked(type->GetGUID(MF_MT_SUBTYPE,&subtype),"Decoder output subtype");
            if(subtype!=MFVideoFormat_NV12) continue;
            checked(decoder_->SetOutputType(0,type.get(),0),"Set NV12 decoder output");
            UINT64 size{}; checked(type->GetUINT64(MF_MT_FRAME_SIZE,&size),"Decoder dimensions");
            width_=static_cast<std::uint32_t>(size>>32); height_=static_cast<std::uint32_t>(size);
            if(!width_ || !height_ || width_>8192 || height_>8192 || width_%2 || height_%2) throw std::runtime_error("Invalid decoded video dimensions.");
            UINT32 stride{};
            if(SUCCEEDED(type->GetUINT32(MF_MT_DEFAULT_STRIDE,&stride))) stride_=static_cast<LONG>(stride);
            else stride_=static_cast<LONG>(width_);
            if(stride_<static_cast<LONG>(width_) || stride_>32768) throw std::runtime_error("Unsupported NV12 decoder stride.");
            visible_x_=visible_y_=0;visible_width_=width_;visible_height_=height_;
            MFVideoArea area{};UINT32 area_bytes{};
            auto aperture=type->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE,reinterpret_cast<UINT8*>(&area),sizeof(area),&area_bytes);
            if(aperture==MF_E_ATTRIBUTENOTFOUND)
                aperture=type->GetBlob(MF_MT_GEOMETRIC_APERTURE,reinterpret_cast<UINT8*>(&area),sizeof(area),&area_bytes);
            if(SUCCEEDED(aperture)) {
                if(area_bytes!=sizeof(area) || area.OffsetX.value<0 || area.OffsetY.value<0 ||
                   area.OffsetX.fract || area.OffsetY.fract || area.Area.cx<=0 || area.Area.cy<=0 ||
                   static_cast<unsigned>(area.OffsetX.value)+static_cast<unsigned>(area.Area.cx)>width_ ||
                   static_cast<unsigned>(area.OffsetY.value)+static_cast<unsigned>(area.Area.cy)>height_)
                    throw std::runtime_error("Unsupported decoder display aperture.");
                visible_x_=static_cast<unsigned>(area.OffsetX.value);visible_y_=static_cast<unsigned>(area.OffsetY.value);
                visible_width_=static_cast<unsigned>(area.Area.cx);visible_height_=static_cast<unsigned>(area.Area.cy);
            }
            return;
        }
    }
public:
    Decoder() {
        checked(CoCreateInstance(CLSID_CMSH264DecoderMFT,nullptr,CLSCTX_INPROC_SERVER,IID_IMFTransform,reinterpret_cast<void**>(decoder_.put())),"Create Windows H.264 decoder");
        Com<IMFAttributes> attributes;
        if(SUCCEEDED(decoder_->GetAttributes(attributes.put()))) attributes->SetUINT32(MF_LOW_LATENCY,TRUE);
    }
    ~Decoder() { if(decoder_.get()) decoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH,0); }
    void feed(const CapturedVideo& video, const std::function<void(std::vector<std::uint8_t>,unsigned,unsigned)>& present) {
        if(video.annex_b.empty() && !ready_) return;
        if(!ready_) {
            Com<IMFMediaType> type; checked(MFCreateMediaType(type.put()),"Create H.264 media type");
            checked(type->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video),"Set video type");
            checked(type->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_H264),"Set H.264 subtype");
            checked(type->SetUINT32(MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive),"Set progressive video");
            if(video.width && video.height) checked(type->SetUINT64(MF_MT_FRAME_SIZE,(UINT64(video.width)<<32)|video.height),"Set input dimensions");
            checked(decoder_->SetInputType(0,type.get(),0),"Configure Windows H.264 input");
            output_type();
            checked(decoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING,0),"Begin decoder stream");
            checked(decoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM,0),"Start decoder stream");
            ready_=true;
        }
        auto drain = [&] {
            for(unsigned attempts=0; attempts<64; ++attempts) {
                MFT_OUTPUT_STREAM_INFO info{}; checked(decoder_->GetOutputStreamInfo(0,&info),"Decoder buffer requirements");
                Com<IMFSample> output; Com<IMFMediaBuffer> buffer;
                if(!(info.dwFlags&MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)) {
                    checked(MFCreateSample(output.put()),"Allocate decoded sample");
                    checked(MFCreateMemoryBuffer(std::max(info.cbSize,static_cast<DWORD>(stride_)*height_*3/2),buffer.put()),"Allocate decoded frame");
                    checked(output->AddBuffer(buffer.get()),"Attach decoded buffer");
                }
                MFT_OUTPUT_DATA_BUFFER result{}; result.pSample=output.get(); DWORD status{};
                const auto hr=decoder_->ProcessOutput(0,1,&result,&status);
                if(result.pEvents) result.pEvents->Release();
                // MFT-provided samples carry a caller-owned reference.
                Com<IMFSample> provided;
                if(result.pSample && result.pSample!=output.get()) *provided.put()=result.pSample;
                if(hr==MF_E_TRANSFORM_NEED_MORE_INPUT) return;
                if(hr==MF_E_TRANSFORM_STREAM_CHANGE) { output_type(); continue; }
                checked(hr,"Decode H.264 frame");
                if(!result.pSample) throw std::runtime_error("Decoder returned no output sample.");
                Com<IMFMediaBuffer> contiguous; checked(result.pSample->ConvertToContiguousBuffer(contiguous.put()),"Read decoded image");
                BYTE* pixels{}; DWORD length{}; checked(contiguous->Lock(&pixels,nullptr,&length),"Lock decoded image");
                struct Unlock { IMFMediaBuffer* b; ~Unlock(){ b->Unlock(); } } unlock{contiguous.get()};
                const auto expected=static_cast<std::size_t>(stride_)*height_*3/2;
                if(length<expected) throw std::runtime_error("Decoded NV12 buffer is shorter than its dimensions.");
                std::vector<std::uint8_t> bgra(static_cast<std::size_t>(width_)*height_*4);
                nv12_to_bgra(std::span<const std::uint8_t>(pixels, length), width_, height_,
                             static_cast<unsigned>(stride_), bgra);
                if(visible_width_!=width_ || visible_height_!=height_ || visible_x_ || visible_y_) {
                    // Compact only the declared visible pixels. Never infer a
                    // crop from color: green can be legitimate phone content.
                    for(unsigned row=0;row<visible_height_;++row)
                        std::memmove(bgra.data()+static_cast<std::size_t>(row)*visible_width_*4,
                            bgra.data()+(static_cast<std::size_t>(row+visible_y_)*width_+visible_x_)*4,
                            static_cast<std::size_t>(visible_width_)*4);
                    bgra.resize(static_cast<std::size_t>(visible_width_)*visible_height_*4);
                }
                present(std::move(bgra),visible_width_,visible_height_);
            }
            throw std::runtime_error("Decoder exceeded its output work bound.");
        };
        if(video.annex_b.empty()) {
            checked(decoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM,0),"End recorded stream");
            checked(decoder_->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN,0),"Drain recorded frames");
            drain();return;
        }
        Com<IMFSample> sample; Com<IMFMediaBuffer> input;
        checked(MFCreateSample(sample.put()),"Create video sample");
        checked(MFCreateMemoryBuffer(static_cast<DWORD>(video.annex_b.size()),input.put()),"Allocate H.264 sample");
        BYTE* memory{}; checked(input->Lock(&memory,nullptr,nullptr),"Lock H.264 sample");
        std::memcpy(memory,video.annex_b.data(),video.annex_b.size()); input->Unlock();
        checked(input->SetCurrentLength(static_cast<DWORD>(video.annex_b.size())),"Set sample length");
        checked(sample->AddBuffer(input.get()),"Attach H.264 sample");
        sample->SetSampleTime(video.time_100ns); sample->SetSampleDuration(166667);
        auto hr=decoder_->ProcessInput(0,sample.get(),0);
        if(hr==MF_E_NOTACCEPTING) {drain(); hr=decoder_->ProcessInput(0,sample.get(),0);}
        checked(hr,"Submit H.264 sample"); drain();
    }
};
class DecoderWorker {
    struct Sample { CapturedVideo video; std::chrono::steady_clock::time_point received_at; };
    // Keep at most one compressed frame waiting behind the active decode. At
    // 60 FPS this bounds application-side queue delay to one frame while still
    // allowing the USB reader to continue during a short decoder spike.
    static constexpr std::size_t capacity=1;
    mutable std::mutex mutex_;
    std::condition_variable sample_ready_,space_ready_;
    std::deque<Sample> samples_;
    bool stopping_{};
    std::string error_;
    std::function<void(std::vector<std::uint8_t>,unsigned,unsigned,std::chrono::steady_clock::time_point)> present_;
    std::function<void(double)> duration_;
    std::thread thread_;
    void fail(std::string error) {
        {std::lock_guard lock(mutex_);error_=std::move(error);stopping_=true;samples_.clear();}
        sample_ready_.notify_all();space_ready_.notify_all();
    }
    void run() {
        try {
            checked(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"Initialize decoder COM");
            struct CoClose{~CoClose(){CoUninitialize();}} co_close;
            checked(MFStartup(MF_VERSION,MFSTARTUP_LITE),"Initialize Windows media decoder");
            struct MfClose{~MfClose(){MFShutdown();}} mf_close;
            Decoder decoder;
            for(;;) {
                Sample sample;
                {
                    std::unique_lock lock(mutex_);
                    sample_ready_.wait(lock,[&]{return stopping_ || !samples_.empty();});
                    if(stopping_ || samples_.empty()) return;
                    sample=std::move(samples_.front());samples_.pop_front();
                }
                space_ready_.notify_one();
                const auto started=std::chrono::steady_clock::now();
                decoder.feed(sample.video,[&](std::vector<std::uint8_t> pixels,unsigned w,unsigned h) {
                    present_(std::move(pixels),w,h,sample.received_at);
                });
                duration_(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());
            }
        } catch(const std::exception& e) {fail(e.what());}
        catch(...) {fail("The H.264 decoder failed with an unknown error.");}
    }
public:
    DecoderWorker(
        std::function<void(std::vector<std::uint8_t>,unsigned,unsigned,std::chrono::steady_clock::time_point)> present,
        std::function<void(double)> duration)
        :present_(std::move(present)),duration_(std::move(duration)),thread_([this]{run();}) {}
    DecoderWorker(const DecoderWorker&)=delete;
    DecoderWorker& operator=(const DecoderWorker&)=delete;
    ~DecoderWorker(){stop();}
    void submit(CapturedVideo video,std::chrono::steady_clock::time_point received_at,const std::atomic_bool& cancel) {
        std::unique_lock lock(mutex_);
        while(!stopping_ && samples_.size()>=capacity && !cancel.load())
            space_ready_.wait_for(lock,std::chrono::milliseconds(5));
        if(cancel.load())return;
        if(stopping_)throw std::runtime_error(error_.empty()?"The H.264 decoder worker stopped.":error_);
        samples_.push_back({std::move(video),received_at});
        sample_ready_.notify_one();
    }
    std::string error() const {
        std::lock_guard lock(mutex_);
        return error_;
    }
    void stop() {
        {std::lock_guard lock(mutex_);stopping_=true;samples_.clear();}
        sample_ready_.notify_all();space_ready_.notify_all();
        if(thread_.joinable())thread_.join();
    }
};
class VideoNeedScheduler {
    std::atomic_bool stopping_{};
    mutable std::mutex mutex_;
    std::string error_;
    std::thread thread_;
    void fail(std::string error) {
        std::lock_guard lock(mutex_);error_=std::move(error);stopping_.store(true);
    }
public:
    VideoNeedScheduler(UsbCaptureTransport& transport,std::vector<std::uint8_t> packet,
                       unsigned target_fps,const std::atomic_bool& cancel) {
        if(target_fps<24 || target_fps>120)throw std::invalid_argument("USB target frame rate must be between 24 and 120 FPS.");
        thread_=std::thread([this,&transport,packet=std::move(packet),target_fps,&cancel]() {
            const auto interval=std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(1.0/target_fps));
            auto deadline=std::chrono::steady_clock::now()+interval;
            while(!stopping_.load() && !cancel.load()) {
                std::this_thread::sleep_until(deadline);
                if(stopping_.load() || cancel.load())break;
                try {transport.write_packet(packet);}
                catch(const std::exception& e){fail(e.what());break;}
                deadline+=interval;
                const auto now=std::chrono::steady_clock::now();
                if(deadline<now)deadline=now+interval;
            }
        });
    }
    VideoNeedScheduler(const VideoNeedScheduler&)=delete;
    VideoNeedScheduler& operator=(const VideoNeedScheduler&)=delete;
    ~VideoNeedScheduler(){stop();}
    std::string error() const {std::lock_guard lock(mutex_);return error_;}
    void stop(){stopping_.store(true);if(thread_.joinable())thread_.join();}
};
class Audio {
    struct Block { WAVEHDR header{}; std::vector<std::uint8_t> data; };
    HWAVEOUT device_{};
    std::deque<std::unique_ptr<Block>> pending_;
    unsigned rate_{}, channels_{};
public:
    ~Audio(){ close(); }
    void close() {
        if(!device_) return;
        waveOutReset(device_);
        for(auto& b:pending_) waveOutUnprepareHeader(device_,&b->header,sizeof(WAVEHDR));
        pending_.clear(); waveOutClose(device_); device_=nullptr;
    }
    void feed(const CaptureActions& a) {
        if(a.pcm.empty()) return;
        if(device_ && (rate_!=a.audio_rate || channels_!=a.audio_channels)) close();
        if(!device_) {
            WAVEFORMATEX f{}; f.wFormatTag=WAVE_FORMAT_PCM; f.nChannels=static_cast<WORD>(a.audio_channels);
            f.nSamplesPerSec=a.audio_rate; f.wBitsPerSample=16; f.nBlockAlign=static_cast<WORD>(f.nChannels*2); f.nAvgBytesPerSec=f.nSamplesPerSec*f.nBlockAlign;
            if(waveOutOpen(&device_,WAVE_MAPPER,&f,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR) throw std::runtime_error("Windows could not open PCM audio playback.");
            rate_=a.audio_rate; channels_=a.audio_channels;
        }
        while(!pending_.empty() && (pending_.front()->header.dwFlags&WHDR_DONE)) {
            waveOutUnprepareHeader(device_,&pending_.front()->header,sizeof(WAVEHDR)); pending_.pop_front();
        }
        std::size_t queued{}; for(const auto& b:pending_) queued+=b->data.size();
        // Bound audible backlog to 250 ms; discard stale audio after a stalled output device.
        if(queued>static_cast<std::size_t>(rate_)*channels_/2) {
            waveOutReset(device_); for(auto& b:pending_) waveOutUnprepareHeader(device_,&b->header,sizeof(WAVEHDR)); pending_.clear();
        }
        if(a.pcm.size()%(channels_*2)) throw std::runtime_error("Unaligned PCM audio sample.");
        auto b=std::make_unique<Block>(); b->data=a.pcm;
        b->header.lpData=reinterpret_cast<LPSTR>(b->data.data()); b->header.dwBufferLength=static_cast<DWORD>(b->data.size());
        if(waveOutPrepareHeader(device_,&b->header,sizeof(WAVEHDR))!=MMSYSERR_NOERROR) throw std::runtime_error("Prepare USB audio playback failed.");
        if(waveOutWrite(device_,&b->header,sizeof(WAVEHDR))!=MMSYSERR_NOERROR) {waveOutUnprepareHeader(device_,&b->header,sizeof(WAVEHDR)); throw std::runtime_error("USB audio playback failed.");}
        pending_.push_back(std::move(b));
    }
};
struct Mirror {
    struct DecodedFrame {
        std::vector<std::uint8_t> pixels;
        unsigned width{}, height{};
        std::uint64_t generation{};
        std::chrono::steady_clock::time_point received_at{};
    };
    unsigned requested_width,requested_height,target_fps;
    bool actual_size{};
    std::atomic_bool stop{}, failed{}, paint_pending{}, running{};
    bool driver_required{};
    bool pairing_enabled{};
    std::atomic_int authenticated{};
    std::mutex auth_mutex;std::string auth_error;
    std::atomic<HWND> window{};
    std::mutex transport_mutex, frame_mutex;
    UsbCaptureTransport* active{};
    std::shared_ptr<const DecodedFrame> frame;
    std::uint64_t frame_generation{}, painted_generation{}, previous_painted{};
    std::atomic<std::uint64_t> painted{}, replaced_before_paint{};
    std::atomic<double> last_receive_to_paint_ms{},last_decode_ms{},source_pts_fps{};
    std::chrono::steady_clock::time_point previous_tick{std::chrono::steady_clock::now()};
    uxm_usb_status_callback callback{}; void* context{};
    void status(int state,const char* message,double fps=0) {
        unsigned w{},h{}; std::uint64_t count{};
        {std::lock_guard lock(frame_mutex);if(frame){w=frame->width;h=frame->height;}count=painted.load();}
        if(callback) callback(context,state,message,w,h,count,fps);
    }
    void halt() {
        stop.store(true);
        // The reader polls at 200 ms. Let its owner send HPA0/HPD0 before
        // releasing the interface, rather than aborting both pipes immediately.
    }
    std::unique_ptr<UsbCaptureTransport> connect() {
        auto paths=apple_usb_interfaces();
        for(const auto& path:paths) {try{return WinUsbCaptureTransport::open(path);}catch(const std::exception&) {}}
        try {return LibUsbWin32CaptureTransport::open_auto(stop,[this](const std::string& message){status(1,message.c_str());});}
        catch(const UsbControlError&) {throw;}
        catch(const std::exception& e) {status(1,e.what());}
        // A whole-device libusb0/libusbK binding can select the hidden capture
        // configuration; WinUSB cannot switch USB configurations on all devices.
        try {return LibUsbCaptureTransport::open_auto(stop,[this](const std::string& message){status(1,message.c_str());});}
        catch(const UsbDriverRequiredError&) {driver_required=true;throw;}
        catch(const std::exception& e) {if(paths.empty())throw;status(1,e.what());}
        bool activated=false; std::string error;
        for(const auto& path:paths) {
            if(stop.load()) return {};
            try { enable_usb_capture_mode(path); activated=true; break; } catch(const std::exception& e){error=e.what();}
        }
        if(!activated) {
            driver_required=true;
            throw std::runtime_error("The installed Apple USB driver blocks capture access. " + error + " Temporary capture-driver access is required.");
        }
        status(1,"Waiting for the iPhone capture interface to re-enumerate.");
        for(unsigned i=0;i<40 && !stop.load();++i) {
            std::this_thread::sleep_for(250ms);
            for(const auto& path:apple_usb_interfaces()) {try{return WinUsbCaptureTransport::open(path);}catch(const std::exception& e){error=e.what();}}
        }
        if(stop.load()) return {};
        throw std::runtime_error("Capture mode did not expose an accessible FF/2A/FF interface. " + error);
    }
    void capture() {
        try {
            auto transport=connect(); if(!transport) return;
            {std::lock_guard lock(transport_mutex); active=transport.get(); if(stop.load()) active->request_stop();}
            struct Clear { Mirror* m; ~Clear(){std::lock_guard lock(m->transport_mutex);m->active=nullptr;} } clear{this};
            if(pairing_enabled) {
                if(!transport->has_control_stream())throw UsbControlError("Authenticated USB capture requires the combined USBMux adapter.");
                status(5,"Establishing this app's trusted iPhone session. Approve Trust on the unlocked phone if prompted.");
                const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(90);
                while(authenticated.load()==0 && !stop.load()) {
                    std::this_thread::sleep_for(50ms);
                    if(std::chrono::steady_clock::now()>deadline)throw UsbControlError("iPhone trust/session initialization timed out after 90 seconds.");
                }
                if(stop.load())return;
                if(authenticated.load()!=1){std::lock_guard lock(auth_mutex);throw UsbControlError(auth_error.empty()?"iPhone trusted-session initialization failed.":auth_error);}
            }
            UsbCaptureSession protocol(requested_width,requested_height); Audio audio;
            DecoderWorker decoder([this](std::vector<std::uint8_t> pixels,unsigned w,unsigned h,std::chrono::steady_clock::time_point received_at) {
                auto decoded=std::make_shared<DecodedFrame>();
                decoded->pixels=std::move(pixels);decoded->width=w;decoded->height=h;decoded->received_at=received_at;
                {
                    std::lock_guard lock(frame_mutex);
                    if(frame && frame->generation>painted_generation) ++replaced_before_paint;
                    decoded->generation=++frame_generation;
                    frame=std::move(decoded);
                }
                if(!paint_pending.exchange(true)) PostMessageW(window.load(),WM_APP+1,0,0);
            },[this](double elapsed_ms){last_decode_ms.store(elapsed_ms);});
            std::unique_ptr<VideoNeedScheduler> need_scheduler;
            status(1,"USB capture interface opened; waiting for the iPhone handshake.");
            auto last_video=std::chrono::steady_clock::now();
            std::set<std::uint32_t> seen_sync;
            std::deque<std::int64_t> recent_source_pts;
            while(!stop.load()) {
                if(pairing_enabled && authenticated.load()!=1) {
                    std::lock_guard lock(auth_mutex);throw UsbControlError(auth_error.empty()?"The trusted iPhone session ended.":auth_error);
                }
                if(const auto error=decoder.error();!error.empty())throw std::runtime_error(error);
                if(need_scheduler)if(const auto error=need_scheduler->error();!error.empty())throw std::runtime_error("The video request pump failed: "+error);
                auto packet=transport->read_packet();
                if(const auto error=decoder.error();!error.empty())throw std::runtime_error(error);
                if(need_scheduler)if(const auto error=need_scheduler->error();!error.empty())throw std::runtime_error("The video request pump failed: "+error);
                if(!packet) {
                    if(std::chrono::steady_clock::now()-last_video>15s && !stop.load())
                        throw std::runtime_error("The iPhone capture handshake or video stream timed out after 15 seconds.");
                    continue;
                }
                if(packet->family==PacketFamily::Sync && packet->subtype() && seen_sync.insert(*packet->subtype()).second) {
                    const auto kind=*packet->subtype();
                    char code[5]{static_cast<char>(kind>>24),static_cast<char>(kind>>16),static_cast<char>(kind>>8),static_cast<char>(kind),0};
                    status(1,(std::string("Received iPhone USB SYNC ")+code+".").c_str());
                }
                auto actions=protocol.receive(*packet);
                for(const auto& bytes:actions.outgoing) transport->write_packet(bytes);
                if(packet->family==PacketFamily::Sync && packet->subtype()==cvrp_tag && !need_scheduler)
                    need_scheduler=std::make_unique<VideoNeedScheduler>(*transport,protocol.video_need(),target_fps,stop);
                if(!actions.event.empty()) status(1,actions.event.c_str());
                if(actions.video) {
                    const auto received_at=std::chrono::steady_clock::now();
                    if(actions.video->timestamp_valid) {
                        const auto pts=actions.video->time_100ns;
                        if(!recent_source_pts.empty() && pts<=recent_source_pts.back())recent_source_pts.clear();
                        recent_source_pts.push_back(pts);
                        if(recent_source_pts.size()>31)recent_source_pts.pop_front();
                        if(recent_source_pts.size()>1) {
                            const auto delta=recent_source_pts.back()-recent_source_pts.front();
                            if(delta>0)source_pts_fps.store(10000000.0*double(recent_source_pts.size()-1)/double(delta));
                        }
                    }
                    decoder.submit(std::move(*actions.video),received_at,stop);
                    last_video=std::chrono::steady_clock::now();
                }
                audio.feed(actions);
                if(std::chrono::steady_clock::now()-last_video>15s) throw std::runtime_error("The iPhone sent no video samples for 15 seconds. Stop and reconnect USB capture.");
            }
            if(need_scheduler)need_scheduler->stop();
            decoder.stop();
            for(const auto& bytes:protocol.shutdown_packets()) transport->write_packet(bytes);
        } catch(const std::exception& e) {
            if(!stop.load()) {failed.store(true); status(driver_required?4:3,e.what());}
        }
        PostMessageW(window.load(),WM_CLOSE,0,0);
    }
};
LRESULT CALLBACK viewer(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    auto* m=reinterpret_cast<Mirror*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE) {m=static_cast<Mirror*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(m));}
    if(!m) return DefWindowProcW(window,message,wparam,lparam);
    switch(message) {
    case WM_CREATE: {
        const wchar_t* labels[]={L"Minimize",L"Maximize / Restore",L"Fit / Actual size",L"Close"};
        const int widths[]={90,150,130,70};int x=6;
        for(int i=0;i<4;++i) {
            auto button=CreateWindowExW(0,L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
                x,4,widths[i],28,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(101+i)),GetModuleHandleW(nullptr),nullptr);
            SendMessageW(button,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
            x+=widths[i]+4;
        }
        return 0;
    }
    case WM_COMMAND:
        switch(LOWORD(wparam)) {
        case 101: ShowWindow(window,SW_MINIMIZE);return 0;
        case 102: ShowWindow(window,IsZoomed(window)?SW_RESTORE:SW_MAXIMIZE);return 0;
        case 103: m->actual_size=!m->actual_size;InvalidateRect(window,nullptr,FALSE);return 0;
        case 104: PostMessageW(window,WM_CLOSE,0,0);return 0;
        }
        break;
    case WM_SIZE: InvalidateRect(window,nullptr,FALSE);return 0;
    case WM_GETMINMAXINFO:
        reinterpret_cast<MINMAXINFO*>(lparam)->ptMinTrackSize={490,240};return 0;
    case WM_APP+1: m->paint_pending.store(false); InvalidateRect(window,nullptr,FALSE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{}; auto dc=BeginPaint(window,&paint); RECT rect{};GetClientRect(window,&rect);
        RECT toolbar{0,0,rect.right,36};FillRect(dc,&toolbar,GetSysColorBrush(COLOR_BTNFACE));
        rect.top=36;
        std::shared_ptr<const Mirror::DecodedFrame> frame;
        {std::lock_guard lock(m->frame_mutex);frame=m->frame;}
        if(frame && !frame->pixels.empty()) {
            BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=static_cast<LONG>(frame->width);
            info.bmiHeader.biHeight=-static_cast<LONG>(frame->height);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
            const auto fit=std::min(double(rect.right)/frame->width,double(std::max(0L,rect.bottom-rect.top))/frame->height);
            const auto scale=m->actual_size?std::min(1.0,fit):fit;
            const auto w=static_cast<int>(frame->width*scale),h=static_cast<int>(frame->height*scale);
            const auto x=(rect.right-w)/2,y=rect.top+(rect.bottom-rect.top-h)/2;
            // Keep the previous image visible until its replacement is ready.
            // Clearing the full client area here exposed a black frame on every
            // update and made otherwise continuous capture visibly flicker.
            const auto black=static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
            RECT bar{rect.left,rect.top,rect.right,y}; if(bar.bottom>bar.top)FillRect(dc,&bar,black);
            bar={rect.left,y+h,rect.right,rect.bottom}; if(bar.bottom>bar.top)FillRect(dc,&bar,black);
            bar={rect.left,y,x,y+h}; if(bar.right>bar.left)FillRect(dc,&bar,black);
            bar={x+w,y,rect.right,y+h}; if(bar.right>bar.left)FillRect(dc,&bar,black);
            SetStretchBltMode(dc,COLORONCOLOR);
            const auto drawn=StretchDIBits(dc,x,y,w,h,0,0,static_cast<int>(frame->width),static_cast<int>(frame->height),frame->pixels.data(),&info,DIB_RGB_COLORS,SRCCOPY);
            if(drawn!=static_cast<int>(GDI_ERROR) && drawn!=0) {
                const auto now=std::chrono::steady_clock::now();
                m->last_receive_to_paint_ms.store(std::chrono::duration<double,std::milli>(now-frame->received_at).count());
                std::lock_guard lock(m->frame_mutex);
                if(m->painted_generation!=frame->generation) {++m->painted;m->painted_generation=frame->generation;}
            }
        } else {FillRect(dc,&rect,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));SetTextColor(dc,RGB(230,230,230));SetBkMode(dc,TRANSPARENT);DrawTextW(dc,L"Starting iPhone USB capture...",-1,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
        EndPaint(window,&paint); return 0;
    }
    case WM_TIMER: {
        const auto now=std::chrono::steady_clock::now(); double fps{}; std::uint64_t count{};
        {std::lock_guard lock(m->frame_mutex);count=m->painted.load();fps=double(count-m->previous_painted)/std::chrono::duration<double>(now-m->previous_tick).count();m->previous_painted=count;}
        m->previous_tick=now;
        if(count) {
            std::ostringstream message;
            message << std::fixed << std::setprecision(1)
                    << "iPhone USB video: target " << m->target_fps << " FPS; ";
            const auto source_fps=m->source_pts_fps.load();
            if(source_fps>0) message << "device PTS " << source_fps << " FPS; ";
            else message << "device PTS unavailable; ";
            message << "displayed " << fps << " FPS; " << nv12_converter_name()
                    << " NV12; decode+convert " << m->last_decode_ms.load()
                    << " ms; USB sample to paint " << m->last_receive_to_paint_ms.load()
                    << " ms; replaced before paint " << m->replaced_before_paint.load();
            m->status(2,message.str().c_str(),fps);
        }
        return 0;
    }
    case WM_CLOSE: m->halt(); DestroyWindow(window); return 0;
    case WM_DESTROY: KillTimer(window,1);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(window,message,wparam,lparam);
}
}
extern "C" {
void* uxm_usb_mirror_create(unsigned width,unsigned height,unsigned target_fps) {
    if(!width || !height || width>8192 || height>8192 || target_fps<24 || target_fps>120) return nullptr;
    try {auto* m=new Mirror; m->requested_width=width;m->requested_height=height;m->target_fps=target_fps;return m;}catch(...){return nullptr;}
}
int uxm_usb_mirror_run(void* session,uxm_usb_status_callback callback,void* context) {
    if(!session) return -1;
    auto* m=static_cast<Mirror*>(session); if(m->running.exchange(true)) return -1;
    m->callback=callback; m->context=context;
    std::unique_lock owner(capture_owner_mutex,std::try_to_lock);
    if(!owner.owns_lock()){m->status(3,"An independent USB capture session is already running.");return 1;}
    const auto instance=GetModuleHandleW(nullptr);
    WNDCLASSW type{};type.lpfnWndProc=viewer;type.hInstance=instance;type.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));type.lpszClassName=L"UnkxwynoIndependentUsbViewer";
    if(!RegisterClassW(&type) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) {m->status(3,"Cannot register USB viewer window.");return 1;}
    constexpr DWORD window_style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_THICKFRAME|WS_CLIPCHILDREN;
    auto window=CreateWindowExW(0,type.lpszClassName,L"UNKXWYNO Mirror - iPhone USB",window_style,CW_USEDEFAULT,CW_USEDEFAULT,650,900,nullptr,nullptr,instance,m);
    if(!window){m->status(3,"Cannot create USB viewer window.");return 1;}
    // Keep normal caption controls while removing the Windows 11 accent-color
    // outline that can appear green around a newly-created capture window.
    constexpr DWORD dwmwa_border_color=34;
    constexpr COLORREF dwm_color_none=0xFFFFFFFE;
    DwmSetWindowAttribute(window,dwmwa_border_color,&dwm_color_none,sizeof(dwm_color_none));
    m->window.store(window);ShowWindow(window,SW_SHOW);SetTimer(window,1,1000,nullptr);
    m->status(1,"Opening iPhone USB capture.");
    std::thread worker;
    try{worker=std::thread([m]{m->capture();});}catch(...){DestroyWindow(window);return 1;}
    MSG message{};
    while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
    m->halt();worker.join();m->window.store(nullptr);
    if(!m->failed.load())m->status(0,"USB mirroring stopped.");
    return m->failed.load()?1:0;
}
void uxm_usb_mirror_stop(void* session) {if(session){auto* m=static_cast<Mirror*>(session);m->halt();if(auto window=m->window.load())PostMessageW(window,WM_CLOSE,0,0);}}
void uxm_usb_mirror_destroy(void* session) {delete static_cast<Mirror*>(session);}
int uxm_usb_mirror_enable_pairing(void* session) {
    if(!session)return -1;auto* m=static_cast<Mirror*>(session);if(m->running.load())return -1;m->pairing_enabled=true;return 0;
}
int uxm_usb_mirror_control_read(void* session,unsigned char* bytes,unsigned capacity) {
    if(!session || !bytes || !capacity || capacity>1024U*1024U)return -2;
    try{auto* m=static_cast<Mirror*>(session);std::lock_guard lock(m->transport_mutex);if(!m->active || m->stop.load())return -2;return m->active->control_read(std::span(bytes,capacity),200ms);}
    catch(const std::exception& e){static_cast<Mirror*>(session)->status(1,(std::string("USB control read error: ")+e.what()).c_str());return -2;}
}
int uxm_usb_mirror_control_write(void* session,const unsigned char* bytes,unsigned length) {
    if(!session || !bytes || !length || length>1024U*1024U)return -1;
    try{auto* m=static_cast<Mirror*>(session);std::lock_guard lock(m->transport_mutex);if(!m->active || m->stop.load())return -1;m->active->control_write(std::span(bytes,length));return static_cast<int>(length);}
    catch(const std::exception& e){static_cast<Mirror*>(session)->status(1,(std::string("USB control write error: ")+e.what()).c_str());return -1;}
}
void uxm_usb_mirror_auth_result(void* session,int trusted,const char* error) {
    if(!session)return;auto* m=static_cast<Mirror*>(session);{std::lock_guard lock(m->auth_mutex);m->auth_error=error?error:"";}m->authenticated.store(trusted==1?1:-1);
}
int uxm_usb_decode_recording(const unsigned char* bytes,unsigned length,uxm_usb_decode_report* report) {
    if(!bytes || !length || length>32U*1024U*1024U || !report) return -1;
    *report={};
    try {
        checked(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"Initialize diagnostic COM");
        struct CoClose{~CoClose(){CoUninitialize();}} co_close;
        checked(MFStartup(MF_VERSION,MFSTARTUP_LITE),"Initialize diagnostic decoder");
        struct MfClose{~MfClose(){MFShutdown();}} mf_close;
        UsbCaptureSession session;CapturePacketReader reader;Decoder decoder;
        auto decoded=[&](std::vector<std::uint8_t> pixels,unsigned w,unsigned h) {
            report->width=w;report->height=h;++report->decoded_frames;
            unsigned long long hash=14695981039346656037ULL;
            for(auto pixel:pixels) {hash^=pixel;hash*=1099511628211ULL;}
            report->pixel_checksum=hash;
        };
        std::span<const std::uint8_t> input(bytes,length);
        while(!input.empty()) {
            auto result=reader.consume(input); input=input.subspan(result.consumed);
            if(result.status==ParseStatus::NeedMore) break;
            if(result.status!=ParseStatus::PacketReady || !result.packet) return -2;
            auto actions=session.receive(*result.packet);
            if(actions.video) decoder.feed(*actions.video,decoded);
        }
        decoder.feed(CapturedVideo{},decoded);
        return reader.buffered_bytes()==0 && report->decoded_frames>0 ? 0 : -3;
    } catch(const std::exception& e) {OutputDebugStringA(e.what());return -4;}
}
}
