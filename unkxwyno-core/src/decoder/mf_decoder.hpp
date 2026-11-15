/**
 * @file mf_decoder.hpp
 * @brief Microsoft Media Foundation D3D11VA Hardware Video Decoder.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#pragma once

#include "video_decoder.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mfapi.h>
#include <mftransform.h>
#include <mferror.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <mutex>

namespace unkxwyno::decoder {

class MfVideoDecoder final : public IVideoDecoder {
public:
    explicit MfVideoDecoder(Microsoft::WRL::ComPtr<ID3D11Device> d3d_device);
    ~MfVideoDecoder() override;

    bool Initialize(uint32_t width, uint32_t height, unkxwyno_video_codec_t codec) override;
    void Shutdown() override;

    bool DecodeNalu(std::span<const uint8_t> nalu, uint64_t pts_us) override;
    void Flush() override;

    void SetFrameCallback(FrameDecodedCallback callback) override {
        frame_callback_ = std::move(callback);
    }

private:
    bool ConfigureDecoder(uint32_t width, uint32_t height, unkxwyno_video_codec_t codec);
    void ProcessOutputSamples();

    Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
    Microsoft::WRL::ComPtr<IMFTransform> decoder_mft_;
    Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> dxgi_manager_;
    UINT dxgi_reset_token_{0};

    uint32_t width_{1920};
    uint32_t height_{1080};
    unkxwyno_video_codec_t codec_{UNKXWYNO_CODEC_H264};
    bool is_initialized_{false};

    FrameDecodedCallback frame_callback_;
    std::mutex decoder_mutex_;
};

} // namespace unkxwyno::decoder
#endif // _WIN32
