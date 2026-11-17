#include "apple_usb_devices.hpp"
#include "winusb_capture_transport.hpp"
#include "usb_mirror.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
int main(int argc, char** argv) {
    try {
        if(argc==3 && std::string(argv[1])=="--replay") {
            std::ifstream stream(argv[2],std::ios::binary|std::ios::ate);
            if(!stream || stream.tellg()<=0 || stream.tellg()>32*1024*1024) throw std::runtime_error("Invalid recording file.");
            std::vector<unsigned char> bytes(static_cast<std::size_t>(stream.tellg())); stream.seekg(0);stream.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
            uxm_usb_decode_report report{};const auto result=uxm_usb_decode_recording(bytes.data(),static_cast<unsigned>(bytes.size()),&report);
            std::cout << "Decoder result=" << result << " frames=" << report.decoded_frames << " dimensions=" << report.width << 'x' << report.height << " pixel_checksum=" << report.pixel_checksum << std::endl;
            return result==0?0:1;
        }
        if(argc==2 && std::string(argv[1])=="--mirror") {
            auto* session=uxm_usb_mirror_create(3840,2160,60); if(!session) return 1;
            const auto result=uxm_usb_mirror_run(session,[](void*,int state,const char* message,unsigned w,unsigned h,unsigned long long frames,double fps){std::cout << "state=" << state << " " << message << " dimensions=" << w << 'x' << h << " displayed_frames=" << frames << " displayed_fps=" << fps << std::endl;},nullptr);
            uxm_usb_mirror_destroy(session);return result;
        }
        const bool activate = argc == 2 && std::string(argv[1]) == "--activate";
        auto paths = unkxwyno::usb::apple_usb_interfaces();
        std::cout << "Apple registered interfaces: " << paths.size() << std::endl;
        bool activated = false;
        for (const auto& path : paths) {
            try {
                auto capture = unkxwyno::usb::WinUsbCaptureTransport::open(path);
                std::cout << "Capture interface is ready." << std::endl; return 0;
            } catch (const std::exception& e) { std::cout << "Capture probe: " << e.what() << std::endl; }
            if (activate && !activated) {
                try { unkxwyno::usb::enable_usb_capture_mode(path); activated = true;
                    std::cout << "Capture-mode request sent; waiting for re-enumeration." << std::endl;
                } catch (const std::exception& e) { std::cout << "Activation: " << e.what() << std::endl; }
            }
        }
        if (activated) {
            for (int i = 0; i < 20; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                for (const auto& path : unkxwyno::usb::apple_usb_interfaces()) {
                    try { auto capture = unkxwyno::usb::WinUsbCaptureTransport::open(path);
                        std::cout << "Capture interface is ready after activation." << std::endl; return 0;
                    } catch (const std::exception&) { }
                }
            }
        }
        std::cerr << "No usable capture interface under the currently bound driver." << std::endl;
        return 2;
    } catch (const std::exception& e) { std::cerr << e.what() << std::endl; return 1; }
}
