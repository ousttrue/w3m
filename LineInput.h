#pragma once
#include "constants.h"

enum InputLineFlags {
    IN_STRING = 0x10,
    IN_FILENAME = 0x20,
    IN_PASSWORD = 0x40,
    IN_COMMAND = 0x80,
    IN_URL = 0x100,
    IN_CHAR = 0x200,
};

struct LineInputResult {
    const char* str;
    bool need_redraw;
    bool i_broken;
};

struct CmdArgs;

struct LineInputResult do_lineinput(struct CmdArgs* args, const char* prompt, const char* def_str,
    enum InputLineFlags flag,
    enum HistoryType hist, IncrFunc incfunc);
