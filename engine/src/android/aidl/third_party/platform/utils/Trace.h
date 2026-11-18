// SPDX-License-Identifier: Apache-2.0
// Minimal utils/Trace.h: atrace macros as no-ops (libutils is not in the NDK).
#pragma once

#define ATRACE_TAG_AUDIO 0
#define ATRACE_NAME(name) ((void)0)
#define ATRACE_CALL() ((void)0)
#define ATRACE_INT(name, value) ((void)0)
#define ATRACE_INT64(name, value) ((void)0)
#define ATRACE_BEGIN(name) ((void)0)
#define ATRACE_END() ((void)0)
#define ATRACE_ASYNC_BEGIN(name, cookie) ((void)0)
#define ATRACE_ASYNC_END(name, cookie) ((void)0)
