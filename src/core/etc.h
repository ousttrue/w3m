#pragma once
#include <Str.h>
#include <time.h>
#include "line.h"

struct _Buffer;
struct _ParsedURL;

Str base64_encode(const char* src, size_t len);
char* mybasename(char* s);
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
int is_localhost(const char* host);
char* file_to_url(char* file);
char* url_unquote_conv(char* url, wc_ces charset);
char* expandName(char* name);

enum TmpFileType {
    TMPF_DFL = 0,
    TMPF_SRC = 1,
    TMPF_CACHE = 2,
    TMPF_COOKIE = 3,
    TMPF_HIST = 4,
    MAX_TMPF_TYPE = 5,
};
Str tmpfname(enum TmpFileType type, char* ext);

time_t mymktime(char* timestr);
char* FQDN(char* host);
pid_t open_pipe_rw(FILE** fr, FILE** fw);
FILE* openSecretFile(char* fname);
void loadPasswd(void);
