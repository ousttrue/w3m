#pragma once

extern char TrapSignal;

#define TRAP_ON                                \
    if (TrapSignal) {                          \
        prevtrap = mySignal(SIGINT, KeyAbort); \
    }
#define TRAP_OFF                        \
    if (TrapSignal) {                   \
        if (prevtrap)                   \
            mySignal(SIGINT, prevtrap); \
    }

typedef void (*MySignalHandler)(int);
MySignalHandler mySignal(int signal_number, MySignalHandler action);

void intTrap(int _dummy);
