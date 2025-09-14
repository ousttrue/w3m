#pragma once
#include "url.h"
#include "textlist.h"
#include "UserInteraction.h"
#include "ContentType.h"
#include <wc.h>

extern char* index_file;
extern char LocalhostOnly;
extern int retryAsHttp;

struct Content {
    struct Url url;
    TextList* document_header;
    const char* ssl_certificate;

    Str page;
    const char* sourcefile;
    struct ContentTypeCharset cc;
};

struct Form;
struct Content loadGeneralFile(const char* path, struct Url* current, struct Form* post,
    const char* referer, struct UserInteraction ui);

bool is_plain_text_type(const char* type);

const char* last_modified(struct Content* content);
