#pragma once
#include "textlist.h"
#include "istream.h"
#include "compression.h"
#include "urlscheme.h"
#include <time.h>
#include <stdbool.h>
#include <libwc/wc_types.h>

union input_stream;
struct URLFile {
    enum UrlScheme scheme;
    char is_cgi;
    enum EncodingType encoding;
    union input_stream* stream;
    const char* ext;
    enum CompressionType compression;
    int content_encoding;
    const char* guess_type;
    char* ssl_certificate;
    char* url;
    time_t modtime;
};

struct URLOption {
    char* referer;
    int flag;
};

#define StrUFgets(f) StrISgets((f)->stream)
#define StrmyUFgets(f) StrmyISgets((f)->stream)
#define UFgetc(f) ISgetc((f)->stream)
#define UFundogetc(f) ISundogetc((f)->stream)
#define UFclose(f)                   \
    if (ISclose((f)->stream) == 0) { \
        (f)->stream = NULL;          \
    }
#define UFfileno(f) ISfileno((f)->stream)

struct Url;
struct FormList;
struct HttpRequest;
struct Buffer;
void init_stream(struct URLFile* uf, int scheme, union input_stream* stream);
int doFileSave(struct URLFile uf, const char* defstr);
struct URLFile openURL(const char* url, struct Url* pu, struct Url* current,
    struct URLOption* option, struct FormList* request,
    TextList* extra_header, struct URLFile* ouf,
    struct HttpRequest* hr, unsigned char* status, bool do_download);
void loadHTMLstream(struct URLFile* f, struct Buffer* newBuf, FILE* src, int internal);
struct Buffer* doExternal(struct URLFile uf, const char* type, struct Buffer* defaultbuf);
struct Buffer* loadHTMLBuffer(struct URLFile* f, struct Buffer* newBuf);
struct Buffer* loadBuffer(struct URLFile* uf, struct Buffer* newBuf);
struct Buffer* loadImageBuffer(struct URLFile* uf, struct Buffer* newBuf);
Str convertLine(struct URLFile* uf, Str line, int mode, wc_ces* charset, wc_ces doc_charset);
Str loadGopherDir(struct URLFile* uf, struct Url* pu, wc_ces* charset);
Str loadGopherSearch(struct URLFile* uf, struct Url* pu, wc_ces* charset);
int save2tmp(struct URLFile uf, char* tmpf);
void UFhalfclose(struct URLFile* f);
