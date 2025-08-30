#pragma once
#include "Str.h"
#include "url_scheme.h"
#include "textlist.h"

#include <wc.h>

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

typedef struct http_request {
    char command;
    char flag;
    char* referer;
    struct form_list* request;
} HRequest;

struct _Buffer;

void parseURL(char* url, ParsedURL* p_url, ParsedURL* current);
void copyParsedURL(ParsedURL* p, const ParsedURL* q);
void parseURL2(char* url, ParsedURL* pu, ParsedURL* current);
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
Str HTTPrequestMethod(HRequest* hr);
Str HTTPrequestURI(ParsedURL* pu, HRequest* hr);
void initMimeTypes(void);
TextList* make_domain_list(char* domain_list);
int check_no_proxy(char* domain);
