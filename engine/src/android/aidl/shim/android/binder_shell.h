// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

// Shim for android/binder_shell.h, which ships in platform builds but not in the NDK
// sysroot. Its presence enables ICInterface::handleShellCommand, which occupies a vtable
// slot before the interface methods. Vendor effect stubs were built in-tree with that
// slot present, so the layout must match or the HAL's dispatch lands one method late.
#pragma once

#include <android/binder_ibinder.h>

__BEGIN_DECLS

typedef binder_status_t (*AIBinder_ShellCommand_onTransact)(AIBinder* binder, int in, int out,
                                                            int err, const char** argv,
                                                            uint32_t argc);

void AIBinder_Class_setHandleShellCommand(AIBinder_Class* clazz,
                                         AIBinder_ShellCommand_onTransact onShellCommand);

__END_DECLS
