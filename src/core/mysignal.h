#pragma once
#include <stdbool.h>

extern bool TrapSignal;

#define TRAP_ON                                \
    if (TrapSignal) {                          \
        prevtrap = mySignal(SIGINT, KeyAbort); \
        term_cbreak();                         \
    }
#define TRAP_OFF                        \
    if (TrapSignal) {                   \
        term_raw();                     \
        if (prevtrap)                   \
            mySignal(SIGINT, prevtrap); \
    }

typedef void (*MySignalFunc)(int);
MySignalFunc mySignal(int signal_number, MySignalFunc action);

void reset_signals(void);
