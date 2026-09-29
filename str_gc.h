#pragma once
#include "Str.h"

extern const char* Editor;
extern const char* personal_document_root;

Str mydirname(const char* s);
Str romanNumeral(int n);
Str romanAlphabet(int n);
Str myExtCommand(const char* cmd, const char* arg, bool redirect);
Str editor_cmd(const char* file, int line);

Str expandPath(const char* name);
/// expand `~` to username in string
Str expandName(const char* name);
Str file_to_url(const char* file, const char* CurrentDir);
/// resolve relative path etc. for example /path/../to/../some
Str cleanupName(const char *name);
