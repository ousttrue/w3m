#pragma once
#include "http_request.h"
#include "url.h"
#include "textlist.h"
#include "compression.h"
#include "input_stream.h"
#include "urlscheme.h"
#include <time.h>
#include <stdbool.h>
#include <libwc/wc_types.h>

struct input_stream;
struct URLFile {
    struct input_stream* stream;
};

struct URLOption {
    const char* referer;
    int flag;
    struct TextList* extra_header;
};

struct FormList;
struct HttpRequest;
struct Buffer;
int doFileSave(struct Url url, struct URLFile uf, const char* defstr,
    enum CompressionType compression);

struct UrlStream {
    struct URLFile uf;
    bool is_cgi;
    const char* url_str;
    struct Url url;
    struct HttpRequest hr;
    unsigned char status;
    const char* ssl_certificate;
    time_t modtime;
};
struct UrlStream openURL(const char* url, struct Url* current,
    struct FormList* request,
    struct URLOption option,
    struct URLFile* ouf,
    bool do_download);

Str loadGopherDir(struct URLFile* uf, struct Url* pu, wc_ces* charset);
Str loadGopherSearch(struct URLFile* uf, struct Url* pu, wc_ces* charset);
void UFhalfclose(struct URLFile* f, enum UrlScheme scheme);

