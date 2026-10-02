#pragma once
#include <signal.h>
#include <stdio.h>
#include "Str.h"

extern struct wc_option WcOption;

extern const char* CurrentDir;
extern int CurrentPid;
extern bool fmInitialized;
extern bool QuietMessage;
extern bool TrapSignal;
extern const char* cgi_bin;
extern const char* document_root;

void w3m_init();
void set_environ(const char* var, const char* value);
const char* currentdir(void);
typedef void (*SigActionFunc)(int);
extern SigActionFunc mySignal(int signal_number, SigActionFunc action);
void setup_child(bool is_child, int i, int f);
pid_t open_pipe_rw(FILE** fr, FILE** fw);
void myExec(const char* command);
int mySystem(const char* command, bool background);

pStr localCookie(void);
struct form_list;
FILE* localcgi_post(const char* uri,
    const char* query, struct form_list* post, const char* referer);
static inline FILE* localcgi_get(const char* uri, const char* query, const char* referer)
{
    return localcgi_post(uri, query, 0, referer);
}
