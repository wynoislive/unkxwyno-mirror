#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winusb.h>
#include "winusb_capture_transport.hpp"
#include <atomic>
#include <mutex>
#include <system_error>
#include <utility>

namespace unkxwyno::usb {
namespace {
[[noreturn]] void fail(const char* operation, DWORD code = GetLastError()) {
    throw std::system_error(static_cast<int>(code), std::system_category(), operation);
}
bool is_capture(WINUSB_INTERFACE_HANDLE handle, UCHAR& alternate) {
    USB_INTERFACE_DESCRIPTOR descriptor{};
    if (!WinUsb_GetCurrentAlternateSetting(handle, &alternate)) fail("USB alternate setting");
    if (!WinUsb_QueryInterfaceSettings(handle, alternate, &descriptor)) fail("USB interface descriptor");
    return descriptor.bInterfaceClass == 0xff && descriptor.bInterfaceSubClass == 0x2a
        && descriptor.bInterfaceProtocol == 0xff && descriptor.bNumEndpoints == 2;
}
} // namespace

struct WinUsbCaptureTransport::Impl {
    HANDLE device{INVALID_HANDLE_VALUE};
    WINUSB_INTERFACE_HANDLE base{};
    WINUSB_INTERFACE_HANDLE capture{};
    UCHAR input{}, output{};
    std::atomic_bool stopped{false};
    std::mutex read_mutex, write_mutex;
    CapturePacketReader parser;
    std::array<std::uint8_t, 0x4000> input_bytes{};
    std::size_t pending_offset{}, pending_length{};
    ~Impl() {
        if (capture && capture != base) WinUsb_Free(capture);
        if (base) WinUsb_Free(base);
        if (device != INVALID_HANDLE_VALUE) CloseHandle(device);
    }
};

WinUsbCaptureTransport::WinUsbCaptureTransport(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

std::unique_ptr<WinUsbCaptureTransport> WinUsbCaptureTransport::open(const std::wstring& path) {
    if (path.empty() || path.find(L'\0') != std::wstring::npos)
        throw std::invalid_argument("USB interface path is empty or contains a null character.");
    auto impl = std::make_unique<Impl>();
    impl->device = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
    if (impl->device == INVALID_HANDLE_VALUE) fail("Open USB capture interface");
    if (!WinUsb_Initialize(impl->device, &impl->base)) fail("Initialize WinUSB capture interface");
    USB_DEVICE_DESCRIPTOR device_descriptor{};
    ULONG descriptor_bytes{};
    if (!WinUsb_GetDescriptor(impl->base, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0,
        reinterpret_cast<PUCHAR>(&device_descriptor), sizeof(device_descriptor), &descriptor_bytes))
        fail("USB device descriptor");
    if (descriptor_bytes != sizeof(device_descriptor) || device_descriptor.idVendor != 0x05ac)
        throw std::invalid_argument("The selected device is not an Apple USB device.");

    UCHAR alternate{};
    if (is_capture(impl->base, alternate)) impl->capture = impl->base;
    else {
        for (unsigned index = 0; index < 256; ++index) {
            WINUSB_INTERFACE_HANDLE candidate{};
            if (!WinUsb_GetAssociatedInterface(impl->base, static_cast<UCHAR>(index), &candidate)) {
                if (GetLastError() == ERROR_NO_MORE_ITEMS) break;
                fail("Query associated USB capture interface");
            }
            try {
                if (is_capture(candidate, alternate)) { impl->capture = candidate; break; }
            } catch (...) { WinUsb_Free(candidate); throw; }
            WinUsb_Free(candidate);
        }
    }
    if (!impl->capture) throw std::runtime_error("USB capture interface FF/2A/FF is not exposed; capture-mode activation remains required.");
    std::array<BulkEndpoint, 2> endpoints{};
    for (UCHAR index = 0; index < 2; ++index) {
        WINUSB_PIPE_INFORMATION pipe{};
        if (!WinUsb_QueryPipe(impl->capture, alternate, index, &pipe)) fail("USB capture endpoint");
        endpoints[index] = {pipe.PipeId, pipe.MaximumPacketSize, pipe.PipeType == UsbdPipeTypeBulk};
    }
    const auto selected = select_capture_endpoints(endpoints);
    if (!selected) throw std::runtime_error("Capture interface requires valid opposite-direction bulk endpoints.");
    impl->input = selected->input;
    impl->output = selected->output;
    ULONG read_timeout = 200, write_timeout = 1000;
    if (!WinUsb_SetPipePolicy(impl->capture, impl->input, PIPE_TRANSFER_TIMEOUT, sizeof(read_timeout), &read_timeout)
        || !WinUsb_SetPipePolicy(impl->capture, impl->output, PIPE_TRANSFER_TIMEOUT, sizeof(write_timeout), &write_timeout))
        fail("Configure USB capture transfer timeouts");
    UCHAR terminate = TRUE;
    if (!WinUsb_SetPipePolicy(impl->capture, impl->output, SHORT_PACKET_TERMINATE, sizeof(terminate), &terminate))
        fail("Configure USB terminating zero-length packets");
    return std::unique_ptr<WinUsbCaptureTransport>(new WinUsbCaptureTransport(std::move(impl)));
}

std::optional<CapturePacket> WinUsbCaptureTransport::read_packet(std::chrono::milliseconds wait) {
    std::lock_guard lock(impl_->read_mutex);
    const auto deadline = std::chrono::steady_clock::now() + wait;
    while (!impl_->stopped.load()) {
        if (impl_->pending_length != 0) {
            auto result = impl_->parser.consume(std::span(impl_->input_bytes).subspan(impl_->pending_offset, impl_->pending_length));
            impl_->pending_offset += result.consumed;
            impl_->pending_length -= result.consumed;
            if (result.status == ParseStatus::PacketReady) return std::move(result.packet);
            if (result.status != ParseStatus::NeedMore) {
                request_stop();
                throw std::runtime_error("Malformed USB capture packet; session stopped.");
            }
        }
        ULONG transferred{};
        if (!WinUsb_ReadPipe(impl_->capture, impl_->input, impl_->input_bytes.data(),
            static_cast<ULONG>(impl_->input_bytes.size()), &transferred, nullptr)) {
            const auto error = GetLastError();
            if (impl_->stopped.load()) return std::nullopt;
            if (error == ERROR_SEM_TIMEOUT) {
                if (std::chrono::steady_clock::now() >= deadline) return std::nullopt;
                continue;
            }
            request_stop();
            fail("Read USB capture stream", error);
        }
        impl_->pending_offset = 0;
        impl_->pending_length = transferred;
        if (!transferred && std::chrono::steady_clock::now() >= deadline) return std::nullopt;
    }
    return std::nullopt;
}

void WinUsbCaptureTransport::write_packet(std::span<const std::uint8_t> packet) {
    if (packet.empty() || packet.size() > 4U * 1024U * 1024U)
        throw std::invalid_argument("USB capture write exceeds packet bounds.");
    std::lock_guard lock(impl_->write_mutex);
    if (impl_->stopped.load()) throw std::runtime_error("USB capture session is stopped.");
    ULONG transferred{};
    if (!WinUsb_WritePipe(impl_->capture, impl_->output, const_cast<PUCHAR>(packet.data()),
        static_cast<ULONG>(packet.size()), &transferred, nullptr)) {
        const auto error = GetLastError();
        request_stop();
        fail("Write USB capture stream", error);
    }
    if (transferred != packet.size()) {
        request_stop();
        throw std::runtime_error("Partial USB capture write; session stopped to avoid replaying bytes.");
    }
}

void WinUsbCaptureTransport::request_stop() noexcept {
    impl_->stopped.store(true);
    if (impl_->capture) {
        WinUsb_AbortPipe(impl_->capture, impl_->input);
        WinUsb_AbortPipe(impl_->capture, impl_->output);
    }
    if (impl_->device != INVALID_HANDLE_VALUE) CancelIoEx(impl_->device, nullptr);
}

WinUsbCaptureTransport::~WinUsbCaptureTransport() {
    request_stop();
    std::scoped_lock lock(impl_->read_mutex, impl_->write_mutex);
    impl_->parser.reset();
}
} // namespace unkxwyno::usb
