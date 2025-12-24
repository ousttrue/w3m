#pragma once
#include "Str.h"
#include "urlscheme.h"
#include <stdbool.h>

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
#define IS_EMPTY_PARSED_URL(pu) ((pu)->scheme == SCM_UNKNOWN && !(pu)->file)
void parseURL2(const char* url, struct Url* pu, struct Url* current);
void parseURL(const char* url, struct Url* p_url, struct Url* current);
Str _parsedURL2Str(struct Url* pu, bool pass, bool user, bool label);
Str parsedURL2Str(struct Url* pu);

