cmake_minimum_required(VERSION 3.25)
project(unkxwyno_mirror VERSION 0.1.0 LANGUAGES CXX)

option(UNKXWYNO_BUILD_TESTS "Build independent core tests" ON)

if(UNKXWYNO_BUILD_TESTS)
  include(CTest)
  enable_testing()
else()
  set(BUILD_TESTING OFF CACHE BOOL "Build tests" FORCE)
endif()

add_subdirectory(core)

