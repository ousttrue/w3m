#pragma once
#include "constants.h"

struct LineInputResult {
    const char* str;
    bool need_redraw;
    bool i_broken;
};

struct CmdArgs;

struct LineInputResult do_lineinput(struct CmdArgs* args, const char* prompt, const char* def_str,
    enum InputLineFlags flag,
    enum HistoryType hist, IncrFunc incfunc);
