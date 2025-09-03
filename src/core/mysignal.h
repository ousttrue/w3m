#pragma once
#include <stdbool.h>

extern bool TrapSignal;

#define RETSIGTYPE void
typedef RETSIGTYPE MySignalHandler;
#define SIGNAL_ARG int _dummy /* XXX */
#define SIGNAL_ARGLIST 0 /* XXX */

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
