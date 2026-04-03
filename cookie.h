#pragma once
#include "Str.h"
#include <time.h>

char* FQDN(char* host);
struct _ParsedURL;
Str find_cookie(struct _ParsedURL* pu);
int add_cookie(struct _ParsedURL* pu, Str name, Str value, time_t expires, Str domain, Str path, int flag, Str comment, int version, Str port, Str commentURL);
void save_cookies(void);
void load_cookies(void);
void initCookie(void);
struct _Buffer* cookie_list_panel(void);
struct parsed_tagarg;
void set_cookie_flag(struct parsed_tagarg* arg);
int check_cookie_accept_domain(char* domain);
