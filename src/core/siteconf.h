#pragma once
#include <wc.h>

extern char* siteconf_file;

enum SiteConfType {
    SCONF_RESERVED = 0,
    SCONF_SUBSTITUTE_URL = 1,
    SCONF_URL_CHARSET = 2,
    SCONF_NO_REFERER_FROM = 3,
    SCONF_NO_REFERER_TO = 4,
    SCONF_USER_AGENT = 5,
    SCONF_N_FIELD = 6,
};

struct Url;
const char* query_SCONF_SUBSTITUTE_URL(const struct Url* pu);
const char* query_SCONF_USER_AGENT(const struct Url* pu);
const wc_ces* query_SCONF_URL_CHARSET(const struct Url* pu);
const int* query_SCONF_NO_REFERER_FROM(const struct Url* pu);
const int* query_SCONF_NO_REFERER_TO(const struct Url* pu);
void loadSiteconf();
