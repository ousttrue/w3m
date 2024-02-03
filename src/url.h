#pragma once
#include "url_schema.h"

extern bool ArgvIsURL;
extern bool LocalhostOnly;
extern bool retryAsHttp;
extern char *HTTP_proxy;
extern char *HTTPS_proxy;
extern char *FTP_proxy;
extern char *NO_proxy;
extern int NOproxy_netaddr;
extern bool use_proxy;
// #define Do_not_use_proxy (!use_proxy)
extern const char *w3m_reqlog;
struct TextList;
extern TextList *NO_proxy_domains;

extern const char *mimetypes_files;


struct Str;
struct Url {
  UrlSchema schema;
  const char *user;
  const char *pass;
  const char *host;
  int port;
  const char *file;
  const char *real_file;
  const char *query;
  const char *label;
  int is_nocache;
};

extern Url HTTP_proxy_parsed;
extern Url HTTPS_proxy_parsed;
extern Url FTP_proxy_parsed;

#define IS_EMPTY_PARSED_URL(pu) ((pu)->schema == SCM_UNKNOWN && !(pu)->file)

void parseURL(const char *url, Url *p_url, Url *current);
void copyUrl(Url *p, const Url *q);
void parseURL2(const char *url, Url *pu, Url *current);
Str *Url2Str(Url *pu);
Str *Url2RefererStr(Url *pu);
Str *_Url2Str(Url *pu, int pass, int user, int label);

struct Url;
Url *schemaToProxy(UrlSchema schema);
const char *url_decode0(const char *url);
#define url_decode2(url, buf) url_decode0(url)
