#include "subprocess.h"
#include <signal.h>

SigActionFunc mySignal(int signal_number, SigActionFunc action)
{
    struct sigaction new_action = {
        .sa_handler = action,
        .sa_flags = (signal_number == SIGALRM) ? 0
                                               : SA_RESTART,
    };
    sigemptyset(&new_action.sa_mask);

    struct sigaction old_action;
    sigaction(signal_number, &new_action, &old_action);

    return old_action.sa_handler;
}
