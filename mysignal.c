#include "mysignal.h"

static JMP_BUF IntReturn;

static JMP_BUF AbortLoading;

MySignalHandler intTrap(SIGNAL_ARG)
{
    LONGJMP(IntReturn, 0);
    SIGNAL_RETURN;
}

MySignalHandler KeyAbort(SIGNAL_ARG)
{
    LONGJMP(AbortLoading, 1);
    SIGNAL_RETURN;
}

void (*mySignal(int signal_number, void (*action)(int)))(int)
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

MySignalHandler SigPipe(SIGNAL_ARG)
{
    mySignal(SIGPIPE, SigPipe);
    SIGNAL_RETURN;
}

static MySignalHandler reset_exit_with_value(SIGNAL_ARG, int rval)
{
    // exitRawMode();
    // w3m_exit(rval);
    SIGNAL_RETURN;
}

MySignalHandler reset_error_exit(SIGNAL_ARG)
{
    reset_exit_with_value(SIGNAL_ARGLIST, 1);
}

MySignalHandler
reset_exit(SIGNAL_ARG)
{
    reset_exit_with_value(SIGNAL_ARGLIST, 0);
}

MySignalHandler
error_dump(SIGNAL_ARG)
{
    mySignal(SIGIOT, SIG_DFL);
    // exitRawMode();
    // abort();
    SIGNAL_RETURN;
}

void set_int(void)
{
    mySignal(SIGHUP, reset_exit);
    mySignal(SIGINT, reset_exit);
    mySignal(SIGQUIT, reset_exit);
    mySignal(SIGTERM, reset_exit);
    mySignal(SIGILL, error_dump);
    mySignal(SIGIOT, error_dump);
    mySignal(SIGFPE, error_dump);
#ifdef SIGBUS
    mySignal(SIGBUS, error_dump);
#endif /* SIGBUS */
    /* mySignal(SIGSEGV, error_dump); */
}

#ifndef SIGIOT
#define SIGIOT SIGABRT
#endif /* not SIGIOT */

void reset_signals(void)
{
#ifdef SIGHUP
    mySignal(SIGHUP, SIG_DFL); /* terminate process */
#endif
    mySignal(SIGINT, SIG_DFL); /* terminate process */
#ifdef SIGQUIT
    mySignal(SIGQUIT, SIG_DFL); /* terminate process */
#endif
    mySignal(SIGTERM, SIG_DFL); /* terminate process */
    mySignal(SIGILL, SIG_DFL); /* create core image */
    mySignal(SIGIOT, SIG_DFL); /* create core image */
    mySignal(SIGFPE, SIG_DFL); /* create core image */
#ifdef SIGBUS
    mySignal(SIGBUS, SIG_DFL); /* create core image */
#endif /* SIGBUS */
#ifdef SIGCHLD
    mySignal(SIGCHLD, SIG_IGN);
#endif
#ifdef SIGPIPE
    mySignal(SIGPIPE, SIG_IGN);
#endif
}

void set_alarm(const char *data)
{
    // int sec = 0;
    // int cmd = -1;
    // if (*data != '\0') {
    //     sec = atoi(getWord(&data));
    //     if (sec > 0)
    //         cmd = getFuncList(getWord(&data));
    // }
    // if (cmd >= 0) {
    //     data = getQWord(&data);
    //     setAlarmEvent(&DefaultAlarm, sec, AL_EXPLICIT, cmd, data);
    //     disp_message_nsec(Sprintf("%dsec %s %s", sec, w3mFuncList[cmd].id,
    //                           data)
    //                           ->ptr,
    //         FALSE, 1, FALSE, TRUE);
    // } else {
    //     setAlarmEvent(&DefaultAlarm, 0, AL_UNSET, FUNCNAME_nulcmd, NULL);
    // }
}
