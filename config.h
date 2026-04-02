#pragma once

#define USE_SSL 1
#define USE_SSL_VERIFY 1
#define USE_IMAGE 1

typedef void MySignalHandler;
#define SIGNAL_ARG int _dummy /* XXX */
#define SIGNAL_ARGLIST 0 /* XXX */
#define SIGNAL_RETURN return

#define HAVE_SIGSETJMP 1
#define SETJMP(env) sigsetjmp(env, 1)
#define LONGJMP(env, val) siglongjmp(env, val)
#define JMP_BUF sigjmp_buf
