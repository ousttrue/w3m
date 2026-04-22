#pragma once
#include "Str.h"
#include "line.h"
#include "constants.h"
#include <stdbool.h>

#define STR_LEN 1024

enum CompletionFlags {
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

struct LineInput {
    bool use_hist;
    enum HistoryType CurrentHist; // = HistoryNone;

    bool is_passwd;
    bool move_word;

    Str strBuf;
    Lineprop strProp[STR_LEN];
    int CLen;
    int CPos;
    int offset;

    Str strCurrentBuf;
    Str CBeforeBuf;
    Str CAfterBuf;
    int NCFileBuf;
    Str CompleteBuf;
    Str CDirBuf;
    Str CFileName;
    char** CFileBuf; // = NULL;
    int NCFileOffset;

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
struct CmdArgs;

struct LineInput LineInputInit(const char* def_str,
    enum InputLineFlags flag,
    enum HistoryType hist);
void setStrType(struct LineInput* li);
void next_dcompl(struct LineInput* li, struct CmdArgs* args, int next);
int terminated(unsigned char c);
void next_compl(struct LineInput* li, int next);
void ins_char(struct LineInput* li, struct CmdArgs* args, Str str);

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

