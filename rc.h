#pragma once

extern int no_rc_dir;

extern void show_params(FILE* fp);
extern int str_to_bool(char* value, int old);
extern char* confFile(char* base);
extern char* rcFile(char* base);

#define SCONF_RESERVED 0
#define SCONF_SUBSTITUTE_URL 1
#define SCONF_URL_CHARSET 2
#define SCONF_NO_REFERER_FROM 3
#define SCONF_NO_REFERER_TO 4
#define SCONF_USER_AGENT 5
#define SCONF_N_FIELD 6
#define query_SCONF_SUBSTITUTE_URL(pu) ((const char*)querySiteconf(pu, SCONF_SUBSTITUTE_URL))
#define query_SCONF_URL_CHARSET(pu) ((const wc_ces*)querySiteconf(pu, SCONF_URL_CHARSET))
#define query_SCONF_NO_REFERER_FROM(pu) ((const int*)querySiteconf(pu, SCONF_NO_REFERER_FROM))
#define query_SCONF_NO_REFERER_TO(pu) ((const int*)querySiteconf(pu, SCONF_NO_REFERER_TO))

struct Url;
extern const void* querySiteconf(const struct Url* query_pu, int field);
