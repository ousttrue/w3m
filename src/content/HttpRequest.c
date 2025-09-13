#include "HttpRequest.h"
#include "runtime.h"
#include "cookie.h"
#include "html_form.h"
#include <stdlib.h>
#include <strings.h>

bool override_user_agent = false;
const char* UserAgent = NULL;
char* AcceptMedia = NULL;
char* AcceptEncoding = NULL;
char* AcceptLang = NULL;
char NoCache = false;
int NoSendReferer = true;
int CrossOriginReferer = true;
int override_content_type = false;

Str getHttpRequestURIStr(struct HttpRequest* hr)
{
    Str tmp = Strnew();
    if (hr->method == HTTP_METHOD_CONNECT) {
        Strcat_charp(tmp, hr->url.host);
        Strcat(tmp, Sprintf(":%d", hr->url.port));
    } else if (hr->flag & HR_FLAG_LOCAL) {
        Strcat_charp(tmp, hr->url.file);
        if (hr->url.query) {
            Strcat_char(tmp, '?');
            Strcat_charp(tmp, hr->url.query);
        }
    } else {
        Strcat(tmp, _parsedURL2Str(&hr->url, true, true, false));
    }
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
    if (/*target->is_nocache ||*/ NoCache) {
        Strcat_charp(s, "Pragma: no-cache\r\n");
        Strcat_charp(s, "Cache-control: no-cache\r\n");
    }
    no_referer = NoSendReferer;
    no_referer = no_referer || (no_referer_ptr && *no_referer_ptr);
    no_referer = no_referer || (no_referer_ptr && *no_referer_ptr);
    if (!no_referer) {
        abort();
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

Str getHttpRequestStr(struct HttpRequest* hr, struct Url* current, TextList* extra)
{
    Str tmp = Strnew_charp(httpRequestMethodStr(hr->method));
    Strcat_charp(tmp, " ");
    Strcat_charp(tmp, getHttpRequestURIStr(hr)->ptr);
    Strcat_charp(tmp, " HTTP/1.0\r\n");
    if (hr->referer == NO_REFERER)
        Strcat_charp(tmp, otherinfo(&hr->url, NULL, NULL));
    else
        Strcat_charp(tmp, otherinfo(&hr->url, current, hr->referer));

    if (extra) {
        TextListItem* i;
        for (i = extra->first; i != NULL; i = i->next) {
            if (strncasecmp(i->ptr, "Authorization:",
                    sizeof("Authorization:") - 1)
                == 0) {
                if (hr->method == HTTP_METHOD_CONNECT)
                    continue;
            }
            if (strncasecmp(i->ptr, "Proxy-Authorization:",
                    sizeof("Proxy-Authorization:") - 1)
                == 0) {
                if (hr->url.scheme == SCM_HTTPS
                    && hr->method != HTTP_METHOD_CONNECT)
                    continue;
            }
            Strcat_charp(tmp, i->ptr);
        }
    }

    Str cookie;
    if (hr->method != HTTP_METHOD_CONNECT && use_cookie && (cookie = find_cookie(&hr->url))) {
        Strcat_charp(tmp, "Cookie: ");
        Strcat(tmp, cookie);
        Strcat_charp(tmp, "\r\n");
        /* [DRAFT 12] s. 10.1 */
        if (cookie->ptr[0] != '$')
            Strcat_charp(tmp, "Cookie2: $Version=\"1\"\r\n");
    }
    if (hr->method == HTTP_METHOD_POST) {
        if (hr->post->enctype == FORM_ENCTYPE_MULTIPART) {
            Strcat_charp(tmp, "Content-Type: multipart/form-data; boundary=");
            Strcat_charp(tmp, hr->post->boundary);
            Strcat_charp(tmp, "\r\n");
            Strcat(tmp, Sprintf("Content-Length: %ld\r\n", hr->post->length));
            Strcat_charp(tmp, "\r\n");
        } else {
            if (!override_content_type) {
                Strcat_charp(tmp,
                    "Content-Type: application/x-www-form-urlencoded\r\n");
            }
            Strcat(tmp,
                Sprintf("Content-Length: %ld\r\n", hr->post->length));
            // if (header_string)
            //     Strcat(tmp, header_string);
            Strcat_charp(tmp, "\r\n");
            Strcat_charp_n(tmp, hr->post->body, hr->post->length);
            Strcat_charp(tmp, "\r\n");
        }
    } else {
        // if (header_string)
        //     Strcat(tmp, header_string);
        Strcat_charp(tmp, "\r\n");
    }
    return tmp;
}
