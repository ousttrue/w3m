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

typedef struct _ParsedURL {
    enum UrlScheme scheme;
    const char* user;
    const char* pass;
    const char* host;
    int port;
    const char* file;
    const char* real_file;
    const char* query;
    const char* label;
    int is_nocache;
} ParsedURL;

inline static bool IS_EMPTY_PARSED_URL(struct _ParsedURL* pu)
{
    return ((pu)->scheme == SCM_UNKNOWN && !(pu)->file);
}

Str _parsedURL2Str(ParsedURL* pu, int pass, int user, int label);
void parseURL(const char* url, ParsedURL* p_url, ParsedURL* current);
void copyParsedURL(ParsedURL* p, const ParsedURL* q);
void parseURL2(const char* url, ParsedURL* pu, ParsedURL* current);
Str parsedURL2Str(ParsedURL* pu);
Str parsedURL2RefererStr(ParsedURL* pu);
const char* filename_extension(const char* path, int is_url);
struct _ParsedURL* schemeToProxy(int scheme);
wc_ces url_to_charset(const char* url, const ParsedURL* base, wc_ces doc_charset);
const char* url_encode(const char* url, const ParsedURL* base, wc_ces doc_charset);

int same_url_p(ParsedURL* pu1, ParsedURL* pu2);
char* file_to_url(const char* file);
char* cleanupName(const char* name);
int is_localhost(const char* host);
