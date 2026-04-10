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
    reset_tty();
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
    reset_tty();
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
    if (w3m_dump & DUMP_EXTRA)
        dump_extra(buf);
    if (w3m_dump & DUMP_HEAD)
        dump_head(buf);
    if (w3m_dump & DUMP_SOURCE)
        dump_source(buf);
    if (w3m_dump == DUMP_BUFFER) {
        int i;
        saveBuffer(buf, stdout, false);
        if (displayLinkNumber && buf->href) {
            int nanchor = buf->href->nanchor;
            printf("\nReferences:\n\n");
            struct Anchor** in_order = New_N(struct Anchor*, buf->href->nanchor);
            for (i = 0; i < nanchor; i++)
                in_order[i] = buf->href->anchors + i;
            qsort(in_order, nanchor, sizeof(struct Anchor*), cmp_anchor_hseq);
            for (i = 0; i < nanchor; i++) {
                struct Url pu;
                char* url;
                if (in_order[i]->slave)
                    continue;
                pu = parseURL2(in_order[i]->url, baseURL(buf));
                url = url_decode2(parsedURL2Str(&pu)->ptr, Currentbuf);
                printf("[%d] %s\n", in_order[i]->hseq + 1, url);
            }
        }
    }
    signal(SIGINT, prevtrap);
}
