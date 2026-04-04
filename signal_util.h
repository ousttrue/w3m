#pragma once

typedef void MySignalHandler;
#define SIGNAL_ARG int _dummy /* XXX */
#define SIGNAL_ARGLIST 0 /* XXX */
#define SIGNAL_RETURN return

void (*mySignal(int signal_number, void (*action)(int)))(int);

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

MySignalHandler reset_exit(SIGNAL_ARG);
MySignalHandler error_dump(SIGNAL_ARG);
MySignalHandler reset_exit(SIGNAL_ARG);
MySignalHandler error_dump(SIGNAL_ARG);
MySignalHandler intTrap(SIGNAL_ARG);
