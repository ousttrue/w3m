#pragma once
#include "Str.h"

struct mailcap {
    char* type;
    char* viewer;
    int flags;
    char* test;
    char* nametemplate;
    char* edit;
};

#define MAILCAP_NEEDSTERMINAL 0x01
#define MAILCAP_COPIOUSOUTPUT 0x02
#define MAILCAP_HTMLOUTPUT 0x04

#define MCSTAT_REPNAME 0x01
#define MCSTAT_REPTYPE 0x02
#define MCSTAT_REPPARAM 0x04

void initMailcap(void);
int mailcapMatch(struct mailcap* mcap, const char* type);
struct mailcap* searchMailcap(struct mailcap* table, const char* type);
Str unquote_mailcap(const char* qstr, const char* type, const char* name, const char* attr, int* mc_stat);
struct mailcap* searchExtViewer(const char* type);
