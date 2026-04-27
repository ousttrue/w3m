#pragma once
#include "Str.h"
#include "url_scheme.h"
#include <stdbool.h>
#include <libwc/wc_types.h>

struct CmdArgs;
struct Url;
struct InputStream;

struct HttpResponse {
    int status_code;
    struct _textlist* headers;
};

struct HttpResponse http_response_header(struct InputStream* stream, enum UrlScheme scheme);
const char* http_response_save_header_source(struct HttpResponse* res);
enum ContentCompression http_response_process(struct HttpResponse* res, struct CmdArgs* args, struct Url* pu);
bool matchattr(const char* p, const char* attr, int len, Str* value);
const char* http_response_get(struct HttpResponse* res, const char* field);
const char* http_response_get_content_type(struct HttpResponse* res, wc_ces* content_charset);
const char* http_response_guess_save_name(struct HttpResponse *res, const char* path);
