#pragma once
#include "Str.h"
#include <stdbool.h>
#include <stdio.h>

#define DEFAULT_URL_EMPTY 0
#define DEFAULT_URL_CURRENT 1
#define DEFAULT_URL_LINK 2

struct sel_c {
    int value;
    const char* cvalue;
    const char* text;
};

enum ParamTypes {
    P_INT = 0,
    P_SHORT = 1,
    P_CHARINT = 2,
    P_CHAR = 3,
    P_STRING = 4,
    P_SSLPATH = 5,
    P_COLOR = 6,
    P_CODE = 7,
    P_PIXELS = 8,
    P_NZINT = 9,
    P_SCALE = 10,
};

enum ParamInputTypes {
    PI_TEXT = 0,
    PI_ONOFF = 1,
    PI_SEL_C = 2,
    PI_CODE = 3,
};

struct param_ptr {
    const char* name;
    enum ParamTypes type;
    enum ParamInputTypes inputtype;
    /// pointer to global variable
    void* varptr;
    const char* comment;
    void* select;
};

struct param_section {
    const char* name;
    struct param_ptr* params;
};

void opt_create_search_table();
// show parameter with bad options invokation
void opt_show_params(FILE* fp);
bool opt_set_param(const char* name, const char* value);
bool opt_set_param_option(const char* option);
char* opt_get_param_option(const char* name);
Str opt_load_panel(void);
