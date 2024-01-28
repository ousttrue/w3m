#pragma once

#define HAVE_SIGSETJMP 1

typedef void (*MySignalHandler)(int);
#define SIGNAL_ARG int _dummy /* XXX */
#define SIGNAL_ARGLIST 0      /* XXX */
#define SIGNAL_RETURN return
MySignalHandler mySignal(int signal_number, MySignalHandler action);
MySignalHandler mySignalInt(MySignalHandler action);
MySignalHandler mySignalGetIgn();

extern char fmInitialized;
extern char QuietMessage;
extern char TrapSignal;

#ifdef HAVE_SIGSETJMP
#define SETJMP(env) sigsetjmp(env, 1)
#define LONGJMP(env, val) siglongjmp(env, val)
#define JMP_BUF sigjmp_buf
#else
#define SETJMP(env) setjmp(env)
#define LONGJMP(env, val) longjmp(env, val)
#define JMP_BUF jmp_buf
#endif

extern void setup_child(int child, int i, int f);
extern void myExec(const char *command);

#define TRAP_ON                                                                \
  if (TrapSignal) {                                                            \
    prevtrap = mySignalInt(KeyAbort);                                          \
    if (fmInitialized)                                                         \
      term_cbreak();                                                           \
  }
#define TRAP_OFF                                                               \
  if (TrapSignal) {                                                            \
    if (fmInitialized)                                                         \
      term_raw();                                                              \
    if (prevtrap)                                                              \
      mySignalInt(prevtrap);                                                   \
  }
