#pragma once
#include "signal_util.h"
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif
#include <setjmp.h>

#define SETJMP(env) sigsetjmp(env, 1)
#define LONGJMP(env, val) siglongjmp(env, val)

extern sigjmp_buf AbortLoading;

MySignalHandler KeyAbort(SIGNAL_ARG);

#define TRAP_ON                                \
    if (TrapSignal) {                          \
        prevtrap = mySignal(SIGINT, KeyAbort); \
        if (fmInitialized)                     \
            term_cbreak();                     \
    }
#define TRAP_OFF                        \
    if (TrapSignal) {                   \
        if (fmInitialized)              \
            term_raw();                 \
        if (prevtrap)                   \
            mySignal(SIGINT, prevtrap); \
    }
