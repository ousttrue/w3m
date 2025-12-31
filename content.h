#pragma once
#include "http_request.h"
#include "url.h"
#include "textlist.h"
#include "compression.h"
#include "Str.h"
#include <stdbool.h>
#include <libwc/ces.h>
#include <time.h>

enum StreamStatus {
    HTST_UNKNOWN = 255,
    HTST_MISSING = 254,
    HTST_NORMAL = 0,
    HTST_CONNECT = 1,
};

struct Content {
    bool is_cgi;
    const char* url_str;
    struct Url url;
    const char* filename;
    /// download file cache
    const char* sourcefile;
    const char* header_source;
    const char* ssl_certificate;

    struct mailcap* mailcap;
    const char* mailcap_source;

    struct HttpRequest hr;
    int http_response_code;
    struct TextList* document_header;

    const char* content_type;
    size_t current_content_length;
    enum wc_ces charset;
    enum CompressionType compression;

    time_t modtime;
};

struct Url;
struct input_stream;
void getHttpResponseHeader(struct Content* content, struct Url url,
    struct input_stream* is);

bool matchattr(const char* p, const char* attr, int len, Str* value);
const char* checkHeader(struct Content* content, const char* field);
const char* checkContentType(struct Content* content);
const char* guess_filename(const char* file);
const char* guess_save_name(struct Content* content, const char* file);
