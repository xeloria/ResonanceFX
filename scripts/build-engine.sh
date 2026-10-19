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
VERSION_CODE="${VERSION_CODE:-20000}"
VERSION_NAME="${VERSION_NAME:-2.0.0}"

# minSdk 21 (Android 5.0) for every ABI: the oldest release the module supports.
ABIS=(arm64-v8a armeabi-v7a x86_64 x86)

for ABI in "${ABIS[@]}"; do
    BUILD="$ROOT/build/engine/$ABI"
    OUT="$ROOT/out/engine/$ABI"
    mkdir -p "$BUILD" "$OUT"
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
    "$CMAKE_DIR/cmake" --build "$BUILD" >/dev/null
    cp "$BUILD/libresonancefx.so" "$OUT/libresonancefx.so"
    echo "built $ABI -> out/engine/$ABI/libresonancefx.so"
done
