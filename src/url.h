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

#define INET6 1
#ifdef INET6
#define DNS_ORDER_UNSPEC 0
#define DNS_ORDER_INET_INET6 1
#define DNS_ORDER_INET6_INET 2
#define DNS_ORDER_INET_ONLY 4
#define DNS_ORDER_INET6_ONLY 6
extern int ai_family_order_table[7][3]; /* XXX */
#endif                                  /* INET6 */

extern const char *mimetypes_files;

extern int DNS_order;

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

struct TextList;
int check_no_proxy(const char *domain);
int openSocket(const char *hostname, const char *remoteport_name,
               unsigned short remoteport_num);

struct Url;
Url *schemaToProxy(UrlSchema schema);
const char *url_decode0(const char *url);
#define url_decode2(url, buf) url_decode0(url)
