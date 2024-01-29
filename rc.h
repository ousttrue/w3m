#pragma once
#include <stdio.h>

extern char *rc_dir;

struct ParsedURL;
void show_params(FILE *fp);
int str_to_bool(char *value, int old);
const void *querySiteconf(const ParsedURL *query_pu, int field);

#define SCONF_RESERVED 0
#define SCONF_SUBSTITUTE_URL 1
#define SCONF_URL_CHARSET 2
#define SCONF_NO_REFERER_FROM 3
#define SCONF_NO_REFERER_TO 4
#define SCONF_USER_AGENT 5
#define SCONF_N_FIELD 6
#define query_SCONF_SUBSTITUTE_URL(pu)                                         \
  ((const char *)querySiteconf(pu, SCONF_SUBSTITUTE_URL))
#define query_SCONF_USER_AGENT(pu)                                             \
  ((const char *)querySiteconf(pu, SCONF_USER_AGENT))
#define query_SCONF_URL_CHARSET(pu)                                            \
  ((const wc_ces *)querySiteconf(pu, SCONF_URL_CHARSET))
#define query_SCONF_NO_REFERER_FROM(pu)                                        \
  ((const int *)querySiteconf(pu, SCONF_NO_REFERER_FROM))
#define query_SCONF_NO_REFERER_TO(pu)                                          \
  ((const int *)querySiteconf(pu, SCONF_NO_REFERER_TO))
