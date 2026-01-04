#pragma once
#include <setjmp.h>
#include <signal.h>

#ifdef HAVE_SIGSETJMP
#ifdef __MINGW32_VERSION
#define SETJMP(env) setjmp(env)
#define LONGJMP(env, val) longjmp(env, val)
#define JMP_BUF jmp_buf
#else
#define SETJMP(env) sigsetjmp(env, 1)
#define LONGJMP(env, val) siglongjmp(env, val)
#define JMP_BUF sigjmp_buf
#endif /* __MINGW32_VERSION */
#else
#define SETJMP(env) setjmp(env)
#define LONGJMP(env, val) longjmp(env, val)
#define JMP_BUF jmp_buf
#endif

#define RETSIGTYPE void
typedef RETSIGTYPE MySignalHandler;
#define SIGNAL_ARG int _dummy /* XXX */
#define SIGNAL_ARGLIST 0 /* XXX */
#define SIGNAL_RETURN return

typedef MySignalHandler (*PrevTrapFunc)(SIGNAL_ARG);
MySignalHandler intTrap(SIGNAL_ARG);
MySignalHandler KeyAbort(SIGNAL_ARG);
void reset_signals(void);
void set_int(void);
void set_alarm(const char *data);
