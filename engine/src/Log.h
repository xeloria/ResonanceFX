// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria
#pragma once

#if defined(__ANDROID__)
#include <android/log.h>
#define RFX_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "ResonanceFX", __VA_ARGS__)
#define RFX_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ResonanceFX", __VA_ARGS__)
#else
#include <cstdio>
#include <cstdlib>
#define RFX_LOGI(...) do { if (std::getenv("RFX_TEST_VERBOSE")) { std::fprintf(stderr, "[ResonanceFX] " __VA_ARGS__); std::fputc('\n', stderr); } } while (0)
#define RFX_LOGE(...) do { std::fprintf(stderr, "[ResonanceFX] " __VA_ARGS__); std::fputc('\n', stderr); } while (0)
#endif
