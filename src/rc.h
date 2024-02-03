#pragma once
#include <stdio.h>

extern const char *rc_dir;
extern bool multicolList;
extern bool DecodeURL;

struct Url;
void show_params(FILE *fp);
int str_to_bool(const char *value, int old);
const void *querySiteconf(const Url *query_pu, int field);

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

const char *rcFile(const char *base);
const char *etcFile(const char *base);
const char *confFile(const char *base);
const char *auxbinFile(const char *base);
const char *libFile(const char *base);
const char *helpFile(const char *base);
int set_param_option(const char *option);
const char *get_param_option(const char *name);
void init_rc();
void init_tmp();
