cmake_minimum_required(VERSION 3.24)

project(unkxwyno_core VERSION 0.1.0 LANGUAGES CXX)

add_library(unkxwyno_core SHARED
    src/c_api.cpp
    src/negotiation.cpp
    src/protocol.cpp
    src/session.cpp
    src/stats.cpp
    ../src/backend/independent/usb_capture_protocol.cpp
    ../src/backend/independent/usb_capture_session.cpp
    ../src/backend/independent/usb_mux_control.cpp
    ../src/backend/independent/usb_video_converter.cpp
)

target_compile_features(unkxwyno_core PUBLIC cxx_std_20)
if(WIN32)
    target_compile_definitions(unkxwyno_core PRIVATE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
    target_sources(unkxwyno_core PRIVATE
        ../src/backend/independent/winusb_capture_transport.cpp
        ../src/backend/independent/apple_usb_devices.cpp
        ../src/backend/independent/libusb_capture_transport.cpp
        ../src/backend/independent/libusb_win32_capture_transport.cpp
        ../src/backend/independent/usb_mirror.cpp)
    set(USB_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/../out/deps/libusb-1.0.30")
    if(NOT EXISTS "${USB_DEPENDENCY}/include/libusb.h")
        message(FATAL_ERROR "Run scripts/prepare-usb-dependency.ps1 before configuring Windows USB capture.")
    endif()
    target_include_directories(unkxwyno_core SYSTEM PRIVATE "${USB_DEPENDENCY}/include")
    add_custom_command(TARGET unkxwyno_core POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${USB_DEPENDENCY}/MinGW64/dll/libusb-1.0.dll" $<TARGET_FILE_DIR:unkxwyno_core>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${USB_DEPENDENCY}/libusb-1.0.30/COPYING" "$<TARGET_FILE_DIR:unkxwyno_core>/libusb-LICENSE.txt"
        VERBATIM)
    target_link_libraries(unkxwyno_core PRIVATE winusb setupapi advapi32 ole32 mfplat mfuuid wmcodecdspuuid winmm gdi32 user32 dwmapi)
    add_executable(unkxwyno_usb_probe ../tests/usb_capture/device_probe.cpp)
    target_link_libraries(unkxwyno_usb_probe PRIVATE unkxwyno_core)
endif()
set_target_properties(unkxwyno_core PROPERTIES PREFIX "")
if(MINGW)
    # Keep the C ABI DLL loadable without a compiler installation on PATH.
    target_link_options(unkxwyno_core PRIVATE -static)
endif()
target_compile_definitions(unkxwyno_core
    PRIVATE UNKXWYNO_CORE_SHARED UNKXWYNO_CORE_BUILD
    INTERFACE UNKXWYNO_CORE_SHARED
)
target_include_directories(unkxwyno_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../src/backend/independent>
        $<INSTALL_INTERFACE:include>
)

if(MSVC)
    target_compile_options(unkxwyno_core PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(unkxwyno_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/core"
        "${CMAKE_CURRENT_BINARY_DIR}/tests/core"
    )
endif()

