#pragma once
#include <memory>
#include <string>

extern char *personal_document_root;

int open_pipe_rw(FILE **fr, FILE **fw);
const char *file_to_url(const char *file);
const char *lastFileName(const char *path);
const char *mydirname(const char *s);
const char *expandName(const char *name);
const char *expandPath(const char *name);
void mySystem(const char *command, int background);
struct Str;
Str *myExtCommand(const char *cmd, const char *arg, int redirect);
struct Buffer;
const char *last_modified(const std::shared_ptr<Buffer> &buf);
int do_recursive_mkdir(const char *dir);
int exec_cmd(const std::string &cmd);
const char *remove_space(const char *str);
Str *unescape_spaces(Str *s);
void setup_child(int child, int i, int f);
void myExec(const char *command);
