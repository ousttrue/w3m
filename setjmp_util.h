#pragma once
#include "signal_util.h"
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif
#include <setjmp.h>

#define SETJMP(env) sigsetjmp(env, 1)
#define LONGJMP(env, val) siglongjmp(env, val)

extern sigjmp_buf AbortLoading;

void KeyAbort(int _);

#define TRAP_ON                                \
    if (TrapSignal) {                          \
        prevtrap = signal(SIGINT, KeyAbort); \
        if (fmInitialized)                     \
            tty_cbreak();                     \
    }
#define TRAP_OFF                        \
    if (TrapSignal) {                   \
        if (fmInitialized)              \
            tty_raw();                 \
        if (prevtrap)                   \
            signal(SIGINT, prevtrap); \
    }
