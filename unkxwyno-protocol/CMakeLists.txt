cmake_minimum_required(VERSION 3.20)
project(unkxwyno-protocol VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header-only interface library for unkxwyno-protocol
add_library(unkxwyno_protocol INTERFACE)
target_include_directories(unkxwyno_protocol INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Test executable
add_executable(test_protocol tests/test_protocol.cpp)
target_link_libraries(test_protocol PRIVATE unkxwyno_protocol)

enable_testing()
add_test(NAME unkxwyno_protocol_test COMMAND test_protocol)
