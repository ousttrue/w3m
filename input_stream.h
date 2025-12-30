#pragma once
#include "url.h"
#include "http_request.h"
#include "ssl_stream.h"
#include "compression.h"
#include <stdbool.h>
#include <stdio.h>

struct input_stream;
struct input_stream* is_from_fd(int fd);
typedef int (*FileCloseFunc)(FILE* stream);
struct input_stream* is_from_file(FILE* f, FileCloseFunc closep);
struct input_stream* is_from_str(Str s);
struct input_stream* is_from_ssl(SSL* ssl, int sock);

struct input_stream* examineFile(const char* path);
struct input_stream* decompress_stream(struct input_stream* s, const char* path);

bool is_isend(struct input_stream* is);
void is_set_unclose(struct input_stream* is, bool unclose);
int is_close(struct input_stream* is);
int is_getc(struct input_stream* is);
int is_undo_getc(struct input_stream* is);
Str is_get_str(struct input_stream* is, bool crnl);
int is_read(struct input_stream* is, char* dst, int bufsize);
int is_file_no(struct input_stream* is);
bool is_save2tmp(struct input_stream* is, const char* tmpf);

struct URLOption {
    const char* referer;
    int flag;
    struct TextList* extra_header;
};

struct FormList;
struct HttpRequest;
struct Buffer;
int doFileSave(struct Url url, struct input_stream* stream,
    const char* defstr, enum CompressionType compression);

enum StreamStatus {
    HTST_UNKNOWN = 255,
    HTST_MISSING = 254,
    HTST_NORMAL = 0,
    HTST_CONNECT = 1,
};

struct UrlStream {
    struct input_stream* stream;
    bool is_cgi;
    const char* url_str;
    struct Url url;
    struct HttpRequest hr;
    enum StreamStatus status;
    const char* ssl_certificate;
    time_t modtime;
};
struct UrlStream openURL(struct Url url, struct Url* current,
    struct FormList* request,
    struct URLOption option,
    struct input_stream* ouf);

void UFhalfclose(struct input_stream* stream, enum UrlScheme scheme);
