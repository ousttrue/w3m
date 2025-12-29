#include "content.h"
#include "func.h"
#include "input_stream.h"
#include "linein.h"
#include "w3m_rc.h"
#include "cookie.h"
#include "message.h"
#include "line.h"
#include "url.h"
#include "etc.h"
#include "indep.h"
#include "myctype.h"
#include <string.h>
#include <libwc/charset.h>

bool matchattr(const char* p, const char* attr, int len, Str* value)
{
    int quoted;
    const char* q = NULL;
    if (strncasecmp(p, attr, len) == 0) {
        p += len;
        p = skip_blanks(p);
        if (value) {
            *value = Strnew();
            if (*p == '=') {
                p++;
                p = skip_blanks(p);
                quoted = 0;
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

const char* checkHeader(struct Content* content, const char* field)
{
    if (field == NULL || !content || content->document_header == NULL)
        return NULL;

    int len = strlen(field);
    for (TextListItem* i = content->document_header->first; i != NULL; i = i->next) {
        if (!strncasecmp(i->ptr, field, len)) {
            char* p = i->ptr + len;
            return remove_space(p);
        }
    }
    return NULL;
}

const char* guess_filename(const char* file)
{
    char *p = NULL, *s;

    if (file != NULL)
        p = mybasename(file);
    if (p == NULL || *p == '\0')
        return DEF_SAVE_FILE;
    s = p;
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

const char* guess_save_name(struct Content* content, const char* path)
{
    if (content && content->document_header) {
        Str name = NULL;
        const char *p, *q;
        if ((p = checkHeader(content, "Content-Disposition:")) != NULL && (q = strcasestr(p, "filename")) != NULL && (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') && matchattr(q, "filename", 8, &name))
            path = name->ptr;
        else if ((p = checkHeader(content, "Content-Type:")) != NULL && (q = strcasestr(p, "name")) != NULL && (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') && matchattr(q, "name", 4, &name))
            path = name->ptr;
    }
    return guess_filename(path);
}

const char* checkContentType(struct Content* content)
{
    const char* p = checkHeader(content, "Content-Type:");
    if (!p) {
        return NULL;
    }

    Str r = Strnew();
    while (*p && *p != ';' && !IS_SPACE(*p))
        Strcat_char(r, *p++);

    if ((p = strcasestr(p, "charset")) != NULL) {
        p += 7;
        p = skip_blanks(p);
        if (*p == '=') {
            p++;
            p = skip_blanks(p);
            if (*p == '"')
                p++;
            content->content_charset = wc_guess_charset(p, 0);
        }
    }

    return r->ptr;
}

#ifdef USE_COOKIE
/* This array should be somewhere else */
/* FIXME: gettextize? */
char* violations[COO_EMAX] = {
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
#endif

void getHttpResponseHeader(struct Content* content, struct Url url,
    struct input_stream* is)
{
    content->document_header = newTextList();
    if (url.scheme == SCM_HTTP || url.scheme == SCM_HTTPS)
        content->http_response_code = -1;
    else
        content->http_response_code = 0;

    // wc_ces charset = WC_CES_US_ASCII;
    Str lineBuf2 = NULL;
    // Lineprop* propBuffer = 0;
    Str tmp;
    while ((tmp = is_get_str(is, true)) && tmp->length) {
        // if (w3m_reqlog) {
        //     FILE* ff;
        //     ff = fopen(w3m_reqlog, "a");
        //     if (ff) {
        //         Strfputs(tmp, ff);
        //         fclose(ff);
        //     }
        // }
        // if (src)
        //     Strfputs(tmp, src);
        cleanup_line(tmp, HEADER_MODE);
        if (tmp->ptr[0] == '\n' || tmp->ptr[0] == '\r' || tmp->ptr[0] == '\0') {
            if (!lineBuf2)
                /* there is no header */
                break;
            /* last header */
        }
        // else if (!(w3m_dump & DUMP_HEAD)) {
        //     if (lineBuf2) {
        //         Strcat(lineBuf2, tmp);
        //     } else {
        //         lineBuf2 = tmp;
        //     }
        //     char c = UFgetc(uf);
        //     UFundogetc(uf);
        //     if (c == ' ' || c == '\t')
        //         /* header line is continued */
        //         continue;
        //
        //     wc_ces mime_charset;
        //     lineBuf2 = decodeMIME(lineBuf2, &mime_charset);
        //     lineBuf2 = convertLine(NULL, lineBuf2, RAW_MODE,
        //         mime_charset ? &mime_charset : &charset,
        //         mime_charset ? mime_charset
        //                      : getRuntime()->DocumentCharset);
        //     /* separated with line and stored */
        //     tmp = Strnew_size(lineBuf2->length);
        //
        //     const char* q;
        //     for (const char* p = lineBuf2->ptr; *p; p = q) {
        //         for (q = p; *q && *q != '\r' && *q != '\n'; q++)
        //             ;
        //         lineBuf2 = checkType(Strnew_charp_n(p, q - p), &propBuffer,
        //             NULL);
        //         Strcat(tmp, lineBuf2);
        //         // if (thru)
        //         //     addnewline(&newBuf->doc, lineBuf2->ptr, propBuffer, NULL,
        //         //         lineBuf2->length, FOLD_BUFFER_WIDTH, -1);
        //         for (; *q && (*q == '\r' || *q == '\n'); q++)
        //             ;
        //     }
        //     // if (thru && getRuntime()->activeImage && getRuntime()->displayImage) {
        //     //     Str src = NULL;
        //     //     if (!strncasecmp(tmp->ptr, "X-Image-URL:", 12)) {
        //     //         const char* tmpf = &tmp->ptr[12];
        //     //         tmpf = skip_blanks(tmpf);
        //     //         src = Strnew_m_charp("<img src=\"", html_quote(tmpf),
        //     //             "\" alt=\"X-Image-URL\">", NULL);
        //     //     }
        //     //     if (src) {
        //     //         struct URLFile f;
        //     //         struct Line* l;
        //     //         wc_ces old_charset = newBuf->document_charset;
        //     //         init_stream(&f, SCM_LOCAL, newStrStream(src));
        //     //         loadHTMLstream(&f, newBuf, NULL, TRUE);
        //     //         UFclose(&f);
        //     //         for (l = newBuf->doc.lastLine; l && l->real_linenumber;
        //     //             l = l->prev)
        //     //             l->real_linenumber = 0;
        //     //         newBuf->document_charset = old_charset;
        //     //     }
        //     // }
        //     lineBuf2 = tmp;
        // }
        else {
            lineBuf2 = tmp;
        }
        if ((url.scheme == SCM_HTTP
                || url.scheme == SCM_HTTPS)
            && content->http_response_code == -1) {
            const char* p = lineBuf2->ptr;
            while (*p && !IS_SPACE(*p))
                p++;
            while (*p && IS_SPACE(*p))
                p++;
            content->http_response_code = atoi(p);
            // if (fmInitialized()) {
            message(lineBuf2->ptr, 0, 0);
            // }
        } else if (!strncasecmp(lineBuf2->ptr, "content-encoding:", 17)) {
            const char* p = lineBuf2->ptr + 17;
            while (IS_SPACE(*p))
                p++;
            content->compression = get_compression(p);
        } else if (getRuntime()->use_cookie && getRuntime()->accept_cookie && check_cookie_accept_domain(url.host) && (!strncasecmp(lineBuf2->ptr, "Set-Cookie:", 11) || !strncasecmp(lineBuf2->ptr, "Set-Cookie2:", 12))) {
            Str name = Strnew(), value = Strnew(), domain = NULL, path = NULL,
                comment = NULL, commentURL = NULL, port = NULL, tmp2;
            int version, quoted, flag = 0;
            time_t expires = (time_t)-1;

            const char* p;
            if (lineBuf2->ptr[10] == '2') {
                p = lineBuf2->ptr + 12;
                version = 1;
            } else {
                p = lineBuf2->ptr + 11;
                version = 0;
            }
            p = skip_blanks(p);
            while (*p != '=' && !IS_ENDT(*p))
                Strcat_char(name, *(p++));
            Strremovetrailingspaces(name);
            if (*p == '=') {
                p++;
                p = skip_blanks(p);
                quoted = 0;
                const char* q = NULL;
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
                p = skip_blanks(p);
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
            if (name->length > 0) {
                int err;
                if (getRuntime()->show_cookie) {
                    if (flag & COO_SECURE)
                        disp_message_nsec("Received a secured cookie", FALSE, 1,
                            TRUE, FALSE);
                    else
                        disp_message_nsec(Sprintf("Received cookie: %s=%s",
                                              name->ptr, value->ptr)
                                              ->ptr,
                            FALSE, 1, TRUE, FALSE);
                }
                err = add_cookie(&url, name, value, expires, domain, path, flag,
                    comment, version, port, commentURL);
                if (err) {
                    char* ans = (getRuntime()->accept_bad_cookie == ACCEPT_BAD_COOKIE_ACCEPT)
                        ? "y"
                        : NULL;
                    if ((err & COO_OVERRIDE_OK) && getRuntime()->accept_bad_cookie == ACCEPT_BAD_COOKIE_ASK) {
                        Str msg = Sprintf("Accept bad cookie from %s for %s?",
                            url.host,
                            ((domain && domain->ptr)
                                    ? domain->ptr
                                    : "<localdomain>"));
                        if (msg->length > TTY_COLS() - 10)
                            Strshrink(msg, msg->length - (TTY_COLS() - 10));
                        Strcat_charp(msg, " (y/n)");
                        ans = inputAnswer(msg->ptr);
                    }
                    if (ans == NULL || TOLOWER(*ans) != 'y' || (err = add_cookie(&url, name, value, expires, domain, path, flag | COO_OVERRIDE, comment, version, port, commentURL))) {
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
                        if (getRuntime()->show_cookie)
                            disp_message_nsec(emsg, FALSE, 1, TRUE, FALSE);
                    } else if (getRuntime()->show_cookie)
                        disp_message_nsec(Sprintf("Accepting invalid cookie: %s=%s",
                                              name->ptr, value->ptr)
                                              ->ptr,
                            FALSE,
                            1, TRUE, FALSE);
                }
            }
        } else if (!strncasecmp(lineBuf2->ptr, "w3m-control:", 12) && url.scheme == SCM_LOCAL_CGI) {
            Str funcname = Strnew();
            int f;

            const char* p = lineBuf2->ptr + 12;
            p = skip_blanks(p);
            while (*p && !IS_SPACE(*p))
                Strcat_char(funcname, *(p++));
            p = skip_blanks(p);
            f = getFuncList(funcname->ptr);
            if (f >= 0) {
                tmp = Strnew_charp(p);
                Strchop(tmp);
                pushEvent(f, tmp->ptr);
            }
        }
        pushText(content->document_header, lineBuf2->ptr);
        Strfree(lineBuf2);
        lineBuf2 = NULL;
    }
    // if (thru)
    //     addnewline(&newBuf->doc, "", propBuffer, NULL, 0, -1, -1);
    // if (src)
    //     fclose(src);
}
