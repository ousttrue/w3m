#pragma once
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif
#include <signal.h>
#include <setjmp.h>

typedef void (*SignalFunc)(int);

extern sigjmp_buf AbortLoading;
extern sigjmp_buf IntReturn;

void reset_exit(int);
void error_dump(int);
void intTrap(int);

#define SETJMP(env) sigsetjmp(env, 1)
#define LONGJMP(env, val) siglongjmp(env, val)

void KeyAbort(int _);

#define TRAP_ON                              \
    if (TrapSignal) {                        \
        prevtrap = signal(SIGINT, KeyAbort); \
        if (fmInitialized)                   \
            term_cbreak();                   \
    }
#define TRAP_OFF                      \
    if (TrapSignal) {                 \
        if (fmInitialized)            \
            term_raw();               \
        if (prevtrap)                 \
            signal(SIGINT, prevtrap); \
    }

struct Buffer;
void do_dump(struct Buffer* buf);
