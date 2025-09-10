#include "http_message.h"
#include "myctype.h"
#include "quote.h"
#include "str_util.h"
#include <string.h>
#include <strings.h>

struct CharSlice makeSlice(const char* p)
{
    return (struct CharSlice) {
        p,
        strlen(p),
    };
}

bool startswith(const char* p, struct CharSlice slice)
{
    return strncasecmp(p, slice.p, slice.len) == 0;
}

const char* getHttpHeaderValue(TextList* document_header, const char* field)
{
    if (!field) {
        return NULL;
    }
    if (!document_header) {
        return NULL;
    }

    int len = strlen(field);
    for (TextListItem* i = document_header->first; i != NULL; i = i->next) {
        if (!strncasecmp(i->ptr, field, len)) {
            char* p = i->ptr + len;
            return remove_space(p);
        }
    }
    return NULL;
}

struct ContentTypeCharset getContentType(TextList* document_header)
{
    const char* p = getHttpHeaderValue(document_header, "Content-Type:");
    if (!p) {
        return (struct ContentTypeCharset) { 0 };
    }
    Str r = Strnew();
    while (*p && *p != ';' && !IS_SPACE(*p))
        Strcat_char(r, *p++);

    struct ContentTypeCharset content_type_charset = {
        .content_type = r->ptr,
        .charset = 0
    };

    if ((p = strcasestr(p, "charset")) != NULL) {
        p += 7;
        SKIP_BLANKS(p);
        if (*p == '=') {
            p++;
            SKIP_BLANKS(p);
            if (*p == '"')
                p++;
            content_type_charset.charset = wc_guess_charset(p, 0);
        }
    }
    return content_type_charset;
}

#define DEF_SAVE_FILE "index.html"

const char* guessFileName(const char* file)
{
    char* p = 0;
    if (file)
        p = allocStr(mybasename(file), -1);
    if (!p || *p == '\0')
        return DEF_SAVE_FILE;
    const char* s = p;
    if (*p == '#')
        p++;
    while (*p != '\0') {
        if ((*p == '#' && *(p + 1) != '\0') || *p == '?') {
            *p = '\0';
            break;
        }
        p++;
    }
    return s;
}

const char* guessSaveName(TextList* document_header, const char* path)
{
    if (document_header) {
        struct CharSlice name;
        const char *p, *q;
        if ((p = getHttpHeaderValue(document_header, "Content-Disposition:")) != NULL && (q = strcasestr(p, "filename")) != NULL && (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') && matchattr(q, (struct CharSlice) { "filename", 8 }, &name))
            path = Strnew_charp_n(name.p, name.len)->ptr;
        else if ((p = getHttpHeaderValue(document_header, "Content-Type:")) != NULL && (q = strcasestr(p, "name")) != NULL && (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') && matchattr(q, (struct CharSlice){ "name", 4 }, &name))
            path = Strnew_charp_n(name.p, name.len)->ptr;
    }
    return guessFileName(path);
}


struct CharSlice extractSemiColon(const char* p, struct CharSlice attr)
{
    if (!startswith(p, attr)) {
        // not found
        return (struct CharSlice) { 0, 0 };
    }

    p += attr.len;
    SKIP_BLANKS(p);

    if (IS_ENDT(*p)) {
        // found and empty value
        return (struct CharSlice) { p, 0 };
    }

    if (*p != '=') {
        // not found
        return (struct CharSlice) { 0, 0 };
    }
    p++;
    SKIP_BLANKS(p);

    if (*p == '"') {
        // quoted. search "
        p++;
        struct CharSlice slice = {
            .p = p,
            .len = 0,
        };
        for (; !IS_ENDL(*p) && *p != '"'; ++p) {
            ++slice.len;
        }
        return slice;
    } else {
        // not quoted search ;
        struct CharSlice slice = {
            .p = p,
            .len = 0,
        };
        for (; !IS_ENDL(*p) && *p != ';'; ++p) {
            ++slice.len;
        }
        return slice;
    }
}
