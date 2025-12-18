#pragma once
#include "Str.h"
#include "line.h"

struct BufferPoint {
    int line;
    int pos;
    int invalid;
};

struct Anchor {
    char* url;
    char* target;
    char* referer;
    char* title;
    unsigned char accesskey;
    struct BufferPoint start;
    struct BufferPoint end;
    int hseq;
    char slave;
    short y;
    short rows;
    struct Image* image;
};

struct AnchorList {
    struct Anchor* anchors;
    int nanchor;
    int anchormax;
    int acache;
};

struct HmarkerList {
    struct BufferPoint* marks;
    int nmark;
    int markmax;
    int prevhseq;
};

struct input_alt_attr {
    int hseq;
    int fid;
    int in;
    Str type, name, value;
};

#define FONTSTAT_SIZE 7

struct Breakpoint {
    int pos;
    int len;
    int tlen;
    long flag;
    struct Anchor anchor;
    Str img_alt;
    struct input_alt_attr input_alt;
    char fontstat[FONTSTAT_SIZE];
    short nobr_level;
    Lineprop prev_ctype;
    char init_flag;
    short top_margin;
    short bottom_margin;
};

#define RB_STACK_SIZE 10
#define FONT_STACK_SIZE 5
#define TAG_STACK_SIZE 10
struct readbuffer {
    Str line;
    Lineprop cprop;
    short pos;
    Str prevchar;
    long flag;
    long flag_stack[RB_STACK_SIZE];
    int flag_sp;
    int status;
    unsigned char end_tag;
    unsigned char q_level;
    short table_level;
    short nobr_level;
    struct Anchor anchor;
    Str img_alt;
    struct input_alt_attr input_alt;
    char fontstat[FONTSTAT_SIZE];
    char fontstat_stack[FONT_STACK_SIZE][FONTSTAT_SIZE];
    int fontstat_sp;
    Lineprop prev_ctype;
    struct Breakpoint bp;
    struct cmdtable* tag_stack[TAG_STACK_SIZE];
    int tag_sp;
    short top_margin;
    short bottom_margin;
};


