#pragma once
#include "url.h"

extern struct Url HTTP_proxy_parsed;
extern struct Url HTTPS_proxy_parsed;
extern struct Url GOPHER_proxy_parsed;
extern struct Url FTP_proxy_parsed;
// #define Do_not_use_proxy (!use_proxy)
// global TextList* NO_proxy_domains;

void proxyInit();
void parse_proxy(void);
bool check_no_proxy(const char* domain);
