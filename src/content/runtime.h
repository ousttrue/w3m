#pragma once
#include <Str.h>

extern const char* CurrentDir;
extern int CurrentPid;
extern char* tmp_dir;
extern char* rc_dir;

const char* expandPath(const char* name);
const char* expandName(const char* name);
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
