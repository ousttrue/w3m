#pragma once
#include "Str.h"
#include "url_scheme.h"
#include "textlist.h"

#include <wc.h>

extern char* mimetypes_files;

#define IS_EMPTY_PARSED_URL(pu) ((pu)->scheme == SCM_UNKNOWN && !(pu)->file)

#define DNS_ORDER_UNSPEC 0
#define DNS_ORDER_INET_INET6 1
#define DNS_ORDER_INET6_INET 2
#define DNS_ORDER_INET_ONLY 4
#define DNS_ORDER_INET6_ONLY 6
extern int DNS_order;
extern int ai_family_order_table[7][3]; /* XXX */
extern char ArgvIsURL;
extern char LocalhostOnly;
extern char* document_root;
extern int retryAsHttp;
extern char* w3m_reqlog;
extern char* index_file;
extern int DecodeURL;

#define HTST_UNKNOWN 255
#define HTST_MISSING 254
#define HTST_NORMAL 0
#define HTST_CONNECT 1

typedef struct _ParsedURL {
    enum UrlScheme scheme;
    char* user;
    char* pass;
    char* host;
    int port;
    char* file;
    char* real_file;
    char* query;
    char* label;
    int is_nocache;
} ParsedURL;

struct form_list;

struct _Buffer;

Str _parsedURL2Str(ParsedURL* pu, int pass, int user, int label);
void parseURL(char* url, ParsedURL* p_url, ParsedURL* current);
void copyParsedURL(ParsedURL* p, const ParsedURL* q);
void parseURL2(const char* url, ParsedURL* pu, ParsedURL* current);
Str parsedURL2Str(ParsedURL* pu);
Str parsedURL2RefererStr(ParsedURL* pu);
char* guessContentType(char* filename);
char* filename_extension(char* path, int is_url);
struct _ParsedURL* schemeToProxy(int scheme);
wc_ces url_to_charset(const char* url, const ParsedURL* base, wc_ces doc_charset);
char* url_encode(const char* url, const ParsedURL* base, wc_ces doc_charset);
struct _Buffer;
char* url_decode2(const char* url, const struct _Buffer* buf);
ParsedURL* baseURL(struct _Buffer* buf);
int openSocket(char* hostname, char* remoteport_name,
    unsigned short remoteport_num);

void initMimeTypes(void);
TextList* make_domain_list(char* domain_list);
int check_no_proxy(char* domain);
