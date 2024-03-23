#pragma once
#include <memory>
#include <string>

extern bool IsForkChild;
extern std::string ExtBrowser;
extern std::string ExtBrowser2;
extern std::string ExtBrowser3;
extern std::string ExtBrowser4;
extern std::string ExtBrowser5;
extern std::string ExtBrowser6;
extern std::string ExtBrowser7;
extern std::string ExtBrowser8;
extern std::string ExtBrowser9;

int open_pipe_rw(FILE **fr, FILE **fw);
std::string file_to_url(std::string file);
const char *lastFileName(const char *path);
const char *mydirname(const char *s);
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
