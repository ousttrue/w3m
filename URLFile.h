#pragma once
#include "textlist.h"
#include "istream.h"
#include <time.h>
#include <stdbool.h>
#include <libwc/wc_types.h>

enum CompressionType {
    CMP_NOCOMPRESS = 0,
    CMP_COMPRESS = 1,
    CMP_GZIP = 2,
    CMP_BZIP2 = 3,
    CMP_DEFLATE = 4,
    CMP_BROTLI = 5,
};

union input_stream;
typedef struct URLFile {
    unsigned char scheme;
    char is_cgi;
    enum EncodingType encoding;
    union input_stream* stream;
    char* ext;
    enum CompressionType compression;
    int content_encoding;
    const char* guess_type;
    char* ssl_certificate;
    char* url;
    time_t modtime;
} URLFile;

typedef struct {
    char* referer;
    int flag;
} URLOption;

struct Url;
struct FormList;
struct HttpRequest;
struct Buffer;
void init_stream(URLFile* uf, int scheme, union input_stream* stream);
int doFileSave(URLFile uf, const char* defstr);
struct URLFile openURL(const char* url, struct Url* pu, struct Url* current,
    URLOption* option, struct FormList* request,
    TextList* extra_header, URLFile* ouf,
    struct HttpRequest* hr, unsigned char* status, bool do_download);
void loadHTMLstream(URLFile* f, struct Buffer* newBuf, FILE* src, int internal);
struct Buffer* doExternal(URLFile uf, const char* type, struct Buffer* defaultbuf);
struct Buffer* loadHTMLBuffer(URLFile* f, struct Buffer* newBuf);
struct Buffer* loadBuffer(URLFile* uf, struct Buffer* newBuf);
struct Buffer* loadImageBuffer(URLFile* uf, struct Buffer* newBuf);
Str convertLine(URLFile* uf, Str line, int mode, wc_ces* charset, wc_ces doc_charset);
Str loadGopherDir(URLFile* uf, struct Url* pu, wc_ces* charset);
Str loadGopherSearch(URLFile* uf, struct Url* pu, wc_ces* charset);
int save2tmp(URLFile uf, char* tmpf);
