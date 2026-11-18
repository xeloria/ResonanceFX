// SPDX-License-Identifier: Apache-2.0
// Minimal utils/SystemClock.h: monotonic clocks used by the FMQ wait loops.
// AOSP's version lives in libutils, which is not in the NDK.
#pragma once

#include <ctime>

#include <utils/Errors.h>

namespace android {

inline int64_t elapsedRealtimeNano() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

inline int64_t elapsedRealtime() { return elapsedRealtimeNano() / 1000000LL; }
inline int64_t uptimeNanos() { return elapsedRealtimeNano(); }
inline int64_t uptimeMillis() { return elapsedRealtime(); }

}  // namespace android
