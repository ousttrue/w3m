#pragma once
#include <stdio.h>
#include <wc.h>

extern char* tmp_dir;

struct _Buffer;
struct parsed_tagarg;
struct _ParsedURL;

void show_params(FILE* fp);
int str_to_bool(char* value, int old);
int set_param_option(char* option);
char* get_param_option(char* name);
void init_rc(void);
void init_tmp(void);
struct _Buffer* load_option_panel(void);
void panel_set_option(struct parsed_tagarg*);
void sync_with_option(void);
char* rcFile(char* base);
char* etcFile(char* base);
char* confFile(char* base);
char* auxbinFile(char* base);
// char* libFile(char* base);
// char* helpFile(char* base);

#define SCONF_RESERVED 0
#define SCONF_SUBSTITUTE_URL 1
#define SCONF_URL_CHARSET 2
#define SCONF_NO_REFERER_FROM 3
#define SCONF_NO_REFERER_TO 4
#define SCONF_USER_AGENT 5
#define SCONF_N_FIELD 6

const void* querySiteconf(const struct _ParsedURL* query_pu, int field);

inline static const char* query_SCONF_SUBSTITUTE_URL(const struct _ParsedURL* pu)
{
    return ((const char*)querySiteconf(pu, SCONF_SUBSTITUTE_URL));
}
inline static const char* query_SCONF_USER_AGENT(const struct _ParsedURL* pu)
{
    return ((const char*)querySiteconf(pu, SCONF_USER_AGENT));
}
inline static const wc_ces* query_SCONF_URL_CHARSET(const struct _ParsedURL* pu)
{
    return ((const wc_ces*)querySiteconf(pu, SCONF_URL_CHARSET));
}
inline static const int* query_SCONF_NO_REFERER_FROM(const struct _ParsedURL* pu)
{
    return ((const int*)querySiteconf(pu, SCONF_NO_REFERER_FROM));
}
inline static const int* query_SCONF_NO_REFERER_TO(const struct _ParsedURL* pu)
{
    return ((const int*)querySiteconf(pu, SCONF_NO_REFERER_TO));
}
