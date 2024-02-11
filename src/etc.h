#pragma once
#include <stdio.h>
#include <unistd.h>
#include <memory>

extern char *personal_document_root;

pid_t open_pipe_rw(FILE **fr, FILE **fw);
const char *file_to_url(const char *file);
int is_localhost(const char *host);
const char *lastFileName(const char *path);
const char *mydirname(const char *s);
const char *expandName(const char *name);
const char *expandPath(const char *name);
void mySystem(const char *command, int background);
struct Str;
Str *myExtCommand(const char *cmd, const char *arg, int redirect);
Str *myEditor(const char *cmd, const char *file, int line);
struct Buffer;
const char *last_modified(const std::shared_ptr<Buffer> &buf);
