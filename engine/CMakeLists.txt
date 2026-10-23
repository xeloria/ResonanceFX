# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
cmake_minimum_required(VERSION 3.18)

project(ResonanceFX CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Driver version reported through the version getters.
set(RFX_VERSION_CODE 20100 CACHE STRING "Driver version code")
set(RFX_VERSION_NAME "2.1.0" CACHE STRING "Driver version name")

# The engine: ResonanceFX processors + the shared DSP core + the Android effect glue.
file(GLOB_RECURSE RFX_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(resonancefx SHARED ${RFX_SOURCES})

target_include_directories(resonancefx PRIVATE src src/core/include)
target_compile_definitions(resonancefx PRIVATE
        RFX_VERSION_CODE=${RFX_VERSION_CODE}
        RFX_VERSION_NAME="${RFX_VERSION_NAME}")

target_compile_options(resonancefx PRIVATE
        -O3 -DNDEBUG -flto
        -fvisibility=hidden -fvisibility-inlines-hidden
        -ffunction-sections -fdata-sections
        -Wall -Wno-unused-parameter)

target_link_options(resonancefx PRIVATE
        -flto -Wl,--gc-sections -Wl,--exclude-libs,ALL
        # 16 KB page alignment: required on Android 15+ devices that use 16 KB pages,
        # harmless on 4 KB devices.
        -Wl,-z,max-page-size=16384
        # Static C++ runtime: the audio HAL's linker namespace can't see libc++_shared.so.
        -static-libstdc++)

target_link_libraries(resonancefx PRIVATE log)

# Strip in release, keeping the dynamic symbol table (AELI must stay exported).
add_custom_command(TARGET resonancefx POST_BUILD
        COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:resonancefx>)
