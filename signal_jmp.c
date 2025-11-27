#include "signal_jmp.h"
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>

char TrapSignal = true;

MySignalHandler mySignal(int signal_number, MySignalHandler action)
{
#ifdef SA_RESTART
    struct sigaction new_action, old_action;

    sigemptyset(&new_action.sa_mask);
    new_action.sa_handler = action;
    if (signal_number == SIGALRM) {
#ifdef SA_INTERRUPT
        new_action.sa_flags = SA_INTERRUPT;
#else
        new_action.sa_flags = 0;
#endif
    } else {
        new_action.sa_flags = SA_RESTART;
    }
    sigaction(signal_number, &new_action, &old_action);
    return (old_action.sa_handler);
#else
    return (signal(signal_number, action));
#endif
}

#ifndef SIGIOT
#define SIGIOT SIGABRT
#endif /* not SIGIOT */

#ifndef FOPEN_MAX
#define FOPEN_MAX 1024 /* XXX */
#endif
