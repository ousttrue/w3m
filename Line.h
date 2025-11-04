#pragma once

typedef unsigned short Lineprop;
typedef unsigned char Linecolor;

struct Line {
    char* lineBuf;
    Lineprop* propBuf;
    Linecolor* colorBuf;
    struct Line* next;
    struct Line* prev;
    int len;
    int width;
    long linenumber; /* on buffer */
    long real_linenumber; /* on file */
    unsigned short usrflags;
    int size;
    int bpos;
    int bwidth;
};
