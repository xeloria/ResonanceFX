// SPDX-License-Identifier: Apache-2.0
// Minimal utils/Errors.h: the status_t type and the codes the FMQ/EventFlag headers use.
// AOSP's real header lives in frameworks/native (libbinder) and pulls in libbase.
#pragma once

#include <cstdint>

namespace android {

using status_t = int32_t;

constexpr status_t NO_ERROR = 0;
constexpr status_t OK = 0;
constexpr status_t PERMISSION_DENIED = -1;
constexpr status_t NAME_NOT_FOUND = -2;
constexpr status_t NO_MEMORY = -12;
constexpr status_t WOULD_BLOCK = -11;
constexpr status_t BAD_VALUE = -22;
constexpr status_t DEAD_OBJECT = -32;
constexpr status_t INVALID_OPERATION = -38;
constexpr status_t TIMED_OUT = -110;
constexpr status_t UNKNOWN_ERROR = -2147483647 - 1;

}  // namespace android
