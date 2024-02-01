#pragma once

#define SCM_UNKNOWN 255
#define SCM_MISSING 254
#define SCM_HTTP 0
#define SCM_GOPHER 1
#define SCM_FTP 2
#define SCM_FTPDIR 3
#define SCM_LOCAL 4
#define SCM_LOCAL_CGI 5
#define SCM_EXEC 6
#define SCM_NNTP 7
#define SCM_NNTP_GROUP 8
#define SCM_NEWS 9
#define SCM_NEWS_GROUP 10
#define SCM_DATA 11
#define SCM_MAILTO 12
#define SCM_HTTPS 13

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
extern char *index_file;
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

/* XXX: note html.h SCM_ */
inline int DefaultPort[] = {
    80,  /* http */
    70,  /* gopher */
    21,  /* ftp */
    21,  /* ftpdir */
    0,   /* local - not defined */
    0,   /* local-CGI - not defined? */
    0,   /* exec - not defined? */
    119, /* nntp */
    119, /* nntp group */
    119, /* news */
    119, /* news group */
    0,   /* data - not defined */
    0,   /* mailto - not defined */
    443, /* https */
};

struct Str;
struct ParsedURL {
  int scheme;
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

#define IS_EMPTY_PARSED_URL(pu) ((pu)->scheme == SCM_UNKNOWN && !(pu)->file)

void parseURL(char *url, ParsedURL *p_url, ParsedURL *current);
void copyParsedURL(ParsedURL *p, const ParsedURL *q);
void parseURL2(char *url, ParsedURL *pu, ParsedURL *current);
Str *parsedURL2Str(ParsedURL *pu);
Str *parsedURL2RefererStr(ParsedURL *pu);
int getURLScheme(char **url);
Str *_parsedURL2Str(ParsedURL *pu, int pass, int user, int label);

struct TextList;
TextList *make_domain_list(const char *domain_list);
int check_no_proxy(const char *domain);
int openSocket(const char *hostname, const char *remoteport_name,
               unsigned short remoteport_num);
