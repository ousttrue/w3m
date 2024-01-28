#include "signal_util.h"
#include <signal.h>
#include <fcntl.h>
#include <stdlib.h>

char fmInitialized = 0;
char QuietMessage = 0;
char TrapSignal = 1;

static void reset_signals(void) {
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
  mySignal(SIGCHLD, SIG_IGN);
  mySignal(SIGPIPE, SIG_IGN);
}

#ifndef FOPEN_MAX
#define FOPEN_MAX 1024 /* XXX */
#endif

static void close_all_fds_except(int i, int f) {
  switch (i) { /* fall through */
  case 0:
    dup2(open("/dev/null", O_RDONLY), 0);
  case 1:
    dup2(open("/dev/null", O_WRONLY), 1);
  case 2:
    dup2(open("/dev/null", O_WRONLY), 2);
  }
  /* close all other file descriptors (socket, ...) */
  for (i = 3; i < FOPEN_MAX; i++) {
    if (i != f)
      close(i);
  }
}

#define SETPGRP_VOID 1
#ifdef HAVE_SETPGRP
#ifdef SETPGRP_VOID
#define SETPGRP() setpgrp()
#else
#define SETPGRP() setpgrp(0, 0)
#endif
#else /* no HAVE_SETPGRP; OS/2 EMX */
#define SETPGRP() setpgid(0, 0)
#endif

void setup_child(int child, int i, int f) {
  reset_signals();
  mySignal(SIGINT, SIG_IGN);
  if (!child)
    SETPGRP();
  /*
   * I don't know why but close_tty() sometimes interrupts loadGeneralFile() in
   * loadImage() and corrupt image data can be cached in ~/.w3m.
   */
  close_all_fds_except(i, f);
  QuietMessage = 1;
  fmInitialized = 0;
  TrapSignal = 0;
}

void myExec(const char *command) {
  mySignal(SIGINT, SIG_DFL);
  execl("/bin/sh", "sh", "-c", command, 0);
  exit(127);
}

void (*mySignal(int signal_number, void (*action)(int)))(int) {
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

MySignalHandler mySignalInt(MySignalHandler action) {
  return mySignal(SIGINT, action);
}

MySignalHandler mySignalGetIgn() { return SIG_IGN; }
