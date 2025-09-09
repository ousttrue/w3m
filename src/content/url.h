#pragma once
#include "url_scheme.h"
#include "Str.h"
#include <stdbool.h>

struct Url {
    enum UrlScheme scheme;
    const char* user;
    const char* pass;
    const char* host;
    int port;
    const char* file;
    const char* query;
    const char* label;
};

inline static bool IS_EMPTY_PARSED_URL(struct Url* pu)
{
    return ((pu)->scheme == SCM_UNKNOWN && !(pu)->file);
}
struct Url copyParsedURL(const struct Url* q);
void parseUrl(const char* url, struct Url* pu, struct Url* current);

Str _parsedURL2Str(struct Url* pu, int pass, int user, int label);
Str parsedURL2Str(struct Url* pu);
Str parsedURL2RefererStr(struct Url* pu);
const char* filename_extension(const char* path, int is_url);
int same_url_p(struct Url* pu1, struct Url* pu2);
char* cleanupName(const char* name);
int is_localhost(const char* host);
