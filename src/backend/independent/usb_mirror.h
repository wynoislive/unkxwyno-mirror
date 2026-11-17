#pragma once
#include "unkxwyno/core/c_api.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*uxm_usb_status_callback)(void* context, int state, const char* message,
    unsigned width, unsigned height, unsigned long long displayed_frames, double displayed_fps);
// Status: 0 stopped, 1 progress, 2 presented video, 3 failure,
// 4 capture driver required (no driver changed), 5 trusted session required.
// One blocking run on a worker thread. Stop is thread-safe; destroy after run exits.
UXM_API void* uxm_usb_mirror_create(unsigned display_width, unsigned display_height, unsigned target_fps);
UXM_API int uxm_usb_mirror_run(void* session, uxm_usb_status_callback callback, void* context);
UXM_API void uxm_usb_mirror_stop(void* session);
UXM_API void uxm_usb_mirror_destroy(void* session);
UXM_API int uxm_usb_mirror_enable_pairing(void* session);
UXM_API int uxm_usb_mirror_control_read(void* session,unsigned char* bytes,unsigned capacity);
UXM_API int uxm_usb_mirror_control_write(void* session,const unsigned char* bytes,unsigned length);
UXM_API void uxm_usb_mirror_auth_result(void* session,int authenticated,const char* error);
typedef struct uxm_usb_decode_report {
    unsigned width, height;
    unsigned long long decoded_frames, pixel_checksum;
} uxm_usb_decode_report;
// Offline diagnostics: validates a bounded concatenation of capture packets with
// the same session parser and Windows decoder used by live mirroring. No file I/O.
UXM_API int uxm_usb_decode_recording(const unsigned char* bytes, unsigned length, uxm_usb_decode_report* report);
#ifdef __cplusplus
}
#endif
