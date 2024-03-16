#pragma once
#include <memory>
#include <string>

extern bool IsForkChild;
extern const char *ExtBrowser;
extern char *ExtBrowser2;
extern char *ExtBrowser3;
extern char *ExtBrowser4;
extern char *ExtBrowser5;
extern char *ExtBrowser6;
extern char *ExtBrowser7;
extern char *ExtBrowser8;
extern char *ExtBrowser9;

int open_pipe_rw(FILE **fr, FILE **fw);
std::string file_to_url(std::string file);
const char *lastFileName(const char *path);
const char *mydirname(const char *s);
std::string expandPath(std::string_view name);
void mySystem(const char *command, int background);
struct Str;
Str *myExtCommand(const char *cmd, const char *arg, int redirect);
struct Buffer;
int do_recursive_mkdir(const char *dir);
int exec_cmd(const std::string &cmd);
Str *unescape_spaces(Str *s);
void setup_child(int child, int i, int f);
void myExec(const char *command);
extern const char *getWord(const char **str);
extern const char *getQWord(const char **str);
void invoke_browser(const char *url);
