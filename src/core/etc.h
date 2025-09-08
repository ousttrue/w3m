#pragma once
#include <Str.h>
#include <unistd.h>

extern int nextpage_topline;
extern int disable_secret_security_check;

int gethtmlcmd(char** s);
char* lastFileName(char* path);
char* mydirname(char* s);
int next_status(char c, int* status);
int read_token(Str buf, char** instr, int* status, int pre, int append);
Str correct_irrtag(int status);

Str romanNumeral(int n);
Str romanAlphabet(int n);
void setup_child(int child, int i, int f);
void myExec(char* command);
void mySystem(char* command, int background);
Str myExtCommand(char* cmd, char* arg, int redirect);
Str myEditor(char* cmd, char* file, int line);

#include <wc.h>
char* url_unquote_conv(char* url, wc_ces charset);
const char* expandName(const char* name);

pid_t open_pipe_rw(FILE** fr, FILE** fw);
FILE* openSecretFile(char* fname);
void loadPasswd(void);
