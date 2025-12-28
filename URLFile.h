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

inline static void UFclose(struct URLFile* f)
{
    if (!f->stream->unclose) {
        is_close(f->stream);
        f->stream = NULL;
    }
}

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
void loadHTMLstream(struct URLFile* f, struct Buffer* newBuf, FILE* src, int internal);
struct Buffer* doExternal(struct URLFile uf, const char* type, struct Buffer* defaultbuf);
struct Buffer* loadHTMLBuffer(struct URLFile* f, struct Buffer* newBuf);
struct Buffer* loadBuffer(struct URLFile* uf, struct Buffer* newBuf);
struct Buffer* loadImageBuffer(struct URLFile* uf, struct Buffer* newBuf);
Str loadGopherDir(struct URLFile* uf, struct Url* pu, wc_ces* charset);
Str loadGopherSearch(struct URLFile* uf, struct Url* pu, wc_ces* charset);
bool uf_save2tmp(struct URLFile uf, const char* tmpf);
void UFhalfclose(struct URLFile* f);
