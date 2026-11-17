add_executable(unkxwyno_core_tests test_main.cpp)
target_link_libraries(unkxwyno_core_tests PRIVATE unkxwyno_core)
target_compile_features(unkxwyno_core_tests PRIVATE cxx_std_20)

# A Windows test executable does not search its parent build directory for
# dependent DLLs. Keep the independently built core beside the test binary so
# CTest exercises the real shared-library ABI instead of failing at process
# startup with a missing-DLL error.
if(WIN32)
    add_custom_command(TARGET unkxwyno_core_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:unkxwyno_core>
            $<TARGET_FILE_DIR:unkxwyno_core_tests>
        VERBATIM
    )
endif()

if(MSVC)
    target_compile_options(unkxwyno_core_tests PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(unkxwyno_core_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

add_test(NAME unkxwyno_core_tests COMMAND unkxwyno_core_tests)

add_executable(unkxwyno_usb_capture_tests
    ../usb_capture/test_main.cpp
    ../../src/backend/independent/usb_video_converter.cpp)
target_link_libraries(unkxwyno_usb_capture_tests PRIVATE unkxwyno_core)
target_compile_features(unkxwyno_usb_capture_tests PRIVATE cxx_std_20)
if(WIN32)
    target_compile_definitions(unkxwyno_usb_capture_tests PRIVATE USB_VIDEO_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/../usb_capture/reference-feed.bin")
endif()
if(MSVC)
    target_compile_options(unkxwyno_usb_capture_tests PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(unkxwyno_usb_capture_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()
if(WIN32)
    add_custom_command(TARGET unkxwyno_usb_capture_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:unkxwyno_core> $<TARGET_FILE_DIR:unkxwyno_usb_capture_tests>
        VERBATIM)
endif()
add_test(NAME unkxwyno_usb_capture_tests COMMAND unkxwyno_usb_capture_tests)

