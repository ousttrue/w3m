/* $Id: myctype.c,v 1.7 2003/09/22 21:02:20 ukai Exp $ */
unsigned char MYCTYPE_MAP[0x100] = {
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


