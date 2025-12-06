#pragma once
#include <gcstr.h>

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

struct sel_c {
    int value;
    const char* cvalue;
    const char* text;
};

struct param_ptr {
    const char* name;
    enum ParamTypes type;
    enum ParamInputTypes inputtype;
    /// value
    void* varptr;
    /// comment
    const char* comment;
    /// enum values
    void* select;
};

struct param_section {
    const char* name;
    /// param list
    struct param_ptr* params;
};

struct W3mConfig {
    char* param_tmp_dir;
    /// ~/.w3m/config
    const char* config_file;

    struct param_section* sections;
};
extern struct W3mConfig w3m_config;

void config_initialize();
void config_make_rc_table();
void config_load(int r);
bool config_set_param(const char* name, const char* value);
bool str_to_bool(const char* value, bool old);
const char* config_get_param_option(const char* name);
bool config_set_param_option(const char* option);
extern Str config_panel_html();
void show_params(int w);
