#pragma once
#include "Str.h"
#include "url_scheme.h"
#include <stdbool.h>

struct CmdArgs;
struct URLFile;
struct Url;
struct InputStream;

struct HttpResponse {
    int status_code;
    struct _textlist* headers;
};

struct HttpResponse http_response_header(struct InputStream* stream, enum UrlScheme scheme);

const char* http_response_save_header_source(struct HttpResponse*);

void http_response_process(struct HttpResponse* http_response, struct CmdArgs* args, struct URLFile* uf, struct Url* pu);

bool matchattr(const char* p, const char* attr, int len, Str* value);
