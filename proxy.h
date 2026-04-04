#pragma once
#include "url.h"

extern ParsedURL HTTP_proxy_parsed;
extern ParsedURL HTTPS_proxy_parsed;
extern ParsedURL GOPHER_proxy_parsed;
extern ParsedURL FTP_proxy_parsed;
// #define Do_not_use_proxy (!use_proxy)
// global TextList* NO_proxy_domains;

void proxyInit();
void parse_proxy(void);
bool check_no_proxy(const char* domain);
