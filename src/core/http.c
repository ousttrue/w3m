#include "http.h"
#include "url.h"
#include "cookie.h"
#include "version.h"
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

int override_user_agent = false;
char* UserAgent = NULL;
char* AcceptMedia = NULL;
char* AcceptEncoding = NULL;
char* AcceptLang = NULL;
char NoCache = false;
int NoSendReferer = false;
int CrossOriginReferer = true;
int use_cookie = true;
int override_content_type = false;
int accept_cookie = true;
int show_cookie = false;
enum AcceptBadCookieMode accept_bad_cookie = (ACCEPT_BAD_COOKIE_DISCARD);

Str getHttpRequestMethodStr(struct HttpRequest* hr)
{
    switch (hr->command) {
    case HR_COMMAND_CONNECT:
        return Strnew_charp("CONNECT");
    case HR_COMMAND_POST:
        return Strnew_charp("POST");
        break;
    case HR_COMMAND_HEAD:
        return Strnew_charp("HEAD");
        break;
    case HR_COMMAND_GET:
    default:
        return Strnew_charp("GET");
    }
    return NULL;
}

Str getHttpRequestURIStr(struct Url* pu, struct HttpRequest* hr)
{
    Str tmp = Strnew();
    if (hr->command == HR_COMMAND_CONNECT) {
        Strcat_charp(tmp, pu->host);
        Strcat(tmp, Sprintf(":%d", pu->port));
    } else if (hr->flag & HR_FLAG_LOCAL) {
        Strcat_charp(tmp, pu->file);
        if (pu->query) {
            Strcat_char(tmp, '?');
            Strcat_charp(tmp, pu->query);
        }
    } else
        Strcat(tmp, _parsedURL2Str(pu, true, true, false));
    return tmp;
}

static Str
parsedURL2RefererOriginStr(struct Url* pu)
{
    const char* f = pu->file;
    const char* q = pu->query;
    pu->file = NULL;
    pu->query = NULL;
    Str s = _parsedURL2Str(pu, false, false, false);
    pu->file = f;
    pu->query = q;
    return s;
}

static char*
otherinfo(struct Url* target, struct Url* current, const char* referer)
{
    Str s = Strnew();
    const int* no_referer_ptr;
    int no_referer;

    if (!override_user_agent) {
        Strcat_charp(s, "User-Agent: ");
        if (UserAgent == NULL || *UserAgent == '\0')
            Strcat_charp(s, w3m_version);
        else
            Strcat_charp(s, UserAgent);
        Strcat_charp(s, "\r\n");
    }

    Strcat_m_charp(s, "Accept: ", AcceptMedia, "\r\n", NULL);
    Strcat_m_charp(s, "Accept-Encoding: ", AcceptEncoding, "\r\n", NULL);
    Strcat_m_charp(s, "Accept-Language: ", AcceptLang, "\r\n", NULL);

    if (target->host) {
        Strcat_charp(s, "Host: ");
        Strcat_charp(s, target->host);
        if (target->port != getSchemeInfo(target->scheme).port)
            Strcat(s, Sprintf(":%d", target->port));
        Strcat_charp(s, "\r\n");
    }
    if (target->is_nocache || NoCache) {
        Strcat_charp(s, "Pragma: no-cache\r\n");
        Strcat_charp(s, "Cache-control: no-cache\r\n");
    }
    no_referer = NoSendReferer;
    no_referer = no_referer || (no_referer_ptr && *no_referer_ptr);
    no_referer = no_referer || (no_referer_ptr && *no_referer_ptr);
    if (!no_referer) {
        int cross_origin = false;
        if (CrossOriginReferer && current && current->host && (!target || !target->host || strcasecmp(current->host, target->host) != 0 || current->port != target->port || current->scheme != target->scheme))
            cross_origin = true;
        if (current && current->scheme == SCM_HTTPS && target->scheme != SCM_HTTPS) {
            /* Don't send Referer: if https:// -> http:// */
        } else if (referer == NULL && current && current->scheme != SCM_LOCAL && current->scheme != SCM_LOCAL_CGI && current->scheme != SCM_DATA && (current->scheme != SCM_FTP || (current->user == NULL && current->pass == NULL))) {
            Strcat_charp(s, "Referer: ");
            if (cross_origin)
                Strcat(s, parsedURL2RefererOriginStr(current));
            else
                Strcat(s, parsedURL2RefererStr(current));
            Strcat_charp(s, "\r\n");
        } else if (referer != NULL && referer != NO_REFERER) {
            Strcat_charp(s, "Referer: ");
            if (cross_origin)
                Strcat(s, parsedURL2RefererOriginStr(current));
            else
                Strcat_charp(s, referer);
            Strcat_charp(s, "\r\n");
        }
    }
    return s->ptr;
}

Str getHttpRequestStr(struct Url* pu, struct Url* current, struct HttpRequest* hr, TextList* extra)
{
    Str tmp = getHttpRequestMethodStr(hr);
    Strcat_charp(tmp, " ");
    Strcat_charp(tmp, getHttpRequestURIStr(pu, hr)->ptr);
    Strcat_charp(tmp, " HTTP/1.0\r\n");
    if (hr->referer == NO_REFERER)
        Strcat_charp(tmp, otherinfo(pu, NULL, NULL));
    else
        Strcat_charp(tmp, otherinfo(pu, current, hr->referer));
    TextListItem* i;
    if (extra != NULL)
        for (i = extra->first; i != NULL; i = i->next) {
            if (strncasecmp(i->ptr, "Authorization:",
                    sizeof("Authorization:") - 1)
                == 0) {
                if (hr->command == HR_COMMAND_CONNECT)
                    continue;
            }
            if (strncasecmp(i->ptr, "Proxy-Authorization:",
                    sizeof("Proxy-Authorization:") - 1)
                == 0) {
                if (pu->scheme == SCM_HTTPS
                    && hr->command != HR_COMMAND_CONNECT)
                    continue;
            }
            Strcat_charp(tmp, i->ptr);
        }

    Str cookie;
    if (hr->command != HR_COMMAND_CONNECT && use_cookie && (cookie = find_cookie(pu))) {
        Strcat_charp(tmp, "Cookie: ");
        Strcat(tmp, cookie);
        Strcat_charp(tmp, "\r\n");
        /* [DRAFT 12] s. 10.1 */
        if (cookie->ptr[0] != '$')
            Strcat_charp(tmp, "Cookie2: $Version=\"1\"\r\n");
    }
    if (hr->command == HR_COMMAND_POST) {
        if (hr->request->enctype == FORM_ENCTYPE_MULTIPART) {
            Strcat_charp(tmp, "Content-Type: multipart/form-data; boundary=");
            Strcat_charp(tmp, hr->request->boundary);
            Strcat_charp(tmp, "\r\n");
            Strcat(tmp,
                Sprintf("Content-Length: %ld\r\n", hr->request->length));
            Strcat_charp(tmp, "\r\n");
        } else {
            if (!override_content_type) {
                Strcat_charp(tmp,
                    "Content-Type: application/x-www-form-urlencoded\r\n");
            }
            Strcat(tmp,
                Sprintf("Content-Length: %ld\r\n", hr->request->length));
            // if (header_string)
            //     Strcat(tmp, header_string);
            Strcat_charp(tmp, "\r\n");
            Strcat_charp_n(tmp, hr->request->body, hr->request->length);
            Strcat_charp(tmp, "\r\n");
        }
    } else {
        // if (header_string)
        //     Strcat(tmp, header_string);
        Strcat_charp(tmp, "\r\n");
    }
#ifdef DEBUG
    fprintf(stderr, "HTTPrequest: [ %s ]\n\n", tmp->ptr);
#endif /* DEBUG */
    return tmp;
}

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
