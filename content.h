#pragma once
#include "url.h"
#include "textlist.h"
#include "compression.h"
#include "Str.h"
#include <stdbool.h>
#include <libwc/ces.h>

struct Content {
    struct Url url;
    const char* filename;
    /// download file cache
    const char* sourcefile;

    const char* mailcap_source;
    const char* header_source;
    const char* ssl_certificate;

    int http_response_code;
    struct TextList* document_header;
    enum wc_ces charset;
    size_t current_content_length;
    enum CompressionType compression;
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
