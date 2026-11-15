/**
 * @file mf_decoder.cpp
 * @brief Implementation of Microsoft Media Foundation D3D11VA Hardware Video Decoder.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#include "mf_decoder.hpp"

#ifdef _WIN32
#include <iostream>
#include <cstring>
#include <mfidl.h>
#include <codecapi.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "d3d11.lib")

namespace unkxwyno::decoder {

using Microsoft::WRL::ComPtr;

MfVideoDecoder::MfVideoDecoder(ComPtr<ID3D11Device> d3d_device)
    : d3d_device_(std::move(d3d_device)) {
    MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
}

MfVideoDecoder::~MfVideoDecoder() {
    Shutdown();
    MFShutdown();
}

bool MfVideoDecoder::Initialize(uint32_t width, uint32_t height, unkxwyno_video_codec_t codec) {
    std::lock_guard<std::mutex> lock(decoder_mutex_);
    if (is_initialized_) return true;

    width_  = width;
    height_ = height;
    codec_  = codec;

    if (!ConfigureDecoder(width, height, codec)) {
        return false;
    }

    is_initialized_ = true;
    std::cout << "[MF Decoder] Initialized hardware D3D11VA decoder for " << width << "x" << height << "\n";
    return true;
}

void MfVideoDecoder::Shutdown() {
    std::lock_guard<std::mutex> lock(decoder_mutex_);
    if (!is_initialized_) return;

    if (decoder_mft_) {
        decoder_mft_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
        decoder_mft_.Reset();
    }
    dxgi_manager_.Reset();
    is_initialized_ = false;
}

void MfVideoDecoder::Flush() {
    std::lock_guard<std::mutex> lock(decoder_mutex_);
    if (decoder_mft_) {
        decoder_mft_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
    }
}

bool MfVideoDecoder::ConfigureDecoder(uint32_t width, uint32_t height, unkxwyno_video_codec_t codec) {
    // 1. Enumerate and instantiate hardware decoder MFT
    GUID sub_type = (codec == UNKXWYNO_CODEC_H265) ? MFVideoFormat_HEVC : MFVideoFormat_H264;
    MFT_REGISTER_TYPE_INFO input_info = { MFMediaType_Video, sub_type };

    IMFActivate** activates = nullptr;
    UINT32 count = 0;

    HRESULT hr = MFTEnumEx(
        MFT_CATEGORY_VIDEO_DECODER,
        MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER,
        &input_info,
        nullptr,
        &activates,
        &count
    );

    if (FAILED(hr) || count == 0) {
        // Fallback to standard software/hardware enumeration
        hr = MFTEnumEx(
            MFT_CATEGORY_VIDEO_DECODER,
            MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ALL,
            &input_info,
            nullptr,
            &activates,
            &count
        );
    }

    if (FAILED(hr) || count == 0) {
        std::cerr << "[MF Decoder] Failed to find a compatible H.264/HEVC MFT decoder.\n";
        return false;
    }

    hr = activates[0]->ActivateObject(IID_PPV_ARGS(&decoder_mft_));
    for (UINT32 i = 0; i < count; ++i) {
        activates[i]->Release();
    }
    CoTaskMemFree(activates);

    if (FAILED(hr) || !decoder_mft_) {
        return false;
    }

    // 2. Enable Direct3D 11 Video Acceleration (D3D11VA)
    if (d3d_device_) {
        hr = MFCreateDXGIDeviceManager(&dxgi_reset_token_, &dxgi_manager_);
        if (SUCCEEDED(hr)) {
            hr = dxgi_manager_->ResetDevice(d3d_device_.Get(), dxgi_reset_token_);
            if (SUCCEEDED(hr)) {
                decoder_mft_->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER,
                                             reinterpret_cast<ULONG_PTR>(dxgi_manager_.Get()));
            }
        }
    }

    // 3. Configure Input Media Type
    ComPtr<IMFMediaType> in_type;
    MFCreateMediaType(&in_type);
    in_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    in_type->SetGUID(MF_MT_SUBTYPE, sub_type);
    MFSetAttributeSize(in_type.Get(), MF_MT_FRAME_SIZE, width, height);

    hr = decoder_mft_->SetInputType(0, in_type.Get(), 0);
    if (FAILED(hr)) {
        std::cerr << "[MF Decoder] SetInputType failed (hr=" << std::hex << hr << ")\n";
        return false;
    }

    // 4. Configure Output Media Type (NV12 for zero-copy D3D11 swapchains)
    ComPtr<IMFMediaType> out_type;
    MFCreateMediaType(&out_type);
    out_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    out_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    MFSetAttributeSize(out_type.Get(), MF_MT_FRAME_SIZE, width, height);

    hr = decoder_mft_->SetOutputType(0, out_type.Get(), 0);
    if (FAILED(hr)) {
        // Try other available output formats
        for (DWORD idx = 0; ; ++idx) {
            ComPtr<IMFMediaType> candidate;
            if (FAILED(decoder_mft_->GetOutputAvailableType(0, idx, &candidate))) break;

            GUID out_guid;
            candidate->GetGUID(MF_MT_SUBTYPE, &out_guid);
            if (out_guid == MFVideoFormat_NV12 || out_guid == MFVideoFormat_YUY2) {
                hr = decoder_mft_->SetOutputType(0, candidate.Get(), 0);
                if (SUCCEEDED(hr)) break;
            }
        }
    }

    // 5. Notify streaming begin
    decoder_mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    decoder_mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);

    return true;
}

bool MfVideoDecoder::DecodeNalu(std::span<const uint8_t> nalu, uint64_t pts_us) {
    std::lock_guard<std::mutex> lock(decoder_mutex_);
    if (!is_initialized_ || !decoder_mft_) return false;

    // 1. Create Media Buffer wrapping NALU
    ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = MFCreateMemoryBuffer(static_cast<DWORD>(nalu.size()), &buffer);
    if (FAILED(hr)) return false;

    BYTE* dest = nullptr;
    buffer->Lock(&dest, nullptr, nullptr);
    std::memcpy(dest, nalu.data(), nalu.size());
    buffer->Unlock();
    buffer->SetCurrentLength(static_cast<DWORD>(nalu.size()));

    // 2. Wrap into IMFSample
    ComPtr<IMFSample> sample;
    MFCreateSample(&sample);
    sample->AddBuffer(buffer.Get());
    sample->SetSampleTime(static_cast<LONGLONG>(pts_us * 10)); // Convert to 100ns units

    // 3. ProcessInput
    hr = decoder_mft_->ProcessInput(0, sample.Get(), 0);
    if (FAILED(hr)) {
        if (hr == MF_E_NOTACCEPTING) {
            ProcessOutputSamples();
            decoder_mft_->ProcessInput(0, sample.Get(), 0);
        } else {
            return false;
        }
    }

    // 4. Retrieve any decoded frames ready for display
    ProcessOutputSamples();
    return true;
}

void MfVideoDecoder::ProcessOutputSamples() {
    MFT_OUTPUT_STREAM_INFO stream_info{};
    decoder_mft_->GetOutputStreamInfo(0, &stream_info);

    MFT_OUTPUT_DATA_BUFFER output_buffer{};
    output_buffer.dwStreamID = 0;

    ComPtr<IMFSample> out_sample;
    if (!(stream_info.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES))) {
        MFCreateSample(&out_sample);
        ComPtr<IMFMediaBuffer> out_mb;
        MFCreateMemoryBuffer(stream_info.cbSize, &out_mb);
        out_sample->AddBuffer(out_mb.Get());
        output_buffer.pSample = out_sample.Get();
    }

    DWORD status = 0;
    while (true) {
        HRESULT hr = decoder_mft_->ProcessOutput(0, 1, &output_buffer, &status);
        if (FAILED(hr)) {
            // MF_E_TRANSFORM_NEED_MORE_INPUT is normal when waiting for next NALU
            break;
        }

        if (output_buffer.pSample) {
            LONGLONG sample_time = 0;
            output_buffer.pSample->GetSampleTime(&sample_time);
            uint64_t pts_us = static_cast<uint64_t>(sample_time / 10);

            // Extract DXGI hardware surface if available
            ComPtr<IMFMediaBuffer> media_buf;
            output_buffer.pSample->GetBufferByIndex(0, &media_buf);

            ComPtr<IMFDXGIBuffer> dxgi_buf;
            media_buf.As(&dxgi_buf);

            DecodedFrame frame{};
            frame.width   = static_cast<int>(width_);
            frame.height  = static_cast<int>(height_);
            frame.pts_us  = pts_us;

            if (dxgi_buf) {
                // Zero-copy hardware D3D11 surface!
                ComPtr<ID3D11Texture2D> d3d_tex;
                dxgi_buf->GetResource(IID_PPV_ARGS(&d3d_tex));
                frame.native_surface = d3d_tex.Get();
                frame.is_hardware_surface = true;
                if (frame_callback_) {
                    frame_callback_(frame);
                }
            } else if (media_buf) {
                // Software buffer fallback: keep buffer locked while consumer reads raw_pixels
                BYTE* p_bytes = nullptr;
                DWORD cur_len = 0;
                if (SUCCEEDED(media_buf->Lock(&p_bytes, nullptr, &cur_len))) {
                    frame.raw_pixels = p_bytes;
                    frame.is_hardware_surface = false;
                    if (frame_callback_) {
                        frame_callback_(frame);
                    }
                    media_buf->Unlock();
                }
            }
        }

        if (output_buffer.pEvents) {
            output_buffer.pEvents->Release();
            output_buffer.pEvents = nullptr;
        }
    }
}

} // namespace unkxwyno::decoder
#endif // _WIN32
