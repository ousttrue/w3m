#pragma once
#include <gcstr.h>

extern const char* w3m_version;

struct w3m {
    const char* CurrentDir;
    int CurrentPid;
    const char* MyProgramName;
};
extern struct w3m w3m;

extern wc_ces DisplayCharset;
extern wc_ces DocumentCharset;
extern wc_ces SystemCharset;
extern wc_ces BookmarkCharset;

inline static Str Str_conv_from_system(Str x) { return wc_Str_conv((x), SystemCharset, InnerCharset); }
inline static Str Str_conv_to_system(Str x) { return wc_Str_conv_strict((x), InnerCharset, SystemCharset); }
inline static char* conv_from_system(const char* x) { return wc_conv((x), SystemCharset, InnerCharset)->ptr; }
inline static char* conv_to_system(const char* x) { return wc_conv_strict((x), InnerCharset, SystemCharset)->ptr; }

char* w3m_auxbin_dir(void);
char* w3m_lib_dir(void);
char* w3m_etc_dir(void);
char* w3m_conf_dir(void);
char* w3m_help_dir(void);

/// expand '~'
Str expandPath(const char* name);
