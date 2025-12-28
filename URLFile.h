#pragma once
#include "textlist.h"
#include "compression.h"
#include "input_stream.h"
#include "urlscheme.h"
#include <time.h>
#include <stdbool.h>
#include <libwc/wc_types.h>

struct input_stream;
struct URLFile {
    enum UrlScheme scheme;
    char is_cgi;
    struct input_stream* stream;
    const char* ext;
    const char* ssl_certificate;
    char* url;
    time_t modtime;
};

struct URLOption {
    const char* referer;
    int flag;
    struct TextList* extra_header;
};

struct Url;
struct FormList;
struct HttpRequest;
struct Buffer;
void init_stream(struct URLFile* uf, int scheme, struct input_stream* stream);
int doFileSave(struct URLFile uf, const char* defstr,
    enum CompressionType compression);
struct URLFile openURL(const char* url, struct Url* pu, struct Url* current,
    struct URLOption option, struct FormList* request,
    struct URLFile* ouf,
    struct HttpRequest* hr, unsigned char* status, bool do_download);

typedef struct Buffer* (*LoadBufferFunc)(struct URLFile*, const char* type,
    struct Buffer*, bool internal);
struct Buffer* doExternal(struct URLFile* uf, const char* type,
    struct Buffer* defaultbuf, bool internal);
struct Buffer* loadHTMLBuffer(struct URLFile* f, const char* type,
    struct Buffer* newBuf, bool internal);
struct Buffer* loadBuffer(struct URLFile* uf, const char* type,
    struct Buffer* newBuf, bool internal);
struct Buffer* loadImageBuffer(struct URLFile* uf, const char* type,
    struct Buffer* newBuf, bool internal);

Str loadGopherDir(struct URLFile* uf, struct Url* pu, wc_ces* charset);
Str loadGopherSearch(struct URLFile* uf, struct Url* pu, wc_ces* charset);
void UFhalfclose(struct URLFile* f);
