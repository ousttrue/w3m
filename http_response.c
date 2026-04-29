#include "http_response.h"
#include "filepath.h"
#include "input_stream.h"
#include "main.h"
#include "display.h"
#include "screen.h"
#include "cookie.h"
#include "etc.h"
#include "global.h"
#include "UrlFile.h"
#include "myctype.h"
#include "line_input.h"
#include "indep.h"

#include <libwc/charset.h>

const char* violations[COO_EMAX] = {
    "internal error",
    "tail match failed",
    "wrong number of dots",
    "RFC 2109 4.3.2 rule 1",
    "RFC 2109 4.3.2 rule 2.1",
    "RFC 2109 4.3.2 rule 2.2",
    "RFC 2109 4.3.2 rule 3",
    "RFC 2109 4.3.2 rule 4",
    "RFC XXXX 4.3.2 rule 5"
};

struct HttpResponse http_response_header(struct InputStream* stream, enum UrlScheme scheme)
{
    struct HttpResponse res = {
        .status_code = 0,
        .headers = newTextList(),
    };

    if (scheme == SCM_HTTP
        || scheme == SCM_HTTPS) {
        res.status_code = -1;

        struct str_view gv = ist_gets(stream, true);
        const char* p = gv.ptr;
        while (*p && !IS_SPACE(*p))
            p++;
        while (*p && IS_SPACE(*p))
            p++;
        res.status_code = atoi(p);
        if (fmInitialized) {
            message(gv.ptr, 0, 0);
            tty_write_sc();
        }
    }

    while (true) {
        struct str_view gv = ist_gets(stream, true);
        if (gv.len == 0) {
            break;
        }
        pushText(res.headers, gv.ptr);
    }

    return res;
}

const char* http_response_save_header_source(struct HttpResponse* res)
{
    FILE* thru_src = NULL;
    const char* tmpf = NULL;
    if (!image_source) {
        tmpf = tmpfname(TMPF_DFL, NULL);
        thru_src = fopen(tmpf, "w");
        // if (thru_src)
        //     newBuf->header_source = tmpf;
    }
    for (TextListItem* ti = res->headers->first; ti; ti = ti->next) {
        Str tmp = Strnew_charp(ti->ptr);
        if (thru_src)
            Strfputs(tmp, thru_src);
    }
    if (thru_src)
        fclose(thru_src);
    return tmpf;
}

// uf->content_encoding = uf->compression;
enum ContentCompression http_response_process(struct HttpResponse* res, struct CmdArgs* args, struct Url* pu)
{
    enum ContentCompression compression = CMP_NOCOMPRESS;
    Str lineBuf2 = NULL;
    for (TextListItem* ti = res->headers->first; ti; ti = ti->next) {
        Str tmp = cleanup_line(ti->ptr, strlen(ti->ptr), HEADER_MODE);
        if (tmp->ptr[0] == '\n' || tmp->ptr[0] == '\r' || tmp->ptr[0] == '\0') {
            if (!lineBuf2)
                /* there is no header */
                break;
            /* last header */
        } else {
            lineBuf2 = tmp;
        }

        if (!strncasecmp(lineBuf2->ptr, "content-encoding:", 17)) {
            const char* p = lineBuf2->ptr + 17;
            while (IS_SPACE(*p))
                p++;

            struct CompressionDecoder* d = compression_from_encodings(p);
            if (d) {
                compression = d->type;
            }
        } else if (use_cookie && accept_cookie && pu && check_cookie_accept_domain(pu->host) && (!strncasecmp(lineBuf2->ptr, "Set-Cookie:", 11) || !strncasecmp(lineBuf2->ptr, "Set-Cookie2:", 12))) {
            Str name = Strnew(), value = Strnew(), domain = NULL, path = NULL,
                comment = NULL, commentURL = NULL, port = NULL, tmp2;
            int version, quoted, flag = 0;
            time_t expires = (time_t)-1;

            const char* q = NULL;
            const char* p;
            if (lineBuf2->ptr[10] == '2') {
                p = lineBuf2->ptr + 12;
                version = 1;
            } else {
                p = lineBuf2->ptr + 11;
                version = 0;
            }
            SKIP_BLANKS(p);
            while (*p != '=' && !IS_ENDT(*p))
                Strcat_char(name, *(p++));
            Strremovetrailingspaces(name);
            if (*p == '=') {
                p++;
                SKIP_BLANKS(p);
                quoted = 0;
                while (!IS_ENDL(*p) && (quoted || *p != ';')) {
                    if (!IS_SPACE(*p))
                        q = p;
                    if (*p == '"')
                        quoted = (quoted) ? 0 : 1;
                    Strcat_char(value, *(p++));
                }
                if (q)
                    Strshrink(value, p - q - 1);
            }
            while (*p == ';') {
                p++;
                SKIP_BLANKS(p);
                if (matchattr(p, "expires", 7, &tmp2)) {
                    /* version 0 */
                    expires = mymktime(tmp2->ptr);
                } else if (matchattr(p, "max-age", 7, &tmp2)) {
                    /* XXX Is there any problem with max-age=0? (RFC 2109 ss. 4.2.1, 4.2.2 */
                    expires = time(NULL) + atol(tmp2->ptr);
                } else if (matchattr(p, "domain", 6, &tmp2)) {
                    domain = tmp2;
                } else if (matchattr(p, "path", 4, &tmp2)) {
                    path = tmp2;
                } else if (matchattr(p, "secure", 6, NULL)) {
                    flag |= COO_SECURE;
                } else if (matchattr(p, "comment", 7, &tmp2)) {
                    comment = tmp2;
                } else if (matchattr(p, "version", 7, &tmp2)) {
                    version = atoi(tmp2->ptr);
                } else if (matchattr(p, "port", 4, &tmp2)) {
                    /* version 1, Set-Cookie2 */
                    port = tmp2;
                } else if (matchattr(p, "commentURL", 10, &tmp2)) {
                    /* version 1, Set-Cookie2 */
                    commentURL = tmp2;
                } else if (matchattr(p, "discard", 7, NULL)) {
                    /* version 1, Set-Cookie2 */
                    flag |= COO_DISCARD;
                }
                quoted = 0;
                while (!IS_ENDL(*p) && (quoted || *p != ';')) {
                    if (*p == '"')
                        quoted = (quoted) ? 0 : 1;
                    p++;
                }
            }
            if (pu && name->length > 0) {
                int err;
                if (show_cookie) {
                    if (flag & COO_SECURE)
                        disp_message_nsec(args, "Received a secured cookie", FALSE, 1,
                            TRUE, false);
                    else
                        disp_message_nsec(args, Sprintf("Received cookie: %s=%s", name->ptr, value->ptr)->ptr,
                            FALSE, 1, TRUE, FALSE);
                }
                err = add_cookie(pu, name, value, expires, domain, path, flag,
                    comment, version, port, commentURL);
                if (err) {
                    const char* ans = (accept_bad_cookie == ACCEPT_BAD_COOKIE_ACCEPT)
                        ? "y"
                        : NULL;
                    if (fmInitialized && (err & COO_OVERRIDE_OK) && accept_bad_cookie == ACCEPT_BAD_COOKIE_ASK) {
                        Str msg = Sprintf("Accept bad cookie from %s for %s?",
                            pu->host,
                            ((domain && domain->ptr)
                                    ? domain->ptr
                                    : "<localdomain>"));
                        if (msg->length > COLS - 10)
                            Strshrink(msg, msg->length - (COLS - 10));
                        Strcat_charp(msg, " (y/n)");
                        ans = inputAnswer(args, msg->ptr);
                    }
                    if (ans == NULL || TOLOWER(*ans) != 'y' || (err = add_cookie(pu, name, value, expires, domain, path, flag | COO_OVERRIDE, comment, version, port, commentURL))) {
                        err = (err & ~COO_OVERRIDE_OK) - 1;
                        const char* emsg;
                        if (err >= 0 && err < COO_EMAX)
                            emsg = Sprintf("This cookie was rejected "
                                           "to prevent security violation. [%s]",
                                violations[err])
                                       ->ptr;
                        else
                            emsg = "This cookie was rejected to prevent security violation.";
                        record_err_message(emsg);
                        if (show_cookie)
                            disp_message_nsec(args, emsg, FALSE, 1, TRUE, FALSE);
                    } else if (show_cookie)
                        disp_message_nsec(args, Sprintf("Accepting invalid cookie: %s=%s", name->ptr, value->ptr)->ptr,
                            FALSE,
                            1, TRUE, FALSE);
                }
            }
        } else if (!strncasecmp(lineBuf2->ptr, "w3m-control:", 12) && pu->scheme == SCM_LOCAL_CGI) {
            Str funcname = Strnew();

            const char* p = lineBuf2->ptr + 12;
            SKIP_BLANKS(p);
            while (*p && !IS_SPACE(*p))
                Strcat_char(funcname, *(p++));
            SKIP_BLANKS(p);
            if (funcname->length) {
                tmp = Strnew_charp(p);
                Strchop(tmp);
                pushEvent(funcname->ptr, tmp->ptr);
            }
        }
        Strfree(lineBuf2);
        lineBuf2 = NULL;
    }
    return compression;
}

bool matchattr(const char* p, const char* attr, int len, Str* value)
{
    if (strncasecmp(p, attr, len) == 0) {
        p += len;
        SKIP_BLANKS(p);
        if (value) {
            *value = Strnew();
            if (*p == '=') {
                p++;
                SKIP_BLANKS(p);
                bool quoted = false;
                const char* q = NULL;
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

const char* http_response_get(struct HttpResponse* res, const char* field)
{
    if (res && res->headers && field) {
        int len = strlen(field);
        for (TextListItem* ti = res->headers->first; ti; ti = ti->next) {
            if (!strncasecmp(ti->ptr, field, len)) {
                const char* p = ti->ptr + len;
                return remove_space(p);
            }
        }
    }
    return NULL;
}

const char* http_response_get_content_type(struct HttpResponse* res, wc_ces* content_charset)
{
    const char* p = http_response_get(res, "Content-Type:");
    if (p == NULL)
        return NULL;

    Str r = Strnew();
    while (*p && *p != ';' && !IS_SPACE(*p))
        Strcat_char(r, *p++);

    if ((p = strcasestr(p, "charset")) != NULL) {
        p += 7;
        SKIP_BLANKS(p);
        if (*p == '=') {
            p++;
            SKIP_BLANKS(p);
            if (*p == '"')
                p++;
            if (content_charset) {
                *content_charset = wc_guess_charset(p, 0);
            }
        }
    }

    return r->ptr;
}

const char* http_response_guess_save_name(struct HttpResponse* res, const char* path)
{
    if (res) {
        Str name = NULL;
        const char* p = http_response_get(res, "Content-Disposition:");
        const char* q;
        if (p != NULL && (q = strcasestr(p, "filename")) != NULL && (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') && matchattr(q, "filename", 8, &name))
            path = name->ptr;
        else if ((p = http_response_get(res, "Content-Type:")) != NULL && (q = strcasestr(p, "name")) != NULL && (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') && matchattr(q, "name", 4, &name))
            path = name->ptr;
    }
    return alloc_guess_filename(path);
}
