#pragma once
#include <stdio.h>
#include "url_schema.h"
#include "url.h"

extern const char *rc_dir;

struct Url;
void show_params(FILE *fp);
int str_to_bool(const char *value, int old);

#define INET6 1
#ifdef INET6
#define DNS_ORDER_UNSPEC 0
#define DNS_ORDER_INET_INET6 1
#define DNS_ORDER_INET6_INET 2
#define DNS_ORDER_INET_ONLY 4
#define DNS_ORDER_INET6_ONLY 6
extern int ai_family_order_table[7][3]; /* XXX */
#endif                                  /* INET6 */
extern int DNS_order;

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
const char *w3m_auxbin_dir();
const char *w3m_lib_dir();
const char *w3m_etc_dir();
const char *w3m_conf_dir();
const char *w3m_help_dir();
void sync_with_option(void);
struct Buffer;
std::shared_ptr<Buffer> load_option_panel(void);
