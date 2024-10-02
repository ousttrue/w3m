#include "trap_jmp.h"
#include "terms.h"
#include <setjmp.h>
#include <signal.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

bool TrapSignal = true;

#ifdef _WIN32
#define SETJMP(env) setjmp(env)
#define LONGJMP(env, val) longjmp(env, val)
#define JMP_BUF jmp_buf
#else
#define SETJMP(env) sigsetjmp(env, 1)
#define LONGJMP(env, val) siglongjmp(env, val)
#define JMP_BUF sigjmp_buf
#endif

#define RETSIGTYPE void
typedef RETSIGTYPE MySignalHandler;
#define SIGNAL_ARG int _dummy /* XXX */
#define SIGNAL_ARGLIST 0      /* XXX */

static JMP_BUF AbortLoading;
static MySignalHandler KeyAbort(SIGNAL_ARG) { LONGJMP(AbortLoading, 1); }
// static MySignalHandler reset_exit_with_value(SIGNAL_ARG, int rval) {
//   term_reset();
//   w3m_exit(rval);
// }
//
// static MySignalHandler reset_error_exit(SIGNAL_ARG) {
//   reset_exit_with_value(SIGNAL_ARGLIST, 1);
// }

static MySignalHandler reset_exit(SIGNAL_ARG) {
  // reset_exit_with_value(SIGNAL_ARGLIST, 0);
}

static MySignalHandler error_dump(SIGNAL_ARG) {
  // mySignal(SIGIOT, SIG_DFL);
  // term_reset();
  // abort();
}

// MySignalHandler intTrap(SIGNAL_ARG) { /* Interrupt catcher */
//   LONGJMP(IntReturn, 0);
// }

static MySignalHandler resize_hook(SIGNAL_ARG) {
//   need_resize_screen = true;
// #ifndef _WIN32
//   mySignal(SIGWINCH, resize_hook);
// #endif
}

// static MySignalHandler SigAlarm(SIGNAL_ARG);
// JMP_BUF IntReturn;

#define TRAP_ON                                                                \
  if (TrapSignal) {                                                            \
    prevtrap = mySignal(SIGINT, KeyAbort);                                     \
    term_cbreak();                                                             \
  }
#define TRAP_OFF                                                               \
  if (TrapSignal) {                                                            \
    term_raw();                                                                \
    if (prevtrap)                                                              \
      mySignal(SIGINT, prevtrap);                                              \
  }

static void (*mySignal(int signal_number, void (*action)(int)))(int) {
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

//
// signal
//
#ifndef SIGIOT
#define SIGIOT SIGABRT
#endif /* not SIGIOT */

void signals_init() {
#if _WIN32
#else
  mySignal(SIGHUP, reset_exit);
  mySignal(SIGQUIT, reset_exit);
#endif

  mySignal(SIGINT, reset_exit);
  mySignal(SIGTERM, reset_exit);
  mySignal(SIGILL, error_dump);
  mySignal(SIGIOT, error_dump);
  mySignal(SIGFPE, error_dump);

#ifdef SIGBUS
  mySignal(SIGBUS, error_dump);
#endif /* SIGBUS */
       /* mySignal(SIGSEGV, error_dump); */
}

void signals_reset() {
#ifdef SIGHUP
  mySignal(SIGHUP, SIG_DFL); /* terminate process */
#endif
  mySignal(SIGINT, SIG_DFL); /* terminate process */
#ifdef SIGQUIT
  mySignal(SIGQUIT, SIG_DFL); /* terminate process */
#endif
  mySignal(SIGTERM, SIG_DFL); /* terminate process */
  mySignal(SIGILL, SIG_DFL);  /* create core image */
  mySignal(SIGIOT, SIG_DFL);  /* create core image */
  mySignal(SIGFPE, SIG_DFL);  /* create core image */
#ifdef SIGBUS
  mySignal(SIGBUS, SIG_DFL); /* create core image */
#endif                       /* SIGBUS */
#ifndef _WIN32
  mySignal(SIGCHLD, SIG_IGN);
  mySignal(SIGPIPE, SIG_IGN);
#endif
}

void signal_int_default() { mySignal(SIGINT, SIG_DFL); }

//
// setjmp
//
bool from_jmp() { return SETJMP(AbortLoading) != 0; }

MySignalHandler (*prevtrap)(SIGNAL_ARG) = NULL;

void trap_on() { TRAP_ON; }

void trap_off() { TRAP_OFF; }

#define SETPGRP_VOID 1
#ifdef _WIN32
#define SETPGRP()
#else
#ifdef HAVE_SETPGRP
#ifdef SETPGRP_VOID
#define SETPGRP() setpgrp()
#else
#define SETPGRP() setpgrp(0, 0)
#endif
#else /* no HAVE_SETPGRP; OS/2 EMX */
#define SETPGRP() setpgid(0, 0)
#endif
#endif

static void close_all_fds_except(int i, int f) {
  switch (i) { /* fall through */
  case 0:
    dup2(open(DEV_NULL_PATH, O_RDONLY), 0);
  case 1:
    dup2(open(DEV_NULL_PATH, O_WRONLY), 1);
  case 2:
    dup2(open(DEV_NULL_PATH, O_WRONLY), 2);
  }
  /* close all other file descriptors (socket, ...) */
  for (i = 3; i < FOPEN_MAX; i++) {
    if (i != f)
      close(i);
  }
}

void setup_child(int child, int i, int f) {
  signals_reset();
  mySignal(SIGINT, SIG_IGN);
  if (!child)
    SETPGRP();
  /*
   * I don't know why but close_tty() sometimes interrupts loadGeneralFile()
   * in loadImage() and corrupt image data can be cached in ~/.w3m.
   */
  close_all_fds_except(i, f);
  QuietMessage = true;
  // fmInitialized = false;
  TrapSignal = false;
}
