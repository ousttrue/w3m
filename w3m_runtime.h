#pragma once
#include <gcstr.h>

extern const char* w3m_version;

struct w3m {
    const char* HostName;
    const char* CurrentDir;
    int CurrentPid;
    const char* MyProgramName;
    /// ~/.w3m
    char* rc_dir;
    /// ~/.w3m
    char* tmp_dir;

    bool UseContentCharset;
};
extern struct w3m w3m;

struct W3mConfig {
    char* param_tmp_dir;
    /// ~/.w3m/config
    const char* config_file;

    const char* cgi_bin;
    const char* document_root;
    const char* personal_document_root;
    const char* index_file;
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

extern wc_ces DisplayCharset;
extern wc_ces DocumentCharset;
extern wc_ces SystemCharset;
extern wc_ces BookmarkCharset;

inline static Str Str_conv_from_system(Str x) { return wc_Str_conv((x), SystemCharset, InnerCharset); }
inline static Str Str_conv_to_system(Str x) { return wc_Str_conv_strict((x), InnerCharset, SystemCharset); }
inline static char* conv_from_system(const char* x) { return wc_conv((x), SystemCharset, InnerCharset)->ptr; }
inline static char* conv_to_system(const char* x) { return wc_conv_strict((x), InnerCharset, SystemCharset)->ptr; }

void w3m_initialize();
void init_tmp();
Str rcFile(const char* base);
Str confFile(const char* base);

const char* w3m_auxbin_dir();
const char* w3m_lib_dir();
const char* w3m_etc_dir();
const char* w3m_conf_dir();
const char* w3m_help_dir();

/// expand '~'
Str expandPath(const char* name);
