#include "myctype.h"
#include <string.h>

/* $Id: myctype.c,v 1.7 2003/09/22 21:02:20 ukai Exp $ */
static enum MYCTYPE_TYPES MYCTYPE_MAP[0x100] = {
    /* NUL SOH STX ETX EOT ENQ ACK BEL   BS  HT  LF  VT  FF  CR  SO  SI */
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    3,
    3,
    3,
    3,
    3,
    1,
    1,
    /* DLE DC1 DC2 DC3 DC4 NAK SYN ETB CAN   EM SUB ESC  FS  GS  RS  US */
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    /* SPC   !   "   #   $   %   &   '    (   )   *   +   ,   -   .   / */
    18,
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
    /*   0   1   2   3   4   5   6   7    8   9   :   ;   <   =   >   ? */
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
    16,
    16,
    16,
    16,
    16,
    16,
    /*   @   A   B   C   D   E   F   G    H   I   J   K   L   M   N   O */
    16,
    52,
    52,
    52,
    52,
    52,
    52,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    /*   P   Q   R   S   T   U   V   W    X   Y   Z   [   \   ]   ^   _ */
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    16,
    16,
    16,
    16,
    16,
    /*   `   a   b   c   d   e   f   g    h   i   j   k   l   m   n   o */
    16,
    52,
    52,
    52,
    52,
    52,
    52,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    /*   p   q   r   s   t   u   v   w    x   y   z   {   |   }   ~ DEL */
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    20,
    16,
    16,
    16,
    16,
    1,

    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
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
    0,
    0,
    0,
    0,
    0,
};
enum MYCTYPE_TYPES GET_MYCTYPE(uint8_t x) { return (MYCTYPE_MAP[(int)(unsigned char)(x)]); }

unsigned char MYCTYPE_DIGITMAP[0x100] = {
    /* NUL SOH STX ETX EOT ENQ ACK BEL   BS  HT  LF  VT  FF  CR  SO  SI */
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    /* DLE DC1 DC2 DC3 DC4 NAK SYN ETB CAN   EM SUB ESC  FS  GS  RS  US */
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    /* SPC   !   "   #   $   %   &   '    (   )   *   +   ,   -   .   / */
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    /*   0   1   2   3   4   5   6   7    8   9   :   ;   <   =   >   ? */
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    255,
    255,
    255,
    255,
    255,
    255,
    /*   @   A   B   C   D   E   F   G    H   I   J   K   L   M   N   O */
    255,
    10,
    11,
    12,
    13,
    14,
    15,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    /*   P   Q   R   S   T   U   V   W    X   Y   Z   [   \   ]   ^   _ */
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    /*   `   a   b   c   d   e   f   g    h   i   j   k   l   m   n   o */
    255,
    10,
    11,
    12,
    13,
    14,
    15,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    /*   p   q   r   s   t   u   v   w    x   y   z   {   |   }   ~ DEL */

    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
};
uint8_t GET_MYCDIGIT(uint8_t x) { return (MYCTYPE_DIGITMAP[(int)(unsigned char)(x)]); }

bool str_to_bool(const char* value, bool old)
{
    if (value == 0)
        return true;

    switch (TOLOWER(value[0])) {
    case '0':
    case 'f': // false
    case 'n': // no
    case 'u': // undef
        return false;

    case 'o':
        if (TOLOWER(value[1]) == 'f')
            // off
            return false;
        // on
        return true;

    case 't':
        if (TOLOWER(value[1]) == 'o')
            // toggle
            return !old;
        // true
        return true;

    case '!':
    case 'r': // reverse
    case 'x': // exchange
        return !old;
    }
    return true;
}

int vscpf(const char* fmt, va_list ap)
{
    int len = 0;
    int status = SP_NORMAL;
    int p = 0;
    for (const char* f = fmt; *f; f++) {
    redo:
        switch (status) {
        case SP_NORMAL:
            if (*f == '%') {
                status = SP_PREC;
                p = 0;
            } else
                len++;
            break;
        case SP_PREC:
            if (IS_ALPHA(*f)) {
                /* conversion char. */
                int vi;
                char* vs;

                switch (*f) {
                case 'l':
                case 'h':
                case 'L':
                case 'w':
                    continue;
                case 'd':
                case 'i':
                case 'o':
                case 'x':
                case 'X':
                case 'u':
                    vi = va_arg(ap, int);
                    len += (p > 0) ? p : 10;
                    break;
                case 'f':
                case 'g':
                case 'e':
                case 'G':
                case 'E':
                    va_arg(ap, double);
                    len += (p > 0) ? p : 15;
                    break;
                case 'c':
                    len += 1;
                    vi = va_arg(ap, int);
                    break;
                case 's':
                    vs = va_arg(ap, char*);
                    vi = strlen(vs);
                    len += (p > vi) ? p : vi;
                    break;
                case 'p':
                    va_arg(ap, void*);
                    len += 10;
                    break;
                case 'n':
                    va_arg(ap, void*);
                    break;
                }
                status = SP_NORMAL;
            } else if (IS_DIGIT(*f))
                p = p * 10 + *f - '0';
            else if (*f == '.')
                status = SP_PREC2;
            else if (*f == '%') {
                status = SP_NORMAL;
                len++;
            }
            break;
        case SP_PREC2:
            if (IS_ALPHA(*f)) {
                status = SP_PREC;
                goto redo;
            }
            break;
        }
    }

    return len;
}
