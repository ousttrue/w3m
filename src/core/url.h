#pragma once
#include "Str.h"
#include "url_scheme.h"
#include "textlist.h"

#include <wc.h>

extern char ArgvIsURL;
extern char LocalhostOnly;
extern char* document_root;
extern int retryAsHttp;
extern char* index_file;
extern int DecodeURL;

struct Url {
    enum UrlScheme scheme;
    const char* user;
    const char* pass;
    const char* host;
    int port;
    const char* file;
    const char* real_file;
    const char* query;
    const char* label;
    bool is_nocache;
};

inline static bool IS_EMPTY_PARSED_URL(struct Url* pu)
{
    return ((pu)->scheme == SCM_UNKNOWN && !(pu)->file);
}

Str _parsedURL2Str(struct Url* pu, int pass, int user, int label);
void parseURL(const char* url, struct Url* p_url, struct Url* current);
void copyParsedURL(struct Url* p, const struct Url* q);
void parseURL2(const char* url, struct Url* pu, struct Url* current);
Str parsedURL2Str(struct Url* pu);
Str parsedURL2RefererStr(struct Url* pu);
const char* filename_extension(const char* path, int is_url);
struct Url* schemeToProxy(int scheme);
const char* url_encode(const char* url, const struct Url* base, wc_ces doc_charset);

int same_url_p(struct Url* pu1, struct Url* pu2);
char* file_to_url(const char* file);
char* cleanupName(const char* name);
int is_localhost(const char* host);
