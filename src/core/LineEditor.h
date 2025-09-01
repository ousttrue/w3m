#pragma once
#include "line.h"
#include "ui.h"
#include <Str.h>
#include <stdbool.h>

enum CompletionMode {
    CPL_NEVER = 0x0,
    CPL_OFF = 0x1,
    CPL_ON = 0x2,
    CPL_ALWAYS = 0x4,
    CPL_URL = 0x8,
};

enum CompletionStatus {
    CPL_OK = 0,
    CPL_AMBIG = 1,
    CPL_FAIL = 2,
    CPL_MENU = 3,
};

enum InputLineFlags {
    IN_STRING = 0x10,
    IN_FILENAME = 0x20,
    IN_PASSWORD = 0x40,
    IN_COMMAND = 0x80,
    IN_URL = 0x100,
    IN_CHAR = 0x200,
};

#define STR_LEN 1024

struct LineEditor {
    Lineprop strProp[STR_LEN];
    struct UI ui;

    bool is_passwd;

    int CPos;
    int CLen;
    int offset;

    bool i_cont;
    bool i_broken;
    bool i_quote;

    // completion
    enum CompletionMode cm_mode;
    bool cm_next;
    bool cm_clear;
    int cm_disp_next;
    int cm_disp_clear;

    bool need_redraw;
    bool move_word;

    Str strBuf;
    struct Hist* CurrentHist;
    Str strCurrentBuf;
    bool use_hist;
    Str CompleteBuf;
    Str CFileName;
    Str CBeforeBuf;
    Str CAfterBuf;
    Str CDirBuf;
    char** CFileBuf;
    int NCFileBuf;
    int NCFileOffset;
};

void le_initialize(struct LineEditor* e, struct UI ui, struct Hist*, enum InputLineFlags flag,
    const char* def_str);
void le_insertself(struct LineEditor* e, char c);
#define iself ((void (*)())insertself)
void le_next_compl(struct LineEditor* e, int next);
void le_next_dcompl(struct LineEditor* e, int next);
Str le_doComplete(struct LineEditor* e, Str ifn, enum CompletionStatus* status, int next);
int le_setStrType(struct LineEditor* e, Str str, Lineprop* prop);
void le_ins_char(struct LineEditor* e, Str str);
void le_addPasswd(struct LineEditor* e, char* p, Lineprop* pr, int len, int offset, int limit);
void le_addStr(struct LineEditor* e, char* p, Lineprop* pr, int len, int offset, int limit);

void _nop(struct LineEditor* e);
void _compl(struct LineEditor* e);
void _mvB(struct LineEditor* e);
void _mvL(struct LineEditor* e);
void _inbrk(struct LineEditor* e);
void delC(struct LineEditor* e);
void _mvE(struct LineEditor* e);
void _mvR(struct LineEditor* e);
void _bs(struct LineEditor* e);
void _enter(struct LineEditor* e);
void killn(struct LineEditor* e);
void _next(struct LineEditor* e);
void _editor(struct LineEditor* e);
void _prev(struct LineEditor* e);
void _quo(struct LineEditor* e);
void _bsw(struct LineEditor* e);
void _mvLw(struct LineEditor*);
void killb(struct LineEditor* e);
void _tcompl(struct LineEditor* e);
void _mvRw(struct LineEditor* e);
void _dcompl(struct LineEditor* e);
void insC(struct LineEditor* e);
void _rdcompl(struct LineEditor* e);
void _rcompl(struct LineEditor* e);
