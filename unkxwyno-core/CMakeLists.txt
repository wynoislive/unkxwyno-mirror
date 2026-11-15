cmake_minimum_required(VERSION 3.20)
project(unkxwyno-core VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Include protocol and core headers
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../unkxwyno-protocol/include
)

# Core source files
set(CORE_SOURCES
    src/protocol/demuxer.cpp
    src/protocol/aoa2_client.cpp
    src/protocol/usbmuxd_client.cpp
    src/protocol/airplay_server.cpp
    src/network/tcp_server.cpp
    src/engine.cpp
    src/abi_exports.cpp
)

if (WIN32)
    list(APPEND CORE_SOURCES
        src/usb/winusb_transport.cpp
        src/decoder/mf_decoder.cpp
        src/renderer/d3d11_renderer.cpp
        src/renderer/native_viewport_win32.cpp
        src/audio/wasapi_renderer.cpp
    )
endif()

# Build shared library for C# .NET P/Invoke interop
add_library(unkxwyno_core SHARED ${CORE_SOURCES})
target_compile_definitions(unkxwyno_core PRIVATE UNKXWYNO_EXPORTS)

if (WIN32)
    target_link_libraries(unkxwyno_core PRIVATE
        setupapi
        winusb
        ws2_32
        d3d11
        dxgi
        mfplat
        mfuuid
        ole32
        avrt
    )
endif()

# Unit test executable
add_executable(test_demuxer
    tests/test_demuxer.cpp
    src/protocol/demuxer.cpp
)

enable_testing()
add_test(NAME demuxer_test COMMAND test_demuxer)
