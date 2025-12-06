#pragma once
#include <gcstr.h>

struct W3mConfig {
    char* param_tmp_dir;
    /// ~/.w3m/config
    const char* config_file;
};
extern struct W3mConfig w3m_config;

void config_initialize();
void config_load(int r);
bool config_set_param(const char* name, const char* value);
bool str_to_bool(const char* value, bool old);
const char* config_get_param_option(const char* name);
bool config_set_param_option(const char* option);
extern Str config_panel_html();
void show_params(int w);
