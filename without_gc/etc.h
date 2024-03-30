#pragma once
#include <string>

extern bool IsForkChild;

int open_pipe_rw(FILE **fr, FILE **fw);
std::string file_to_url(std::string file);
std::string lastFileName(std::string_view path);
std::string mydirname(std::string_view s);
int do_recursive_mkdir(const char *dir);
void setup_child(int child, int i, int f);
std::string getWord(const char **str);
std::string getQWord(const char **str);
