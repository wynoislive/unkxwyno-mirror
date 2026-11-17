#include "usb_video_converter.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#if (defined(__clang__) || defined(__GNUC__)) && (defined(__x86_64__) || defined(__i386__))
#include <immintrin.h>
#define UNKXWYNO_USB_HAS_RUNTIME_AVX2 1
#define UNKXWYNO_USB_AVX2_TARGET __attribute__((target("avx2")))
#else
#define UNKXWYNO_USB_HAS_RUNTIME_AVX2 0
#define UNKXWYNO_USB_AVX2_TARGET
#endif
#define UNKXWYNO_USB_HAS_SSE2 1
#else
#define UNKXWYNO_USB_HAS_SSE2 0
#endif

namespace unkxwyno::usb {
namespace {

std::uint8_t clamp_byte(int value) {
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

void convert_pixel(int y, int u, int v, std::uint8_t* out) {
    const int c = y - 16;
    const int d = u - 128;
    const int e = v - 128;
    out[0] = clamp_byte((298 * c + 516 * d + 128) >> 8);
    out[1] = clamp_byte((298 * c - 100 * d - 208 * e + 128) >> 8);
    out[2] = clamp_byte((298 * c + 409 * e + 128) >> 8);
    out[3] = 255;
}

#if UNKXWYNO_USB_HAS_SSE2
__m128i pack_channel(__m128i values) {
    const auto words = _mm_packs_epi32(values, _mm_setzero_si128());
    return _mm_packus_epi16(words, _mm_setzero_si128());
}

void convert_four(const std::uint8_t* y, const std::uint8_t* uv,
                  std::uint8_t* out) {
    std::array<std::int16_t, 8> red_terms{};
    std::array<std::int16_t, 8> green_luma_chroma_terms{};
    std::array<std::int16_t, 8> green_v_terms{};
    std::array<std::int16_t, 8> blue_terms{};

    for (unsigned i = 0; i < 4; ++i) {
        const int c = static_cast<int>(y[i]) - 16;
        const auto chroma = static_cast<std::size_t>(i & ~1U);
        const int d = static_cast<int>(uv[chroma]) - 128;
        const int e = static_cast<int>(uv[chroma + 1]) - 128;
        red_terms[2 * i] = static_cast<std::int16_t>(c);
        red_terms[2 * i + 1] = static_cast<std::int16_t>(e);
        green_luma_chroma_terms[2 * i] = static_cast<std::int16_t>(c);
        green_luma_chroma_terms[2 * i + 1] = static_cast<std::int16_t>(d);
        green_v_terms[2 * i] = static_cast<std::int16_t>(e);
        blue_terms[2 * i] = static_cast<std::int16_t>(c);
        blue_terms[2 * i + 1] = static_cast<std::int16_t>(d);
    }

    const auto red = _mm_srai_epi32(_mm_add_epi32(
        _mm_madd_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(red_terms.data())),
                       _mm_setr_epi16(298, 409, 298, 409, 298, 409, 298, 409)),
        _mm_set1_epi32(128)), 8);
    auto green = _mm_add_epi32(
        _mm_madd_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(green_luma_chroma_terms.data())),
                       _mm_setr_epi16(298, -100, 298, -100, 298, -100, 298, -100)),
        _mm_madd_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(green_v_terms.data())),
                       _mm_setr_epi16(-208, 0, -208, 0, -208, 0, -208, 0)));
    green = _mm_srai_epi32(_mm_add_epi32(green, _mm_set1_epi32(128)), 8);
    const auto blue = _mm_srai_epi32(_mm_add_epi32(
        _mm_madd_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(blue_terms.data())),
                       _mm_setr_epi16(298, 516, 298, 516, 298, 516, 298, 516)),
        _mm_set1_epi32(128)), 8);

    const auto r = pack_channel(red);
    const auto g = pack_channel(green);
    const auto b = pack_channel(blue);
    const auto bg = _mm_unpacklo_epi8(b, g);
    const auto ra = _mm_unpacklo_epi8(r, _mm_set1_epi8(static_cast<char>(-1)));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(out), _mm_unpacklo_epi16(bg, ra));
}
#endif

#if UNKXWYNO_USB_HAS_RUNTIME_AVX2
bool has_avx2() noexcept {
    static const bool available = [] {
        __builtin_cpu_init();
        return __builtin_cpu_supports("avx2");
    }();
    return available;
}

UNKXWYNO_USB_AVX2_TARGET
__m128i pack_channel_eight(__m256i values) {
    const auto words = _mm256_packs_epi32(values, _mm256_setzero_si256());
    const auto bytes = _mm256_packus_epi16(words, _mm256_setzero_si256());
    const auto first = _mm256_castsi256_si128(bytes);
    const auto second = _mm256_extracti128_si256(bytes, 1);
    return _mm_unpacklo_epi32(first, second);
}

UNKXWYNO_USB_AVX2_TARGET
void convert_eight(const std::uint8_t* y, const std::uint8_t* uv,
                   std::uint8_t* out) {
    std::array<std::int32_t, 8> luma{};
    std::array<std::int32_t, 8> blue_chroma{};
    std::array<std::int32_t, 8> red_chroma{};
    for (unsigned i = 0; i < 8; ++i) {
        luma[i] = static_cast<int>(y[i]) - 16;
        const auto chroma = static_cast<std::size_t>(i & ~1U);
        blue_chroma[i] = static_cast<int>(uv[chroma]) - 128;
        red_chroma[i] = static_cast<int>(uv[chroma + 1]) - 128;
    }
    const auto c = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(luma.data()));
    const auto d = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(blue_chroma.data()));
    const auto e = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(red_chroma.data()));
    const auto rounding = _mm256_set1_epi32(128);
    const auto red = _mm256_srai_epi32(_mm256_add_epi32(
        _mm256_add_epi32(_mm256_mullo_epi32(c, _mm256_set1_epi32(298)),
                         _mm256_mullo_epi32(e, _mm256_set1_epi32(409))), rounding), 8);
    const auto green = _mm256_srai_epi32(_mm256_add_epi32(
        _mm256_sub_epi32(_mm256_sub_epi32(_mm256_mullo_epi32(c, _mm256_set1_epi32(298)),
                                          _mm256_mullo_epi32(d, _mm256_set1_epi32(100))),
                         _mm256_mullo_epi32(e, _mm256_set1_epi32(208))), rounding), 8);
    const auto blue = _mm256_srai_epi32(_mm256_add_epi32(
        _mm256_add_epi32(_mm256_mullo_epi32(c, _mm256_set1_epi32(298)),
                         _mm256_mullo_epi32(d, _mm256_set1_epi32(516))), rounding), 8);
    const auto r = pack_channel_eight(red);
    const auto g = pack_channel_eight(green);
    const auto b = pack_channel_eight(blue);
    const auto bg = _mm_unpacklo_epi8(b, g);
    const auto ra = _mm_unpacklo_epi8(r, _mm_set1_epi8(static_cast<char>(-1)));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(out), _mm_unpacklo_epi16(bg, ra));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(out + 16), _mm_unpackhi_epi16(bg, ra));
}
#endif

} // namespace

void nv12_to_bgra(std::span<const std::uint8_t> nv12, unsigned width,
                  unsigned height, unsigned stride,
                  std::span<std::uint8_t> bgra) {
    if (width == 0 || height == 0 || (width & 1U) != 0 || (height & 1U) != 0 ||
        stride < width || stride > 32768) {
        throw std::invalid_argument("Invalid NV12 dimensions or stride.");
    }

    const auto maximum = std::numeric_limits<std::size_t>::max();
    if (static_cast<std::size_t>(height) > maximum / stride ||
        static_cast<std::size_t>(height) > maximum / width) {
        throw std::invalid_argument("NV12 dimensions overflow the address space.");
    }
    const auto y_bytes = static_cast<std::size_t>(stride) * height;
    if (y_bytes > maximum - y_bytes / 2) {
        throw std::invalid_argument("NV12 surface size overflows the address space.");
    }
    const auto nv12_bytes = y_bytes + y_bytes / 2;
    const auto pixels = static_cast<std::size_t>(width) * height;
    if (nv12.size() < nv12_bytes || pixels > maximum / 4 ||
        bgra.size() < pixels * 4) {
        throw std::invalid_argument("NV12 or BGRA surface is shorter than its dimensions.");
    }

    const auto* y_plane = nv12.data();
    const auto* uv_plane = nv12.data() + y_bytes;
#if UNKXWYNO_USB_HAS_RUNTIME_AVX2
    const bool use_avx2 = has_avx2();
#endif
    for (unsigned row = 0; row < height; ++row) {
        const auto* y = y_plane + static_cast<std::size_t>(row) * stride;
        const auto* uv = uv_plane + static_cast<std::size_t>(row / 2) * stride;
        auto* output = bgra.data() + static_cast<std::size_t>(row) * width * 4;
        unsigned x = 0;
#if UNKXWYNO_USB_HAS_RUNTIME_AVX2
        if (use_avx2) {
            for (; x + 8 <= width; x += 8) {
                convert_eight(y + x, uv + x, output + static_cast<std::size_t>(x) * 4);
            }
        }
#endif
#if UNKXWYNO_USB_HAS_SSE2
        for (; x + 4 <= width; x += 4) {
            convert_four(y + x, uv + x, output + static_cast<std::size_t>(x) * 4);
        }
#endif
        for (; x < width; ++x) {
            const auto chroma = static_cast<std::size_t>(x & ~1U);
            convert_pixel(y[x], uv[chroma], uv[chroma + 1],
                          output + static_cast<std::size_t>(x) * 4);
        }
    }
}

std::string_view nv12_converter_name() noexcept {
#if UNKXWYNO_USB_HAS_RUNTIME_AVX2
    if (has_avx2()) return "AVX2";
#endif
#if UNKXWYNO_USB_HAS_SSE2
    return "SSE2";
#else
    return "scalar";
#endif
}

} // namespace unkxwyno::usb
