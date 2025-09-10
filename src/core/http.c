#include "http.h"
#include "url.h"
#include "cookie.h"
#include "html_form.h"
#include "istream.h"
#include "convertline.h"
#include "quote.h"
#include "mailcap.h"
#include "mimehead.h"
#include "buffer_loader.h"
#include "ui.h"
#include <myctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

int accept_cookie = true;
int show_cookie = false;
enum AcceptBadCookieMode accept_bad_cookie = (ACCEPT_BAD_COOKIE_DISCARD);

bool matchattr(const char* p, const char* attr, int len, Str* value)
{
    const char* q = NULL;
    if (strncasecmp(p, attr, len) == 0) {
        p += len;
        SKIP_BLANKS(p);
        if (value) {
            *value = Strnew();
            if (*p == '=') {
                p++;
                SKIP_BLANKS(p);
                int quoted = 0;
                while (!IS_ENDL(*p) && (quoted || *p != ';')) {
                    if (!IS_SPACE(*p))
                        q = p;
                    if (*p == '"')
                        quoted = (quoted) ? 0 : 1;
                    else
                        Strcat_char(*value, *p);
                    p++;
                }
                if (q)
                    Strshrink(*value, p - q - 1);
            }
            return 1;
        } else {
            if (IS_ENDT(*p)) {
                return 1;
            }
        }
    }
    return 0;
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

const char* mybasename(const char* s)
{
    const char* p = s;
    while (*p)
        p++;
    while (s <= p && *p != '/')
        p--;
    if (*p == '/')
        p++;
    else
        p = s;
    return allocStr(p, -1);
}

#define DEF_SAVE_FILE "index.html"

const char* guessFileName(const char* file)
{
    char* p;
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
        Str name = NULL;
        const char *p, *q;
        if ((p = getHttpHeaderValue(document_header, "Content-Disposition:")) != NULL && (q = strcasestr(p, "filename")) != NULL && (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') && matchattr(q, "filename", 8, &name))
            path = name->ptr;
        else if ((p = getHttpHeaderValue(document_header, "Content-Type:")) != NULL && (q = strcasestr(p, "name")) != NULL && (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') && matchattr(q, "name", 4, &name))
            path = name->ptr;
    }
    return guessFileName(path);
}

bool is_text_type(const char* type)
{
    return (type == NULL || type[0] == '\0' || strncasecmp(type, "text/", 5) == 0 || (strncasecmp(type, "application/", 12) == 0 && strstr(type, "xhtml") != NULL) || strncasecmp(type, "message/", sizeof("message/") - 1) == 0);
}

bool is_plain_text_type(const char* type)
{
    return ((type && strcasecmp(type, "text/plain") == 0) || (is_text_type(type) && !is_dump_text_type(type)));
}
bool is_html_type(const char* type)
{
    return (type && (strcasecmp(type, "text/html") == 0 || strcasecmp(type, "application/xhtml+xml") == 0));
}
