#pragma once

extern const char *siteconf_file;

enum SiteConfField {
  SCONF_RESERVED = 0,
  SCONF_SUBSTITUTE_URL = 1,
  SCONF_URL_CHARSET = 2,
  SCONF_NO_REFERER_FROM = 3,
  SCONF_NO_REFERER_TO = 4,
  SCONF_USER_AGENT = 5,
  SCONF_N_FIELD = 6,
};

struct Url;
const void *querySiteconf(struct Url *query_pu, enum SiteConfField field);
void loadSiteconf();

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
