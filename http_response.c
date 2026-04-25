#include "http_response.h"
#include "html_loader.h"
#include "main.h"
#include "display.h"
#include "terms.h"
#include "cookie.h"
#include "etc.h"
#include <w3m.h>
#include <w3m/growbuf.h>
#include "input_stream.h"
#include "global.h"
#include "Str.h"
#include "mimehead.h"
#include "textlist.h"
#include "line.h"
#include "buffer.h"
#include "UrlFile.h"
#include "myctype.h"
#include "indep.h"
#include "line_input.h"

#include "wc_util.h"
#include <libwc/wc_types.h>
#include <libwc/ces.h>

int http_response_code;

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

void readHeader(struct CmdArgs* args, struct URLFile* uf, struct Buffer* newBuf, int thru, struct Url* pu)
{
    char *p, *q;
    char* emsg;
    char c;
    Str lineBuf2 = NULL;
    TextList* headerlist;
    wc_ces charset = WC_CES_US_ASCII, mime_charset;
    FILE* src = NULL;
    Lineprop* propBuffer;

    headerlist = newBuf->document_header = newTextList();
    if (uf->scheme == SCM_HTTP
        || uf->scheme == SCM_HTTPS)
        http_response_code = -1;
    else
        http_response_code = 0;

    if (thru && !newBuf->header_source
        && !image_source) {
        const char* tmpf = tmpfname(TMPF_DFL, NULL);
        src = fopen(tmpf, "w");
        if (src)
            newBuf->header_source = tmpf;
    }

    struct growbuf* gb = growbuf_create();
    while (true) {
        growbuf_clear(gb);
        ist_gets_to_growbuf(uf->stream, gb, true);
        struct str_view gv = growbuf_str_view(gb);
        if (gv.len == 0) {
            break;
        }
        Str tmp = Strnew_charp_n(gv.ptr, gv.len);
        if (uf->scheme == SCM_NEWS && tmp->ptr[0] == '.')
            Strshrinkfirst(tmp, 1);
        if (w3m_reqlog) {
            FILE* ff;
            ff = fopen(w3m_reqlog, "a");
            if (ff) {
                Strfputs(tmp, ff);
                fclose(ff);
            }
        }
        if (src)
            Strfputs(tmp, src);
        cleanup_line(tmp, HEADER_MODE);
        if (tmp->ptr[0] == '\n' || tmp->ptr[0] == '\r' || tmp->ptr[0] == '\0') {
            if (!lineBuf2)
                /* there is no header */
                break;
            /* last header */
        }
        else {
            lineBuf2 = tmp;
        }
        if ((uf->scheme == SCM_HTTP
                || uf->scheme == SCM_HTTPS)
            && http_response_code == -1) {
            p = lineBuf2->ptr;
            while (*p && !IS_SPACE(*p))
                p++;
            while (*p && IS_SPACE(*p))
                p++;
            http_response_code = atoi(p);
            if (fmInitialized) {
                message(lineBuf2->ptr, 0, 0);
                refresh();
            }
        }
        if (!strncasecmp(lineBuf2->ptr, "content-transfer-encoding:", 26)) {
            p = lineBuf2->ptr + 26;
            while (IS_SPACE(*p))
                p++;
            if (!strncasecmp(p, "base64", 6))
                uf->encoding = ENC_BASE64;
            else if (!strncasecmp(p, "quoted-printable", 16))
                uf->encoding = ENC_QUOTE;
            else if (!strncasecmp(p, "uuencode", 8) || !strncasecmp(p, "x-uuencode", 10))
                uf->encoding = ENC_UUENCODE;
            else
                uf->encoding = ENC_7BIT;
        } else if (!strncasecmp(lineBuf2->ptr, "content-encoding:", 17)) {
            p = lineBuf2->ptr + 17;
            while (IS_SPACE(*p))
                p++;

            parseCompression(uf, p);
        } else if (use_cookie && accept_cookie && pu && check_cookie_accept_domain(pu->host) && (!strncasecmp(lineBuf2->ptr, "Set-Cookie:", 11) || !strncasecmp(lineBuf2->ptr, "Set-Cookie2:", 12))) {
            Str name = Strnew(), value = Strnew(), domain = NULL, path = NULL,
                comment = NULL, commentURL = NULL, port = NULL, tmp2;
            int version, quoted, flag = 0;
            time_t expires = (time_t)-1;

            q = NULL;
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
        } else if (!strncasecmp(lineBuf2->ptr, "w3m-control:", 12) && uf->scheme == SCM_LOCAL_CGI) {
            Str funcname = Strnew();

            p = lineBuf2->ptr + 12;
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
        if (headerlist)
            pushText(headerlist, lineBuf2->ptr);
        Strfree(lineBuf2);
        lineBuf2 = NULL;
    }
    growbuf_destroy(gb);

    if (thru)
        addnewline(newBuf, "", propBuffer, NULL, 0, -1, -1);
    if (src)
        fclose(src);
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
