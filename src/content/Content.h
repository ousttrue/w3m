#pragma once
#include "url.h"
#include "textlist.h"
#include "geometry.h"
#include "ContentType.h"
#include <wc.h>

extern char* index_file;
extern char LocalhostOnly;
extern int retryAsHttp;
extern int UseDictCommand;
extern char* DictCommand;

struct Content {
    struct Url url;
    TextList* document_header;
    const char* ssl_certificate;

    Str page;
    const char* sourcefile;
    struct ContentTypeCharset cc;
};

inline static struct Content emptyContent()
{
    return (struct Content) {};
}

inline static struct Content makeContentFromHtmlUtf8(Str html)
{
    return (struct Content) {
        .page = html,
        .cc = {
            .content_type = CONTENTTYPE_TEXT_HTML,
            .charset = WC_CES_UTF_8,
        },
    };
};

struct Form;
struct Content getContent(struct UI ui, const char* path, struct Url* current,
    struct Form* post, const char* referer);

bool is_plain_text_type(const char* type);

const char* last_modified(struct Content* content);

struct Content execdict(struct UI ui, const char* word);
