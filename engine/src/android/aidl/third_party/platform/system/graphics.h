// SPDX-License-Identifier: Apache-2.0
// Minimal system/graphics.h: thread_defs.h needs HAL_PRIORITY_URGENT_DISPLAY, and the
// graphics HAL headers are not part of the NDK.
#pragma once

#define HAL_PRIORITY_URGENT_DISPLAY (-8)
