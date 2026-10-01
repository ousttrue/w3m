#pragma once
#include "url.h"
#include "url_stream.h"
#include "Str.h"
#include "form.h"
#include "textlist.h"

#define NO_REFERER ((char*)-1)

typedef struct http_request {
    char command;
    char flag;
    char* referer;
    FormList* request;
} HRequest;

#define HR_COMMAND_GET 0
#define HR_COMMAND_POST 1
#define HR_COMMAND_CONNECT 2
#define HR_COMMAND_HEAD 3

#define HR_FLAG_LOCAL 1
#define HR_FLAG_PROXY 2

#define HTST_UNKNOWN 255
#define HTST_MISSING 254
#define HTST_NORMAL 0
#define HTST_CONNECT 1

Str HTTPrequestMethod(HRequest* hr);
Str HTTPrequestURI(ParsedURL* pu, HRequest* hr);
URLFile openURL(const char* url, ParsedURL* pu, ParsedURL* current,
    struct URLOption* option, FormList* request,
    TextList* extra_header, URLFile* ouf,
    HRequest* hr, unsigned char* status);

extern Str header_string;
extern int override_content_type;
extern int override_user_agent;
extern char* w3m_reqlog;

extern char use_proxy;
extern char* HTTP_proxy;
extern ParsedURL HTTP_proxy_parsed;
#ifdef USE_SSL
extern char* HTTPS_proxy;
extern ParsedURL HTTPS_proxy_parsed;
#endif /* USE_SSL */
extern char* FTP_proxy;
extern ParsedURL FTP_proxy_parsed;
#ifdef USE_GOPHER
extern char* GOPHER_proxy;
extern ParsedURL GOPHER_proxy_parsed;
#endif /* USE_GOPHER */
extern char* NO_proxy;
extern TextList* NO_proxy_domains;
extern int NOproxy_netaddr;
extern char NoCache;

#ifdef INET6
extern int ai_family_order_table[7][3]; /* XXX */
#endif

#if defined(USE_SSL)
#include <openssl/ssl.h>
extern char* ssl_forbid_method;
extern char* ssl_min_version;
extern char* ssl_cipher;
#if defined(USE_SSL_VERIFY)
extern int ssl_verify_server;
extern char* ssl_cert_file;
extern char* ssl_key_file;
extern char* ssl_ca_path;
extern char* ssl_ca_file;
extern int ssl_ca_default;
extern int ssl_path_modified;
#endif
#endif

int needs_proxy(const char* domain);
char* url_encode(const char* url, ParsedURL* base, wc_ces doc_charset);
int openSocket(const char* hostname,
    const char* remoteport_name, unsigned short remoteport_num);
wc_ces url_to_charset(const char* url, ParsedURL* base, wc_ces doc_charset);
struct _Buffer;
Str url_decode2(const char* url, struct _Buffer* buf);
