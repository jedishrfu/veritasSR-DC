cmake_minimum_required(VERSION 3.21)
project(veritasSR_DC LANGUAGES C CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(SR_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(SR_BUILD_EX0 "Build the piecewise regression experiments" ON)
option(SR_BUILD_EX2 "Build the earlier expression experiments" ON)
option(SR_BUILD_EX3 "Build the current expression experiment" ON)

add_library(sr_build_options INTERFACE)
target_compile_options(sr_build_options INTERFACE
    "$<$<COMPILE_LANG_AND_ID:C,AppleClang,Clang,GNU>:-Wall;-Wextra>"
    "$<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang,GNU>:-Wall;-Wextra>")
if(SR_ENABLE_ASAN)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "SR_ENABLE_ASAN requires Clang or GCC")
    endif()
    target_compile_options(sr_build_options INTERFACE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(sr_build_options INTERFACE -fsanitize=address)
endif()

if(SR_BUILD_EX0)
    add_subdirectory(ex_0_pwpr_fall_2025)
endif()
if(SR_BUILD_EX2)
    add_subdirectory(ex_2)
endif()
if(SR_BUILD_EX3)
    add_subdirectory(ex_3)
endif()
