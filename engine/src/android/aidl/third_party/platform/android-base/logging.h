// SPDX-License-Identifier: Apache-2.0
// Minimal android-base/logging.h for the out-of-tree AIDL effect build.
//
// AOSP's header needs libbase, which the audio HAL's linker namespace exposes but which is not
// part of the NDK. The vendored EffectImpl only uses the LOG() stream, so this shim forwards it
// to liblog instead.
#pragma once

#include <android/log.h>

#include <cstdlib>
#include <ostream>
#include <sstream>
#include <string>

namespace android::base {

enum LogSeverity {
    VERBOSE = ANDROID_LOG_VERBOSE,
    DEBUG = ANDROID_LOG_DEBUG,
    INFO = ANDROID_LOG_INFO,
    WARNING = ANDROID_LOG_WARN,
    ERROR = ANDROID_LOG_ERROR,
    FATAL = ANDROID_LOG_FATAL,
};

class LogMessage {
  public:
    LogMessage(LogSeverity severity, const char* file, int line)
        : severity_(severity), file_(file), line_(line) {}

    ~LogMessage() {
        const std::string text = stream_.str();
        __android_log_print(severity_, "ResonanceFX.Aidl", "%s", text.c_str());
        if (severity_ == FATAL) std::abort();
    }

    template <typename T>
    LogMessage& operator<<(const T& value) {
        stream_ << value;
        return *this;
    }

    LogMessage& operator<<(std::ostream& (*manipulator)(std::ostream&)) {
        stream_ << manipulator;
        return *this;
    }

  private:
    LogSeverity severity_;
    const char* file_;
    int line_;
    std::ostringstream stream_;
};

}  // namespace android::base

#define LOG(severity) \
    ::android::base::LogMessage(::android::base::severity, __FILE__, __LINE__)
#define LOG_ALWAYS_FATAL(...)                                                     \
    do {                                                                          \
        __android_log_print(ANDROID_LOG_FATAL, "ResonanceFX.Aidl", __VA_ARGS__);  \
        std::abort();                                                             \
    } while (0)
#define LOG_ALWAYS_FATAL_IF(condition, ...)                                       \
    do {                                                                          \
        if (condition) {                                                          \
            __android_log_print(ANDROID_LOG_FATAL, "ResonanceFX.Aidl", __VA_ARGS__); \
            std::abort();                                                         \
        }                                                                         \
    } while (0)

#ifndef FALLTHROUGH_INTENDED
#define FALLTHROUGH_INTENDED [[fallthrough]]
#endif


extern "C" int android_errorWriteLog(int tag, const char* info);

#define CHECK(condition) \
    if (condition) { \
    } else \
        ::android::base::LogMessage(::android::base::FATAL, __FILE__, __LINE__)

