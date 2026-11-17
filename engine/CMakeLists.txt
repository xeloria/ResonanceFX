# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
cmake_minimum_required(VERSION 3.18)

project(ResonanceFX CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Driver version reported through the version getters.
set(RFX_VERSION_CODE 20300 CACHE STRING "Driver version code")
set(RFX_VERSION_NAME "2.3.0" CACHE STRING "Driver version name")

# Everything except the Android entry-point glue (the DSP core and the processors are shared by
# both effect-library ABIs).
file(GLOB_RECURSE RFX_ALL_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
set(RFX_CORE_SOURCES ${RFX_ALL_SOURCES})
list(FILTER RFX_CORE_SOURCES EXCLUDE REGEX "/src/android/")

# The engine: ResonanceFX processors + the shared DSP core + the legacy (AELI) Android glue.
# This is the library every HIDL cause effect HAL, and every AIDL HAL that still loads native
# effect libraries, opens.
add_library(resonancefx SHARED
        ${RFX_CORE_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/android/EffectLibrary.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/android/EffectContext.cpp)

# The AIDL effect front-end (createEffect/queryEffect/destroyEffect + IEffect), for Android 16
# effect HALs that only load AIDL plug-ins. Built as a second library from the same engine when
# its sources are present, so a single module build covers both factory generations.
file(GLOB_RECURSE RFX_AIDL_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/android/aidl/*.cpp)
if (RFX_AIDL_SOURCES)
    add_library(resonancefx_aidl SHARED
            ${RFX_CORE_SOURCES}
            ${CMAKE_CURRENT_SOURCE_DIR}/src/android/EffectContext.cpp
            ${RFX_AIDL_SOURCES})
endif ()

foreach (target resonancefx resonancefx_aidl)
    if (NOT TARGET ${target})
        continue ()
    endif ()
    target_include_directories(${target} PRIVATE src src/core/include)
    target_compile_definitions(${target} PRIVATE
            RFX_VERSION_CODE=${RFX_VERSION_CODE}
            RFX_VERSION_NAME="${RFX_VERSION_NAME}")

    target_compile_options(${target} PRIVATE
            -O3 -DNDEBUG -flto
            -fvisibility=hidden -fvisibility-inlines-hidden
            -ffunction-sections -fdata-sections
            -Wall -Wno-unused-parameter)

    target_link_options(${target} PRIVATE
            -flto -Wl,--gc-sections -Wl,--exclude-libs,ALL
            # 16 KB page alignment: required on Android 15+ devices that use 16 KB pages,
            # harmless on 4 KB devices.
            -Wl,-z,max-page-size=16384
            # Static C++ runtime: the audio HAL's linker namespace can't see libc++_shared.so.
            -static-libstdc++)

    target_link_libraries(${target} PRIVATE log)

    # Strip in release, keeping the dynamic symbol table (AELI / createEffect must stay exported).
    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:${target}>)
endforeach ()
