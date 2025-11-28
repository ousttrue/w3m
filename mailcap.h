#pragma once
#include <gcstr.h>
#include <stdbool.h>

extern const char* mailcap_files;

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
struct mailcap* searchExtViewer(char* type);
Str unquote_mailcap(char* qstr, char* type, char* name, char* attr,
    int* mc_stat);

bool is_text_type(const char* type);
bool is_plain_text_type(const char* type);
bool is_dump_text_type(const char* type);
bool is_html_type(const char* type);
