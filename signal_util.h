#pragma once

#define HAVE_SIGSETJMP 1

#define RETSIGTYPE void
typedef RETSIGTYPE MySignalHandler;
#define SIGNAL_ARG int _dummy	/* XXX */
#define SIGNAL_ARGLIST 0	/* XXX */
#define SIGNAL_RETURN return

#ifdef HAVE_SIGSETJMP
# define SETJMP(env) sigsetjmp(env,1)
# define LONGJMP(env,val) siglongjmp(env,val)
# define JMP_BUF sigjmp_buf
#else
# define SETJMP(env) setjmp(env)
# define LONGJMP(env,val) longjmp(env, val)
# define JMP_BUF jmp_buf
#endif

extern char fmInitialized;
extern char QuietMessage;
extern char TrapSignal;

extern MySignalHandler intTrap(SIGNAL_ARG);
extern MySignalHandler reset_exit(SIGNAL_ARG);
extern MySignalHandler error_dump(SIGNAL_ARG);

#define TRAP_ON                                                                \
  if (TrapSignal) {                                                            \
    prevtrap = mySignal(SIGINT, KeyAbort);                                     \
    if (fmInitialized)                                                         \
      term_cbreak();                                                           \
  }
#define TRAP_OFF                                                               \
  if (TrapSignal) {                                                            \
    if (fmInitialized)                                                         \
      term_raw();                                                              \
    if (prevtrap)                                                              \
      mySignal(SIGINT, prevtrap);                                              \
  }

