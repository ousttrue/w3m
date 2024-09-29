#include "trap_jmp.h"
#include "terms.h"
#include <setjmp.h>
#include <signal.h>

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

bool from_jmp() { return SETJMP(AbortLoading) != 0; }

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

MySignalHandler (*prevtrap)(SIGNAL_ARG) = NULL;

void trap_on() {
  //
  TRAP_ON;
}
