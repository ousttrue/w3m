#pragma once
#include "url_scheme.h"
#include "compression.h"
#include <time.h>

struct URLFile {
    enum UrlScheme scheme;
    char is_cgi;
    char encoding;
    union input_stream* stream;
    const char* ext;
    enum CompressionTyep compression;
    int content_encoding;
    const char* guess_type;
    const char* ssl_certificate;
    const char* url;
    time_t modtime;
};

void examineFile(struct URLFile* uf, const char* path);

struct _Buffer;
struct _Buffer* loadHTMLBuffer(struct URLFile* f, struct _Buffer* newBuf);
struct _Buffer* loadBuffer(struct URLFile* uf, struct _Buffer* newBuf);
int doFileSave(struct URLFile uf, const char* defstr, int current_content_length);
union input_stream;
void init_stream(struct URLFile* uf, int scheme, union input_stream* stream);
