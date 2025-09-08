#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* strcasestr() */
#include <string.h>
#endif

#include "url.h"
#include <stdio.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <stdlib.h>
#include "indep.h"
#include "Str.h"
#include <gc.h>
#include "myctype.h"
#include "entity.h"

unsigned char QUOTE_MAP[0x100] = {
    /* NUL SOH STX ETX EOT ENQ ACK BEL  BS  HT  LF  VT  FF  CR  SO  SI */
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    /* DLE DC1 DC2 DC3 DC4 NAK SYN ETB CAN  EM SUB ESC  FS  GS  RS  US */
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    /* SPC   !   "   #   $   %   &   '   (   )   *   +   ,   -   .   / */
    24,
    72,
    76,
    40,
    8,
    40,
    41,
    77,
    72,
    72,
    72,
    40,
    72,
    8,
    0,
    64,
    /*   0   1   2   3   4   5   6   7   8   9   :   ;   <   =   >   ? */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    32,
    72,
    74,
    72,
    75,
    40,
    /*   @   A   B   C   D   E   F   G   H   I   J   K   L   M   N   O */
    72,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /*   P   Q   R   S   T   U   V   W   X   Y   Z   [   \   ]   ^   _ */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    72,
    72,
    72,
    72,
    0,
    /*   `   a   b   c   d   e   f   g   h   i   j   k   l   m   n   o */
    72,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /*   p   q   r   s   t   u   v   w   x   y   z   {   |   }   ~ DEL */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    72,
    72,
    72,
    72,
    24,

    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
};

static int
strcasematch(char* s1, char* s2)
{
    int x;
    while (*s1) {
        if (*s2 == '\0')
            return 1;
        x = TOLOWER(*s1) - TOLOWER(*s2);
        if (x != 0)
            break;
        s1++;
        s2++;
    }
    return (*s2 == '\0');
}

/* search multiple strings */
int strcasemstr(char* str, char* srch[], char** ret_ptr)
{
    int i;
    while (*str) {
        for (i = 0; srch[i]; i++) {
            if (strcasematch(str, srch[i])) {
                if (ret_ptr)
                    *ret_ptr = str;
                return i;
            }
        }
        str++;
    }
    return -1;
}

int strmatchlen(const char* s1, const char* s2, int maxlen)
{
    int i;

    /* To allow the maxlen to be negatie (infinity),
     * compare by "!=" instead of "<=". */
    for (i = 0; i != maxlen; ++i) {
        if (!s1[i] || !s2[i] || s1[i] != s2[i])
            break;
    }
    return i;
}

/* Local Variables:    */
/* c-basic-offset: 4   */
/* tab-width: 8        */
/* End:                */
