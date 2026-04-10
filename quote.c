#include "quote.h"

// clang-format off
static uint8_t QUOTE_MAP[0x100] = {
    /* NUL SOH STX ETX EOT ENQ ACK BEL  BS  HT  LF  VT  FF  CR  SO  SI */
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    /* DLE DC1 DC2 DC3 DC4 NAK SYN ETB CAN  EM SUB ESC  FS  GS  RS  US */
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    /* SPC   !   "   #   $   %   &   '   (   )   *   +   ,   -   .   / */
    24, 72, 76, 40, 8, 40, 41, 77, 72, 72, 72, 40, 72, 8, 0, 64,
    /*   0   1   2   3   4   5   6   7   8   9   :   ;   <   =   >   ? */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 72, 74, 72, 75, 40,
    /*   @   A   B   C   D   E   F   G   H   I   J   K   L   M   N   O */
    72, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /*   P   Q   R   S   T   U   V   W   X   Y   Z   [   \   ]   ^   _ */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 72, 72, 72, 72, 0,
    /*   `   a   b   c   d   e   f   g   h   i   j   k   l   m   n   o */
    72, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /*   p   q   r   s   t   u   v   w   x   y   z   {   |   }   ~ DEL */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 72, 72, 72, 72, 24,

    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
};
// clang-format on

#define GET_QUOTE_TYPE(c) QUOTE_MAP[(int)(unsigned char)(c)]
bool is_html_quote(int c) { return (GET_QUOTE_TYPE(c) & HTML_QUOTE_MASK); }
bool is_shell_unsafe(int c) { return (GET_QUOTE_TYPE(c) & SHELL_UNSAFE_MASK); }
bool is_url_quote(int c) { return (GET_QUOTE_TYPE(c) & URL_QUOTE_MASK); }
bool is_file_quote(int c) { return (GET_QUOTE_TYPE(c) & FILE_QUOTE_MASK); }
bool is_url_unsafe(int c) { return (GET_QUOTE_TYPE(c) & URL_UNSAFE_MASK); }

static const char* HTML_QUOTE_MAP[] = {
    0,
    "&amp;",
    "&lt;",
    "&gt;",
    "&quot;",
    "&apos;",
    0,
    0,
};

const char* html_quote_char(int c)
{
    return HTML_QUOTE_MAP[is_html_quote(c)];
}
