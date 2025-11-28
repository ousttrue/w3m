/* $Id: hash.c,v 1.5 2003/04/07 16:27:10 ukai Exp $ */
#include <string.h>
#include "hash.h"
#include <gc.h>

#define keycomp(x, y) ((x) == (y))

    /* XXX: we assume sizeof(unsigned long) >= sizeof(void *) */
    static unsigned long hashfunc(HashItem_ss* x)
{
    return (unsigned long)x;
}

/* *INDENT-OFF* */
defhashfunc(HashItem_ss*, int, hss_i)
    /* *INDENT-ON* */

