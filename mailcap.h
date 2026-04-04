#pragma once
#include "Str.h"

struct mailcap {
    const char* type;
    const char* viewer;
    int flags;
    const char* test;
    const char* nametemplate;
    const char* edit;
};

int mailcapMatch(struct mailcap* mcap, const char* type);
struct mailcap* searchMailcap(struct mailcap* table, const char* type);
void initMailcap(void);
const char* acceptableMimeTypes(void);
struct mailcap* searchExtViewer(const char* type);
Str unquote_mailcap(const char* qstr, const char* type, const char* name, const char* attr, int* mc_stat);
