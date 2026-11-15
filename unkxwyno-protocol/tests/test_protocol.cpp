#include <cassert>
#include <cstdio>
#include <cstring>
#include "../include/unkxwyno_protocol.h"
#include "../include/unkxwyno_abi.h"

int main() {
    std::printf("========================================\n");
    std::printf(" unkXwyno Protocol C++ Struct Test\n");
    std::printf("========================================\n");

    // 1. Verify strict 1-byte alignment and sizes
    static_assert(sizeof(unkxwyno_packet_header_t) == 8, "unkxwyno_packet_header_t must be exactly 8 bytes!");
    static_assert(sizeof(unkxwyno_video_metadata_t) == 24, "unkxwyno_video_metadata_t must be exactly 24 bytes!");
    static_assert(sizeof(unkxwyno_touch_event_t) == 16, "unkxwyno_touch_event_t must be exactly 16 bytes!");
    static_assert(sizeof(unkxwyno_key_event_t) == 4, "unkxwyno_key_event_t must be exactly 4 bytes!");
    static_assert(sizeof(unkxwyno_mouse_event_t) == 8, "unkxwyno_mouse_event_t must be exactly 8 bytes!");

    std::printf("✔ [PASS] static_assert size validations passed!\n");

    // 2. Test header helper functions
    unkxwyno_packet_header_t hdr;
    unkxwyno_header_init(&hdr, UNKXWYNO_CHANNEL_VIDEO, UNKXWYNO_VIDEO_KEYFRAME, 65536, 1);

    assert(hdr.channel_id == UNKXWYNO_CHANNEL_VIDEO);
    assert(hdr.packet_type == UNKXWYNO_VIDEO_KEYFRAME);
    assert(hdr.sequence == 1);
    assert(hdr.reserved == 0x00);
    assert(unkxwyno_header_is_valid(&hdr) == true);

    std::printf("✔ [PASS] Header initialization and validation passed!\n");

    // 3. Test string constants
    assert(std::strcmp(UNKXWYNO_BRAND_NAME, "unkXwyno Mirror") == 0);
    assert(std::strcmp(UNKXWYNO_SUBDOMAIN_URI, "https://unkxwyno-mirror.genzhost.best") == 0);
    assert(std::strcmp(UNKXWYNO_AOA_MANUFACTURER, "unkXwyno") == 0);
    assert(std::strcmp(UNKXWYNO_AOA_MODEL, "unkXwyno Low-Latency Display") == 0);

    std::printf("✔ [PASS] Brand identifier string constants verified!\n");
    std::printf("========================================\n");
    std::printf(" All C++ Protocol Assertions PASSED!\n");
    std::printf("========================================\n");
    return 0;
}
