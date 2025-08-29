#pragma once
#include "Str.h"
#include "url_scheme.h"
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
