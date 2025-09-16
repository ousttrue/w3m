#pragma once
#include <Str.h>
#include <wc.h>
#include <stdbool.h>

extern const char* HostName;
extern char* w3m_version;
extern const char* CurrentDir;
extern int CurrentPid;
extern char* tmp_dir;
extern char* rc_dir;
// local-cgi
extern const char* cgi_bin;
extern const char* document_root;

#define CGI_EXTENSION ".cgi"
// #define CGI_EXTENSION ".cmd"

#define SYSTEM_CHARSET WC_CES_UTF_8
extern wc_ces InnerCharset;
extern wc_ces SystemCharset;

inline static Str Str_conv_to_system(Str x)
{
    return wc_Str_conv_strict((x), InnerCharset, SystemCharset);
}
// inline static Str_conv_to_halfdump(Strx){
//     (ExtHalfdump ? wc_Str_conv((x), InnerCharset, DisplayCharset) : (x))
// }
inline static const char* conv_from_system(const char* x)
{
    return wc_conv((x), SystemCharset, InnerCharset)->ptr;
}
inline static const char* conv_to_system(const char* x)
{
    return wc_conv_strict((x), InnerCharset, SystemCharset)->ptr;
}

extern bool DecodeURL;

const char* expandPath(const char* name);
const char* expandName(const char* name);
const char* file_to_url(const char* file, const char* currentDir);
const char* w3m_auxbin_dir();
const char* w3m_lib_dir();
const char* w3m_etc_dir();
const char* w3m_conf_dir();
const char* w3m_help_dir();
const char* rcFile(const char* base);
const char* etcFile(const char* base);
const char* confFile(const char* base);
const char* auxbinFile(const char* base);

enum TmpFileType {
    TMPF_DFL = 0,
    TMPF_SRC = 1,
    TMPF_CACHE = 2,
    TMPF_COOKIE = 3,
    TMPF_HIST = 4,
    MAX_TMPF_TYPE = 5,
};
void initDeleteFile();
void deinitDeleteFile();
void pushDeleteFile(const char* path);
Str tmpfname(enum TmpFileType type, const char* ext);

const char* url_decode2(const char* url, wc_ces url_charset);

int doFileCopy(const char* tmpf, const char* defstr);
