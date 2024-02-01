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
struct ParsedURL {
  UrlSchema schema;
  const char *user;
  const char *pass;
  const char *host;
  int port;
  char *file;
  char *real_file;
  char *query;
  char *label;
  int is_nocache;
};

extern ParsedURL HTTP_proxy_parsed;
extern ParsedURL HTTPS_proxy_parsed;
extern ParsedURL FTP_proxy_parsed;

#define IS_EMPTY_PARSED_URL(pu) ((pu)->schema == SCM_UNKNOWN && !(pu)->file)

void parseURL(char *url, ParsedURL *p_url, ParsedURL *current);
void copyParsedURL(ParsedURL *p, const ParsedURL *q);
void parseURL2(char *url, ParsedURL *pu, ParsedURL *current);
Str *parsedURL2Str(ParsedURL *pu);
Str *parsedURL2RefererStr(ParsedURL *pu);
Str *_parsedURL2Str(ParsedURL *pu, int pass, int user, int label);

struct TextList;
int check_no_proxy(const char *domain);
int openSocket(const char *hostname, const char *remoteport_name,
               unsigned short remoteport_num);


struct ParsedURL;
ParsedURL *schemaToProxy(UrlSchema schema);

