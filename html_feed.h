#pragma once
#include "Line.h"
#include "anchor.h"
#include "textlist.h"
#include <gcstr/gcstr.h>

#define RB_STACK_SIZE 10
#define TAG_STACK_SIZE 10
#define FONT_STACK_SIZE 5
#define FONTSTAT_SIZE 7
#define FONTSTAT_MAX 127

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
    Anchor anchor;
    Str img_alt;
    struct input_alt_attr input_alt;
    char fontstat[FONTSTAT_SIZE];
    short nobr_level;
    Lineprop prev_ctype;
    char init_flag;
    short top_margin;
    short bottom_margin;
} Breakpoint;

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
    Anchor anchor;
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

struct html_feed_environ {
    struct readbuffer* obuf;
    TextLineList* buf;
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

void push_render_image(Str str, int width, int limit,
    struct html_feed_environ* h_env);
void flushline(struct html_feed_environ* h_env, struct readbuffer* obuf,
    int indent, int force, int width);
void do_blankline(struct html_feed_environ* h_env,
    struct readbuffer* obuf, int indent, int indent_incr,
    int width);
void purgeline(struct html_feed_environ* h_env);
void save_fonteffect(struct html_feed_environ* h_env,
    struct readbuffer* obuf);
void restore_fonteffect(struct html_feed_environ* h_env,
    struct readbuffer* obuf);
struct HtmlTag;
int HTMLtagproc1(struct HtmlTag* tag, struct html_feed_environ* h_env);
void HTMLlineproc0(char* istr, struct html_feed_environ* h_env, int internal);
#define HTMLlineproc1(x, y) HTMLlineproc0(x, y, TRUE)
void init_henv(struct html_feed_environ*, struct readbuffer*,
    struct environment*, int, TextLineList*, int, int);
void completeHTMLstream(struct html_feed_environ*,
    struct readbuffer*);
