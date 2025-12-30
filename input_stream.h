#pragma once
#include "url.h"
#include "http_request.h"
#include "ssl_stream.h"
#include "compression.h"
#include <stdbool.h>
#include <stdio.h>

enum InputStreamType {
    IST_BASIC = 0,
    IST_FILE = 1,
    IST_STR = 2,
    IST_SSL = 3,
};

struct stream_buffer {
    uint8_t* buf;
    int size;
    int cur;
    int next;
};

inline static bool MUST_BE_UPDATED(struct stream_buffer* sb)
{
    return (sb->cur == sb->next);
}

typedef int (*FileCloseFunc)(FILE* stream);
struct io_file_handle {
    FILE* f;
    FileCloseFunc close;
};

struct input_stream {
    enum InputStreamType type;
    bool iseos;
    bool unclose;
    struct stream_buffer sb;
    union {
        int base;
        struct io_file_handle file;
        struct ssl_handle ssl;
    };
};

struct input_stream* is_from_fd(int fd);
struct input_stream* is_from_file(FILE* f, FileCloseFunc closep);
struct input_stream* is_from_str(Str s);
struct input_stream* is_from_ssl(SSL* ssl, int sock);

struct input_stream* examineFile(const char* path);
struct input_stream* decompress_stream(struct input_stream* s, const char* path);

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
struct UrlStream openURL(const char* url, struct Url* current,
    struct FormList* request,
    struct URLOption option,
    struct input_stream* ouf);

void UFhalfclose(struct input_stream* stream, enum UrlScheme scheme);
