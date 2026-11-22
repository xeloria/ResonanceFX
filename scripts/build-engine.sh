#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
# Builds libresonancefx.so for every Android ABI into out/engine/<abi>/.
#
#   ANDROID_SDK  path to the Android SDK (default: $ANDROID_HOME or %LOCALAPPDATA%/Android/Sdk)
#   NDK_VERSION  NDK under $ANDROID_SDK/ndk to use (default: newest installed)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK="${ANDROID_SDK:-${ANDROID_HOME:-${LOCALAPPDATA:-$HOME}/Android/Sdk}}"
NDK_VERSION="${NDK_VERSION:-$(ls "$SDK/ndk" | sort -V | tail -1)}"
NDK="$SDK/ndk/$NDK_VERSION"
CMAKE_DIR="$(ls -d "$SDK"/cmake/* | sort -V | tail -1)/bin"
VERSION_CODE="${VERSION_CODE:-20501}"
VERSION_NAME="${VERSION_NAME:-2.5.1}"

# minSdk 21 (Android 5.0) for every ABI: the oldest release the module supports.
ABIS=(arm64-v8a armeabi-v7a x86_64 x86)

# AIDL effect HAL front-end (Android 16+). Opt-in: it needs the platform's frozen interface
# library for the ABI, which is not part of the NDK. Point RFX_DEVICE_LIBS at a directory
# containing <abi>/android.hardware.audio.effect-V3-ndk.so (pull it from the target device:
# /system/lib64 or /system/lib). RFX_AIDL_ALLOW_UNDEFINED=1 builds it with the interface
# symbols left unresolved (development only).
AIDL_API="${AIDL_API:-36}"
AIDL_IFACE="${AIDL_IFACE:-android.hardware.audio.effect-V3-ndk.so}"
DEVICE_LIBS="${RFX_DEVICE_LIBS:-$ROOT/build/device-libs}"

for ABI in "${ABIS[@]}"; do
    OUT="$ROOT/out/engine/$ABI"
    mkdir -p "$OUT"

    # Legacy AIDL (AELI): API 21, what HIDL effect HALs and older Android releases load.
    BUILD="$ROOT/build/engine/$ABI"
    "$CMAKE_DIR/cmake" -S "$ROOT/engine" -B "$BUILD" -G Ninja \
        -DCMAKE_MAKE_PROGRAM="$CMAKE_DIR/ninja" \
        -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$ABI" \
        -DANDROID_PLATFORM=android-21 \
        -DANDROID_ARM_NEON=ON \
        -DANDROID_STL=c++_static \
        -DCMAKE_BUILD_TYPE=Release \
        -DRFX_VERSION_CODE="$VERSION_CODE" \
        -DRFX_VERSION_NAME="$VERSION_NAME" >/dev/null
    "$CMAKE_DIR/cmake" --build "$BUILD" --target resonancefx >/dev/null
    cp "$BUILD/libresonancefx.so" "$OUT/"

    # AIDL effect front-end: needs API 29+ binder NDK, and the platform interface library.
    AIDL_ARGS=()
    PREBUILT="$ROOT/engine/src/android/aidl/prebuilt/$ABI/$AIDL_IFACE"
    if [ -f "$DEVICE_LIBS/$ABI/$AIDL_IFACE" ]; then
        AIDL_ARGS=(-DRFX_EFFECT_NDK_LIB="$DEVICE_LIBS/$ABI/$AIDL_IFACE")
    elif [ -f "$PREBUILT" ]; then
        AIDL_ARGS=(-DRFX_EFFECT_NDK_LIB="$PREBUILT")
    elif [ "${RFX_AIDL_ALLOW_UNDEFINED:-0}" = "1" ]; then
        AIDL_ARGS=(-DRFX_AIDL_ALLOW_UNDEFINED=ON)
    fi
    if [ "${#AIDL_ARGS[@]}" -gt 0 ]; then
        AIDL_BUILD="$ROOT/build/engine-aidl/$ABI"
        "$CMAKE_DIR/cmake" -S "$ROOT/engine" -B "$AIDL_BUILD" -G Ninja \
            -DCMAKE_MAKE_PROGRAM="$CMAKE_DIR/ninja" \
            -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
            -DANDROID_ABI="$ABI" \
            -DANDROID_PLATFORM=android-31 \
            -DANDROID_ARM_NEON=ON \
            -DANDROID_STL=c++_static \
            -DCMAKE_BUILD_TYPE=Release \
            -DRFX_VERSION_CODE="$VERSION_CODE" \
            -DRFX_VERSION_NAME="$VERSION_NAME" \
            "${AIDL_ARGS[@]}" >/dev/null
        "$CMAKE_DIR/cmake" --build "$AIDL_BUILD" --target resonancefx_aidl >/dev/null
        cp "$AIDL_BUILD/libresonancefx_aidl.so" "$OUT/libresonancefx_aidl_${AIDL_API}.so"
    fi
    echo "built $ABI -> out/engine/$ABI/"
done
