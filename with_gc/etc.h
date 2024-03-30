#pragma once
#include <string>

extern bool IsForkChild;

int open_pipe_rw(FILE **fr, FILE **fw);
std::string file_to_url(std::string file);
const char *lastFileName(const char *path);
const char *mydirname(const char *s);
struct Str;
struct Buffer;
int do_recursive_mkdir(const char *dir);
int exec_cmd(const std::string &cmd);
Str *unescape_spaces(Str *s);
void setup_child(int child, int i, int f);
extern const char *getWord(const char **str);
extern const char *getQWord(const char **str);
