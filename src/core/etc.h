#pragma once
#include <Str.h>
#include <fcntl.h>
#include <time.h>
#include "line.h"

struct _Buffer;
struct _ParsedURL;

Str base64_encode(const char* src, size_t len);
int columnSkip(struct _Buffer* buf, int offset);
struct _Line* lineSkip(struct _Buffer* buf, struct _Line* line, int offset, int last);
struct _Line* currentLineSkip(struct _Buffer* buf, struct _Line* line, int offset, int last);
int gethtmlcmd(char** s);
Str checkType(Str s, Lineprop** oprop, Linecolor** ocolor);
char* lastFileName(char* path);
char* mydirname(char* s);
int next_status(char c, int* status);
int read_token(Str buf, char** instr, int* status, int pre, int append);
Str correct_irrtag(int status);
int find_auth_user_passwd(struct _ParsedURL* pu, char* realm, Str* uname, Str* pwd, int is_proxy);
void add_auth_user_passwd(struct _ParsedURL* pu, char* realm, Str uname, Str pwd, int is_proxy);
void invalidate_auth_user_passwd(struct _ParsedURL* pu, char* realm, Str uname, Str pwd, int is_proxy);
char* last_modified(struct _Buffer* buf);
Str romanNumeral(int n);
Str romanAlphabet(int n);
void setup_child(int child, int i, int f);
void myExec(char* command);
void mySystem(char* command, int background);
Str myExtCommand(char* cmd, char* arg, int redirect);
Str myEditor(char* cmd, char* file, int line);
char* url_unquote_conv(char* url, wc_ces charset);
char* expandName(char* name);

time_t mymktime(const char* timestr);
char* FQDN(char* host);
pid_t open_pipe_rw(FILE** fr, FILE** fw);
FILE* openSecretFile(char* fname);
void loadPasswd(void);
