#pragma once

extern char PermitSaveToPipe;

/// helper for qsort
int strCmp(const void* s1, const void* s2);
const char* mybasename(const char* s);
const char* w3m_auxbin_dir(void);
const char* w3m_lib_dir(void);
const char* w3m_etc_dir(void);
const char* w3m_conf_dir(void);
const char* w3m_help_dir(void);
const char* guessContentType(const char* filename);
bool canCopyFile(const char* path1, const char* path2);
