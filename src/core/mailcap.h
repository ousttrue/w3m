#pragma once
#include <Str.h>

#define MAILCAP_NEEDSTERMINAL 0x01
#define MAILCAP_COPIOUSOUTPUT 0x02
#define MAILCAP_HTMLOUTPUT 0x04

#define MCSTAT_REPNAME 0x01
#define MCSTAT_REPTYPE 0x02
#define MCSTAT_REPPARAM 0x04

struct mailcap {
    char* type;
    char* viewer;
    int flags;
    char* test;
    char* nametemplate;
    char* edit;
};

int mailcapMatch(struct mailcap* mcap, char* type);
struct mailcap* searchMailcap(struct mailcap* table, char* type);
void initMailcap(void);
char* acceptableMimeTypes(void);
struct mailcap* searchExtViewer(const char* type);
Str unquote_mailcap(const char* qstr, const char* type, const char* name, const char* attr,
    int* mc_stat);
