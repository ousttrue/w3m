#pragma once
#include "Str.h"
#include "urlscheme.h"
#include <stdbool.h>
#include <libwc/ces.h>

#define NO_REFERER ((char*)-1)

struct Url {
    enum UrlScheme scheme;
    char* user;
    char* pass;
    char* host;
    int port;
    char* file;
    const char* real_file;
    char* query;
    char* label;
    int is_nocache;
};
extern struct Url HTTP_proxy_parsed;
extern struct Url HTTPS_proxy_parsed;
extern struct Url FTP_proxy_parsed;

struct TextList* make_domain_list(const char* domain_list);

#define IS_EMPTY_PARSED_URL(pu) ((pu)->scheme == SCM_UNKNOWN && !(pu)->file)
void parseURL2(const char* url, struct Url* pu, struct Url* current);
void parseURL(const char* url, struct Url* p_url, struct Url* current);
Str _parsedURL2Str(struct Url* pu, bool pass, bool user, bool label);
Str parsedURL2Str(struct Url* pu);
const char* guessContentType(const char* filename);
const char* filename_extension(const char* patch, int is_url);
void parse_proxy(void);
extern Str searchURIMethods(struct Url* pu);
extern void copyParsedURL(struct Url* p, const struct Url* q);
extern Str parsedURL2RefererStr(struct Url* pu);
extern struct Url* schemeToProxy(int scheme);
extern enum wc_ces url_to_charset(const char* url, const struct Url* base,
    enum wc_ces doc_charset);
extern char* url_encode(const char* url, const struct Url* base,
    enum wc_ces doc_charset);
struct Buffer;
extern void chkExternalURIBuffer(struct Buffer* buf);
extern void initMimeTypes(void);
extern void initURIMethods(void);

char* file_to_url(const char* file);
