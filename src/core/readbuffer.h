#pragma once
#include <Str.h>
#include <stdio.h>
#include <string.h>
#include "token.h"
#include "line.h"
#include "anchor.h"
#include "textlist.h"
#include "HtmlTag.h"

#include <wc.h>

extern char DisableCenter;
extern int IndentIncr;
extern char DisplayBorders;
extern int view_unseenobject;


#define DISPLAY_INS_DEL_SIMPLE 0
#define DISPLAY_INS_DEL_NORMAL 1
#define DISPLAY_INS_DEL_FONTIFY 2
extern int displayInsDel;
struct html_feed_environ;
int table_width(struct html_feed_environ* h_env, int table_level);
extern int need_number;
extern wc_ces meta_charset;

// TODO
extern int cur_hseq;
extern int cur_iseq;
Str getLinkNumberStr(int correction);

#define MAX_UL_LEVEL 9
#define UL_SYMBOL(x) (N_GRAPH_SYMBOL + (x))
#define UL_SYMBOL_DISC UL_SYMBOL(9)
#define UL_SYMBOL_CIRCLE UL_SYMBOL(10)
#define UL_SYMBOL_SQUARE UL_SYMBOL(11)
#define HR_SYMBOL 26
#define IMG_SYMBOL UL_SYMBOL(12)

struct cmdtable {
    const char* cmdname;
    enum HtmlTag cmd;
};
#define RB_STACK_SIZE 10
#define FONTSTAT_SIZE 7
#define FONTSTAT_MAX 127
#define FONT_STACK_SIZE 5
#define TAG_STACK_SIZE 10

#define RB_PRE 0x01
#define RB_SCRIPT 0x02
#define RB_STYLE 0x04
#define RB_PLAIN 0x08
#define RB_LEFT 0x10
#define RB_CENTER 0x20
#define RB_RIGHT 0x40
#define RB_ALIGN (RB_LEFT | RB_CENTER | RB_RIGHT)
#define RB_NOBR 0x80
#define RB_P 0x100
#define RB_PRE_INT 0x200
#define RB_IN_DT 0x400
#define RB_INTXTA 0x800
#define RB_INSELECT 0x1000
#define RB_IGNORE_P 0x2000
#define RB_TITLE 0x4000
#define RB_NFLUSHED 0x8000
#define RB_NOFRAMES 0x10000
#define RB_INTABLE 0x20000
#define RB_PREMODE (RB_PRE | RB_PRE_INT | RB_SCRIPT | RB_STYLE | RB_PLAIN | RB_INTXTA)
#define RB_SPECIAL (RB_PRE | RB_PRE_INT | RB_SCRIPT | RB_STYLE | RB_PLAIN | RB_NOBR)
#define RB_PLAIN_PRE 0x40000

#define RB_FILL 0x80000
#define RB_DEL 0x100000
#define RB_S 0x200000
#define RB_HTML5 0x400000

#define RB_GET_ALIGN(obuf) ((obuf)->flag & RB_ALIGN)
#define RB_SET_ALIGN(obuf, align)  \
    do {                           \
        (obuf)->flag &= ~RB_ALIGN; \
        (obuf)->flag |= (align);   \
    } while (0)
#define RB_SAVE_FLAG(obuf)                                              \
    {                                                                   \
        if ((obuf)->flag_sp < RB_STACK_SIZE)                            \
            (obuf)->flag_stack[(obuf)->flag_sp++] = RB_GET_ALIGN(obuf); \
    }
#define RB_RESTORE_FLAG(obuf)                                          \
    {                                                                  \
        if ((obuf)->flag_sp > 0)                                       \
            RB_SET_ALIGN(obuf, (obuf)->flag_stack[--(obuf)->flag_sp]); \
    }

struct input_alt_attr {
    int hseq;
    int fid;
    int in;
    Str type, name, value;
};

typedef struct {
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
} Breakpoint;

#define in_bold fontstat[0]
#define in_under fontstat[1]
#define in_italic fontstat[2]
#define in_strike fontstat[3]
#define in_ins fontstat[4]
#define in_stand fontstat[5]

void push_link(int cmd, int offset, int pos);

struct readbuffer {
    Str line;
    Lineprop cprop;
    short pos;
    Str prevchar;
    long flag;
    long flag_stack[RB_STACK_SIZE];
    int flag_sp;
    enum ReadtokenStatus status;
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
    Breakpoint bp;
    struct cmdtable* tag_stack[TAG_STACK_SIZE];
    int tag_sp;
    short top_margin;
    short bottom_margin;
};

void back_to_breakpoint(struct readbuffer* obuf);
void push_nchars(struct readbuffer* obuf, int width, const char* str, int len, Lineprop mode);
inline static void push_charp(struct readbuffer* obuf, int width, const char* str, Lineprop mode)
{
    push_nchars(obuf, width, str, strlen(str), mode);
}
inline static void push_str(struct readbuffer* obuf, int width, Str str, Lineprop mode)
{
    push_nchars(obuf, width, str->ptr, str->length, mode);
}
void fillline(struct readbuffer* obuf, int indent);
void check_breakpoint(struct readbuffer* obuf, bool pre_mode, const char* ch);
void push_char(struct readbuffer* obuf, int pre_mode, char ch);
inline static void PUSH(struct readbuffer* obuf, char c)
{
    push_char(obuf, obuf->flag & RB_SPECIAL, c);
}
void proc_mchar(struct readbuffer* obuf, bool pre_mode, int width, const char** str, Lineprop mode);
int close_effect0(struct readbuffer* obuf, enum HtmlTag cmd);
void push_spaces(struct readbuffer* obuf, bool pre_mode, int width);
void clear_ignore_p_flag(struct readbuffer* obuf, int cmd);
void set_alignment(struct readbuffer* obuf, struct HtmlTagParsed* tag);
void append_tags(struct readbuffer* obuf);
void push_tag(struct readbuffer* obuf, const char* cmdname, enum HtmlTag cmd);
char* has_hidden_link(struct readbuffer* obuf, enum HtmlTag cmd);
void passthrough(struct readbuffer* obuf, char* str, int back);
void set_breakpoint(struct readbuffer* obuf, int tag_length);

#define MAX_ENV_LEVEL 20
struct environment {
    unsigned char env;
    int type;
    int count;
    char indent;
};

#define MAX_INDENT_LEVEL 10
struct html_feed_environ {
    struct readbuffer* obuf;
    TextLineList* buf;
    FILE* f;
    Str tagbuf;
    int limit;
    int maxlimit;
    struct environment* envs;
    int nenv;
    int envc;
    int envc_real;
    char* title;
    int blank_lines;
};

void flushline(struct html_feed_environ* h_env, struct readbuffer* obuf, int indent, int force, int width);
void purgeline(struct html_feed_environ* h_env);
void push_render_image(Str str, int width, int limit, struct html_feed_environ* h_env);
void do_blankline(struct html_feed_environ* h_env, struct readbuffer* obuf, int indent, int indent_incr, int width);
void save_fonteffect(struct html_feed_environ* h_env, struct readbuffer* obuf);
void restore_fonteffect(struct html_feed_environ* h_env, struct readbuffer* obuf);
int HTMLtagproc1(struct HtmlTagParsed* tag, struct html_feed_environ* h_env);
void init_henv(struct html_feed_environ*, struct readbuffer*, struct environment*, int, TextLineList*, int, int);
void completeHTMLstream(struct html_feed_environ*, struct readbuffer*);
void process_idattr(struct readbuffer* obuf, int cmd, struct HtmlTagParsed* tag);
