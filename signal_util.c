#include "signal_util.h"
#include "global.h"
#include "terms.h"
#include "main.h"
#include "buffer.h"
#include "anchor.h"
#include "alloc.h"

#include <stdlib.h>

sigjmp_buf IntReturn;

sigjmp_buf AbortLoading;

static void reset_exit_with_value(int _, int rval)
{
    tty_reset();
    w3m_exit(rval);
}

// void reset_error_exit(int _)
// {
//     reset_exit_with_value(0, 1);
// }

void reset_exit(int _)
{
    reset_exit_with_value(0, 0);
}

void error_dump(int _)
{
    signal(SIGIOT, SIG_DFL);
    tty_reset();
    abort();
}

void intTrap(int _)
{ /* Interrupt catcher */
    LONGJMP(IntReturn, 0);
}

void KeyAbort(int _)
{
    LONGJMP(AbortLoading, 1);
}

static int
cmp_anchor_hseq(const void* a, const void* b)
{
    return (*((const struct Anchor**)a))->hseq - (*((const struct Anchor**)b))->hseq;
}

void do_dump(struct Buffer* buf)
{
    SignalFunc prevtrap = NULL;

    prevtrap = signal(SIGINT, intTrap);
    if (SETJMP(IntReturn) != 0) {
        signal(SIGINT, prevtrap);
        return;
    }

    signal(SIGINT, prevtrap);
}
