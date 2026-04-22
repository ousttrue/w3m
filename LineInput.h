#pragma once
#include "Str.h"
#include "line.h"
#include <stdbool.h>

#define STR_LEN 1024

enum CompletionFlags {
    CPL_NEVER = 0x0,
    CPL_OFF = 0x1,
    CPL_ON = 0x2,
    CPL_ALWAYS = 0x4,
    CPL_URL = 0x8,
};

struct LineInput {
    bool is_passwd;
    bool move_word;

    Str strBuf;
    Lineprop strProp[STR_LEN];

    int CLen;
    int CPos;
    int offset;

    bool need_redraw;

    bool i_cont;
    bool i_broken;
    bool i_quote;

    bool cm_next;
    bool cm_clear;
    int cm_disp_next;
    bool cm_disp_clear;
    enum CompletionFlags cm_mode; // = 0;
};

struct LineInput LineInputInit(const char* def_str);
void setStrType(struct LineInput* li);

struct CmdArgs;
typedef int (*InputFunc)(struct CmdArgs* args, struct LineInput* li);
int iself(struct CmdArgs* args, struct LineInput* li);
int _mvR(struct CmdArgs* args, struct LineInput* li);
int _mvL(struct CmdArgs* args, struct LineInput* li);
int _mvRw(struct CmdArgs* args, struct LineInput* li);
int _mvLw(struct CmdArgs* args, struct LineInput* li);
int delC(struct CmdArgs* args, struct LineInput* li);
int insC(struct CmdArgs* args, struct LineInput* li);
int _mvB(struct CmdArgs* args, struct LineInput* li);
int _mvE(struct CmdArgs* args, struct LineInput* li);
int _enter(struct CmdArgs* args, struct LineInput* li);
int _quo(struct CmdArgs* args, struct LineInput* li);
int _bs(struct CmdArgs* args, struct LineInput* li);
int _bsw(struct CmdArgs* args, struct LineInput* li);
int killn(struct CmdArgs* args, struct LineInput* li);
int killb(struct CmdArgs* args, struct LineInput* li);
int _inbrk(struct CmdArgs* args, struct LineInput* li);
int _esc(struct CmdArgs* args, struct LineInput* li);
int _editor(struct CmdArgs* args, struct LineInput* li);
int _prev(struct CmdArgs* args, struct LineInput* li);
int _next(struct CmdArgs* args, struct LineInput* li);
int _compl(struct CmdArgs* args, struct LineInput* li);
int _tcompl(struct CmdArgs* args, struct LineInput* li);
int _dcompl(struct CmdArgs* args, struct LineInput* li);
int _rdcompl(struct CmdArgs* args, struct LineInput* li);
int _rcompl(struct CmdArgs* args, struct LineInput* li);
