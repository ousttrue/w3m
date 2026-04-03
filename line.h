#pragma once
#include "Str.h"

typedef unsigned short Lineprop;
typedef unsigned char Linecolor;

typedef struct _Line {
    char* lineBuf;
    Lineprop* propBuf;
    Linecolor* colorBuf;
    struct _Line* next;
    struct _Line* prev;
    int len;
    int width;
    long linenumber; /* on buffer */
    long real_linenumber; /* on file */
    unsigned short usrflags;
    int size;
    int bpos;
    int bwidth;
} Line;

int columnPos(Line* line, int column);
int columnLen(Line* line, int column);
struct _Buffer;
Line* lineSkip(struct _Buffer* buf, Line* line, int offset, int last);
Line* currentLineSkip(struct _Buffer* buf, Line* line, int offset, int last);
int gethtmlcmd(char** s);
Str checkType(Str s, Lineprop** oprop, Linecolor** ocolor);
int calcPosition(char* l, Lineprop* pr, int len, int pos, int bpos, int mode);
