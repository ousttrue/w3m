#pragma once
#include "Str.h"
#include <libwc/wtf.h>
#include "constants.h"

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

int columnPos(struct Line* line, int column);
int columnLen(struct Line* line, int column);
struct Buffer;
struct Line* lineSkip(struct Buffer* buf, struct Line* line, int offset, int last);
struct Line* currentLineSkip(struct Buffer* buf, struct Line* line, int offset, int last);
int gethtmlcmd(char** s);
Str checkType(Str s, Lineprop** oprop, Linecolor** ocolor);

/* Flags for calcPosition() */
#define CP_AUTO 0
#define CP_FORCE 1

int calcPosition(char* l, Lineprop* pr, int len, int pos, int bpos, int mode);

enum LineMode {
    RAW_MODE = 0,
    PAGER_MODE = 1,
    HTML_MODE = 2,
    HEADER_MODE = 3,
};

void cleanup_line(Str s, enum LineMode mode);
Str convertLine(const char* line, int len, enum LineMode mode, wc_ces* charset, wc_ces doc_charset, bool do_chop);

void addStr(char* p, Lineprop* pr, int len, int offset, int limit);
void addPasswd(char* p, Lineprop* pr, int len, int offset, int limit);
