#pragma once
#include <stdio.h>
#include <unistd.h>

extern char *personal_document_root;

pid_t open_pipe_rw(FILE **fr, FILE **fw);
const char *file_to_url(const char *file);
int is_localhost(const char *host);
const char *lastFileName(const char *path);
const char *mybasename(const char *s);
const char *mydirname(const char *s);
const char *expandName(const char *name);
const char *expandPath(const char *name);
FILE *openSecretFile(const char *fname);
void mySystem(const char *command, int background);
struct Str;
Str *myExtCommand(const char *cmd, const char *arg, int redirect);
Str *myEditor(const char *cmd, const char *file, int line);
const char *url_unquote_conv0(const char *url);
#define url_unquote_conv(url, charset) url_unquote_conv0(url)
struct Buffer;
const char *last_modified(Buffer *buf);
