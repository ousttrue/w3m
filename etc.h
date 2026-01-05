#pragma once
#include "Str.h"
#include <sys/types.h>
#include <libwc/ces.h>

Str base64_encode(const char* src, size_t len);
char* lastFileName(const char* path);
char* mydirname(const char* s);
char* mybasename(const char* s);
pid_t open_pipe_rw(FILE** fr, FILE** fw);
time_t mymktime(const char* timestr);

void setup_child(int child, int i, int f);
struct Url;
int find_auth_user_passwd(struct Url* pu, char* realm,
    Str* uname, Str* pwd, int is_proxy);
void add_auth_user_passwd(struct Url* pu, char* realm,
    Str uname, Str pwd, int is_proxy);
void invalidate_auth_user_passwd(struct Url* pu, char* realm,
    Str uname, Str pwd, int is_proxy);
void myExec(const char* command);
char* url_unquote_conv(const char* url, enum wc_ces charset);
struct Buffer;
char* last_modified(struct Buffer* buf);
Str myEditor(const char* cmd, const char* file, int line);
int is_localhost(const char* host);
char* expandName(const char* name);
FILE* openSecretFile(const char* fname);
void mySystem(const char* command, int background);
Str myExtCommand(const char* cmd, const char* arg, int redirect);
Str unescape_spaces(Str s);

char* getQWord(const char** str);
char* getWord(const char** str);
struct Regex;
const char* getRegexWord(const char** str, struct Regex** regex_ret);
extern void loadPasswd(void);
Str qstr_unquote(Str s);
