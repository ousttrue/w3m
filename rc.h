/* vi: set sw=4 ts=8 ai sm noet : */
#ifndef W3M_RC_H
#define W3M_RC_H

#include "buffer.h"
#include "config.h"
#include "parsetag.h"

#include <stdio.h>

Buffer * load_option_panel(void);
char * auxbinFile(const char *base);
char * confFile(const char *base);
char * get_param_option(char *name);
char * rcFile(char *base);
int set_param_option(char *option);
int str_to_bool(const char *value, int old);
void * querySiteconf(ParsedURL *query_pu, int field);
void init_rc(void);
void panel_set_option(struct parsed_tagarg *arg);
void show_params(FILE * fp);
void sync_with_option(void);

#ifndef USE_HELP_CGI
char * helpFile(const char *base);
#endif

#define IS_EMPTY_PARSED_URL(pu) ((pu)->scheme == SCM_UNKNOWN && !(pu)->file)
#define SCONF_RESERVED		0
#define SCONF_SUBSTITUTE_URL	1
#define SCONF_URL_CHARSET	2
#define SCONF_NO_REFERER_FROM	3
#define SCONF_NO_REFERER_TO	4
#define SCONF_USER_AGENT	5
#define SCONF_N_FIELD		6
#define query_SCONF_SUBSTITUTE_URL(pu) ((char *)querySiteconf(pu, SCONF_SUBSTITUTE_URL))
#define query_SCONF_USER_AGENT(pu) ((const char *)querySiteconf(pu, SCONF_USER_AGENT))
#define query_SCONF_URL_CHARSET(pu) ((const wc_ces *)querySiteconf(pu, SCONF_URL_CHARSET))
#define query_SCONF_NO_REFERER_FROM(pu) ((const int *)querySiteconf(pu, SCONF_NO_REFERER_FROM))
#define query_SCONF_NO_REFERER_TO(pu) ((const int *)querySiteconf(pu, SCONF_NO_REFERER_TO))

extern int use_cookie;
extern int show_cookie;
extern int accept_cookie;
extern int accept_bad_cookie;
extern char *cookie_reject_domains;
extern char *cookie_accept_domains;
extern char *cookie_avoid_wrong_number_of_dots;

#endif
