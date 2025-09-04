#define _GNU_SOURCE
#include "alloc.h"
#include "indep.h"
#include "file.h"
#include "url.h"
#include "file_copy.h"
#include "buffer_loader.h"
#include "ui.h"
#include "tmpfile.h"
#include "istream.h"
#include "HtmlTagParsed.h"
#include "readbuffer.h"
#include "html_title.h"
#include "progress.h"
#include "table.h"
#include "http.h"
#include "form.h"
#include "mysignal.h"
#include "buffer.h"
#include "rc.h"
#include "mailcap.h"
#include "auth.h"
#include "image.h"
#include "etc.h"
#include "tty.h"
#include "screen.h"
#include "local.h"
#include "myctype.h"
#include <signal.h>
#include <setjmp.h>

int FollowRedirection = (10);
char DecodeCTE = (false);
int label_topline = (false);
int UseExternalDirBuffer = (true);
char* DirBufferCommand = ("file:///$LIB/dirlist" CGI_EXTENSION);
char* DefaultType = (NULL);
int displayLinkNumber = (false);
char SimplePreserveSpace = (false);
int squeezeBlankLine = (false);

static sigjmp_buf AbortLoading;
static MySignalHandler KeyAbort(int _dummy)
{
    siglongjmp(AbortLoading, 1);
}

static bool
checkRedirection(ParsedURL* pu)
{
    static ParsedURL* puv = NULL;
    static int nredir = 0;
    static int nredir_size = 0;
    Str tmp;

    if (pu == NULL) {
        nredir = 0;
        nredir_size = 0;
        puv = NULL;
        return true;
    }
    if (nredir >= FollowRedirection) {
        /* FIXME: gettextize? */
        tmp = Sprintf("Number of redirections exceeded %d at %s",
            FollowRedirection, parsedURL2Str(pu)->ptr);
        message(getUI(), MSG_ERR, tmp->ptr);
        return false;
    } else if (nredir_size > 0 && (same_url_p(pu, &puv[(nredir - 1) % nredir_size]) || (!(nredir % 2) && same_url_p(pu, &puv[(nredir / 2) % nredir_size])))) {
        /* FIXME: gettextize? */
        tmp = Sprintf("Redirection loop detected (%s)",
            parsedURL2Str(pu)->ptr);
        message(getUI(), MSG_ERR, tmp->ptr);
        return false;
    }
    if (!puv) {
        nredir_size = FollowRedirection / 2 + 1;
        puv = New_N(ParsedURL, nredir_size);
        memset(puv, 0, sizeof(ParsedURL) * nredir_size);
    }
    copyParsedURL(&puv[nredir % nredir_size], pu);
    nredir++;
    return true;
}

Buffer* _load(ParsedURL pu, struct URLFile f,
    Str page, wc_ces charset, const char* real_type, Buffer* t_buf, bool do_download)
{
    if (page) {
        if (image_source)
            return NULL;
        Str tmp = tmpfname(TMPF_SRC, ".html");
        FILE* src = fopen(tmp->ptr, "w");
        if (src) {
            Str s = wc_Str_conv_strict(page, InnerCharset, charset);
            Strfputs(s, src);
            fclose(src);
        }
        if (do_download) {
            if (!src)
                return NULL;
            const char* file = guessFileName(pu.file);
            doFileMove(tmp->ptr, file);
            return NO_BUFFER;
        }
        Buffer* b = loadHTMLString(page);
        if (b) {
            copyParsedURL(&b->currentURL, &pu);
            b->real_scheme = pu.scheme;
            b->real_type = real_type;
            if (src)
                b->sourcefile = tmp->ptr;
            b->document_charset = charset;
        }
        return b;
    }

    current_content_length = 0;
    const char* p;
    if ((p = getHttpHeaderValue(t_buf->document_header, "Content-Length:")) != NULL)
        current_content_length = strtoclen(p);
    if (do_download) {
        /* download only */
        if (DecodeCTE && IStype(f.stream) != IST_ENCODED)
            f.stream = newEncodedStream(f.stream, f.encoding);
        const char* file;
        if (pu.scheme == SCM_LOCAL) {
            struct stat st;
            if (PreserveTimestamp && !stat(pu.real_file, &st))
                f.modtime = st.st_mtime;
            file = conv_from_system(guessSaveName(NULL, pu.real_file));
        } else
            file = guessSaveName(t_buf->document_header, pu.file);
        if (doFileSave(f, file, current_content_length) == 0)
            UFhalfclose(&f);
        else
            UFclose(&f);
        return NO_BUFFER;
    }

    if ((f.content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
        uncompress_stream(&f, &pu.real_file);
    } else if (f.compression != CMP_NOCOMPRESS) {
        if (is_text_type(real_type) || searchExtViewer(real_type)) {
            if (t_buf == NULL)
                t_buf = newBuffer();
            uncompress_stream(&f, &t_buf->sourcefile);
            uncompressed_file_type(pu.file, &f.ext);
        } else {
            real_type = compress_application_type(f.compression);
            f.compression = CMP_NOCOMPRESS;
        }
    }
    if (image_source) {
        Buffer* b = NULL;
        if (IStype(f.stream) != IST_ENCODED)
            f.stream = newEncodedStream(f.stream, f.encoding);
        if (save2tmp(f, image_source) == 0) {
            b = newBuffer();
            b->sourcefile = image_source;
            b->real_type = real_type;
        }
        UFclose(&f);
        return b;
    }

    if (t_buf == NULL)
        t_buf = newBuffer();
    copyParsedURL(&t_buf->currentURL, &pu);
    t_buf->filename = pu.real_file ? pu.real_file : pu.file ? conv_to_system(pu.file)
                                                            : NULL;
    t_buf->ssl_certificate = f.ssl_certificate;

    // Buffer* (*proc)(struct URLFile*, Buffer*) = loadBuffer;
    Buffer* b;
    if (is_html_type(real_type)) {
        b = loadHTMLBuffer(&f, t_buf);
        b->type = "text/html";
    } else {
        b = loadBuffer(&f, t_buf);
        b->type = "text/plain";
    }
    if (b) {
        if (b->buffername == NULL || b->buffername[0] == '\0') {
            b->buffername = getHttpHeaderValue(b->document_header, "Subject:");
            if (b->buffername == NULL && b->filename != NULL)
                b->buffername = conv_from_system(lastFileName(b->filename));
        }
        if (b->currentURL.scheme == SCM_UNKNOWN)
            b->currentURL.scheme = f.scheme;
        if (f.scheme == SCM_LOCAL && b->sourcefile == NULL)
            b->sourcefile = b->filename;
    }

    UFclose(&f);
    if (b && b != NO_BUFFER) {
        b->real_scheme = f.scheme;
        b->real_type = real_type;
        if (pu.label) {
            if (is_html_type(real_type)) {
                Anchor* a;
                a = searchURLLabel(b, pu.label);
                if (a != NULL) {
                    gotoLine(b, a->start.line);
                    if (label_topline)
                        b->topLine = lineSkip(b, b->topLine,
                            b->currentLine->linenumber
                                - b->topLine->linenumber,
                            false);
                    b->pos = a->start.pos;
                    arrangeCursor(b);
                }
            } else { /* plain text */
                int l = atoi(pu.label);
                gotoRealLine(b, l);
                b->pos = 0;
                arrangeCursor(b);
            }
        }
    }
    if (header_string)
        header_string = NULL;
    if (b && b != NO_BUFFER)
        preFormUpdateBuffer(b);
    return b;
}

/*
 * loadGeneralFile: load file to buffer
 */
Buffer*
loadGeneralFile(char* path, ParsedURL* current, const char* referer,
    int flag, FormList* request, bool do_download)
{
    struct URLFile f, *of = NULL;
    ParsedURL pu;
    Buffer* b = NULL;
    const char* tpath;
    const char* t = "text/plain";
    const char* p;
    const char* real_type = NULL;
    Buffer* t_buf = NULL;
    // int  searchHeader = SearchHeader;
    MySignalHandler (*prevtrap)(int _dummy) = NULL;
    TextList* extra_header = newTextList();
    Str uname = NULL;
    Str pwd = NULL;
    Str realm = NULL;
    int add_auth_cookie_flag;
    unsigned char status = HTST_NORMAL;
    struct URLOption url_option;
    Str tmp;
    Str page = NULL;
    wc_ces charset = WC_CES_US_ASCII;
    struct HttpRequest hr;
    ParsedURL* auth_pu;

    tpath = path;
    prevtrap = NULL;
    add_auth_cookie_flag = 0;

    checkRedirection(NULL);

load_doc: {
    const char* sc_redirect;
    parseURL2(tpath, &pu, current);
    sc_redirect = query_SCONF_SUBSTITUTE_URL(&pu);
    if (sc_redirect && *sc_redirect && checkRedirection(&pu)) {
        tpath = (char*)sc_redirect;
        request = NULL;
        add_auth_cookie_flag = 0;
        current = New(ParsedURL);
        *current = pu;
        status = HTST_NORMAL;
        goto load_doc;
    }
}
    TRAP_OFF;
    url_option.referer = referer;
    url_option.flag = flag;
    f = openURL(tpath, &pu, current, &url_option, request, extra_header, of,
        &hr, &status);
    of = NULL;
    content_charset = 0;
    if (f.stream == NULL) {
        switch (f.scheme) {
        case SCM_LOCAL: {
            struct stat st;
            if (stat(pu.real_file, &st) < 0)
                return NULL;
            if (S_ISDIR(st.st_mode)) {
                if (UseExternalDirBuffer) {
                    Str cmd = Sprintf("%s?dir=%s#current",
                        DirBufferCommand, pu.file);
                    b = loadGeneralFile(cmd->ptr, NULL, NO_REFERER, 0,
                        NULL, do_download);
                    if (b != NULL && b != NO_BUFFER) {
                        copyParsedURL(&b->currentURL, &pu);
                        b->filename = b->currentURL.real_file;
                    }
                    return b;
                } else {
                    page = loadLocalDir(pu.real_file);
                    t = "local:directory";
                    charset = SystemCharset;
                }
            }
        } break;
        case SCM_UNKNOWN:
            /* FIXME: gettextize? */
            message(getUI(), MSG_ERR, Sprintf("Unknown URI: %s", parsedURL2Str(&pu)->ptr)->ptr);
            break;

        default:
            break;
        }
        if (page && page->length > 0)
            goto page_loaded;
        return NULL;
    }

    if (status == HTST_MISSING) {
        TRAP_OFF;
        UFclose(&f);
        return NULL;
    }

    /* openURL() succeeded */
    if (sigsetjmp(AbortLoading, 1) != 0) {
        /* transfer interrupted */
        TRAP_OFF;
        if (b)
            discardBuffer(b);
        UFclose(&f);
        return NULL;
    }

    b = NULL;
    if (f.is_cgi) {
        /* local CGI */
        // searchHeader = true;
    }
    if (header_string)
        header_string = NULL;
    TRAP_ON;
    if (pu.scheme == SCM_HTTP || pu.scheme == SCM_HTTPS) {

        term_cbreak();
        /* FIXME: gettextize? */
        message(getUI(), MSG_INFO, Sprintf("%s contacted. Waiting for reply...", pu.host)->ptr);
        // refresh(ttyWriter());

        if (t_buf == NULL)
            t_buf = newBuffer();

        struct HttpResponse response = readHttpResponse(&f, &pu);
        t_buf->document_header = response.headers;
        if (((response.status_code >= 301 && response.status_code <= 303)
                || response.status_code == 307)
            && (p = (char*)getHttpHeaderValue(t_buf->document_header, "Location:")) != NULL
            && checkRedirection(&pu)) {
            /* document moved */
            /* 301: Moved Permanently */
            /* 302: Found */
            /* 303: See Other */
            /* 307: Temporary Redirect (HTTP/1.1) */
            tpath = url_encode(p, NULL, 0);
            request = NULL;
            UFclose(&f);
            current = New(ParsedURL);
            copyParsedURL(current, &pu);
            t_buf = newBuffer();
            t_buf->bufferprop |= BP_REDIRECTED;
            status = HTST_NORMAL;
            goto load_doc;
        }
        struct ContentTypeCharset cc = getContentType(t_buf->document_header);
        t = cc.content_type;
        if (t == NULL && pu.file != NULL) {
            if (!((response.status_code >= 400 && response.status_code <= 407) || (response.status_code >= 500 && response.status_code <= 505)))
                t = guessContentType(pu.file);
        }
        if (t == NULL)
            t = "text/plain";
        if (add_auth_cookie_flag && realm && uname && pwd) {
            /* If authorization is required and passed */
            add_auth_user_passwd(&pu, qstr_unquote(realm)->ptr, uname, pwd,
                0);
            add_auth_cookie_flag = 0;
        }
        if ((p = getHttpHeaderValue(t_buf->document_header, "WWW-Authenticate:")) != NULL && response.status_code == 401) {
            /* Authentication needed */
            struct http_auth hauth;
            if (findAuthentication(&hauth, t_buf->document_header, "WWW-Authenticate:") != NULL
                && (realm = get_auth_param(hauth.param, "realm")) != NULL) {
                auth_pu = &pu;
                getAuthCookie(&hauth, "Authorization:", extra_header,
                    auth_pu, &hr, request, &uname, &pwd);
                if (uname == NULL) {
                    /* abort */
                    TRAP_OFF;
                    goto page_loaded;
                }
                UFclose(&f);
                add_auth_cookie_flag = 1;
                status = HTST_NORMAL;
                goto load_doc;
            }
        }
        if ((p = getHttpHeaderValue(t_buf->document_header, "Proxy-Authenticate:")) != NULL && response.status_code == 407) {
            /* Authentication needed */
            struct http_auth hauth;
            if (findAuthentication(&hauth, t_buf->document_header, "Proxy-Authenticate:")
                    != NULL
                && (realm = get_auth_param(hauth.param, "realm")) != NULL) {
                auth_pu = schemeToProxy(pu.scheme);
                getAuthCookie(&hauth, "Proxy-Authorization:",
                    extra_header, auth_pu, &hr, request,
                    &uname, &pwd);
                if (uname == NULL) {
                    /* abort */
                    TRAP_OFF;
                    goto page_loaded;
                }
                UFclose(&f);
                add_auth_cookie_flag = 1;
                status = HTST_NORMAL;
                add_auth_user_passwd(auth_pu, qstr_unquote(realm)->ptr, uname, pwd, 1);
                goto load_doc;
            }
        }
        /* XXX: RFC2617 3.2.3 Authentication-Info: ? */

        if (status == HTST_CONNECT) {
            of = &f;
            goto load_doc;
        }

        f.modtime = mymktime(getHttpHeaderValue(t_buf->document_header, "Last-Modified:"));
    } else if (pu.scheme == SCM_FTP) {
        check_compression(&f, path);
        if (f.compression != CMP_NOCOMPRESS) {
            char* t1 = (char*)uncompressed_file_type(pu.file, NULL);
            real_type = f.guess_type;
            if (t1)
                t = t1;
            else
                t = real_type;
        } else {
            real_type = guessContentType(pu.file);
            if (real_type == NULL)
                real_type = "text/plain";
            t = real_type;
        }
    } else if (pu.scheme == SCM_DATA) {
        t = f.guess_type;
    }
    //     else if (searchHeader) {
    //         searchHeader = SearchHeader = false;
    //         if (t_buf == NULL)
    //             t_buf = newBuffer();
    //         readHeader(&f, t_buf, searchHeader_through, &pu);
    //         if (f.is_cgi && (p = checkHeader(t_buf, "Location:")) != NULL && checkRedirection(&pu)) {
    //             /* document moved */
    //             tpath = url_encode(remove_space(p), NULL, 0);
    //             request = NULL;
    //             UFclose(&f);
    //             add_auth_cookie_flag = 0;
    //             current = New(ParsedURL);
    //             copyParsedURL(current, &pu);
    //             t_buf = newBuffer();
    //             t_buf->bufferprop |= BP_REDIRECTED;
    //             status = HTST_NORMAL;
    //             goto load_doc;
    //         }
    // #ifdef AUTH_DEBUG
    //         if ((p = checkHeader(t_buf, "WWW-Authenticate:")) != NULL) {
    //             /* Authentication needed */
    //             struct http_auth hauth;
    //             if (findAuthentication(&hauth, t_buf, "WWW-Authenticate:") != NULL
    //                 && (realm = get_auth_param(hauth.param, "realm")) != NULL) {
    //                 auth_pu = &pu;
    //                 getAuthCookie(&hauth, "Authorization:", extra_header,
    //                     auth_pu, &hr, request, &uname, &pwd);
    //                 if (uname == NULL) {
    //                     /* abort */
    //                     TRAP_OFF;
    //                     goto page_loaded;
    //                 }
    //                 UFclose(&f);
    //                 add_auth_cookie_flag = 1;
    //                 status = HTST_NORMAL;
    //                 goto load_doc;
    //             }
    //         }
    // #endif /* defined(AUTH_DEBUG) */
    //         t = checkContentType(t_buf);
    //         if (t == NULL)
    //             t = "text/plain";
    //     }
    else if (DefaultType) {
        t = DefaultType;
        DefaultType = NULL;
    } else {
        t = guessContentType(pu.file);
        if (t == NULL)
            t = "text/plain";
        real_type = t;
        if (f.guess_type)
            t = f.guess_type;
    }

    /* XXX: can we use guess_type to give the type to loadHTMLstream
     *      to support default utf8 encoding for XHTML here? */
    f.guess_type = t;

page_loaded:
    TRAP_OFF;
    return _load(pu, f, page, charset, t, t_buf, do_download);
}

#define TAG_IS(s, tag, len) \
    (strncasecmp(s, tag, len) == 0 && (s[len] == '>' || IS_SPACE((int)s[len])))

static InputStream _file_lp2;

static void
proc_escape(struct readbuffer* obuf, char** str_return)
{
    char *str = *str_return, *estr;
    int ech = getescapechar(str_return);
    int width, n_add = *str_return - str;
    Lineprop mode = PC_ASCII;

    if (ech < 0) {
        *str_return = str;
        proc_mchar(obuf, obuf->flag & RB_SPECIAL, 1, str_return, PC_ASCII);
        return;
    }
    mode = IS_CNTRL(ech) ? PC_CTRL : PC_ASCII;

    estr = conv_entity(ech);
    check_breakpoint(obuf, obuf->flag & RB_SPECIAL, estr);
    width = get_strwidth(estr);
    if (width == 1 && ech == (unsigned char)*estr && ech != '&' && ech != '<' && ech != '>') {
        if (IS_CNTRL(ech))
            mode = PC_CTRL;
        push_charp(obuf, width, estr, mode);
    } else
        push_nchars(obuf, width, str, n_add, mode);
    set_prevchar(obuf->prevchar, estr, strlen(estr));
    obuf->prev_ctype = mode;
}

static int
need_flushline(struct html_feed_environ* h_env, struct readbuffer* obuf,
    Lineprop mode)
{
    char ch;

    if (obuf->flag & RB_PRE_INT) {
        if (obuf->pos > h_env->limit)
            return 1;
        else
            return 0;
    }

    ch = Strlastchar(obuf->line);
    /* if (ch == ' ' && obuf->tag_sp > 0) */
    if (ch == ' ')
        return 0;

    if (obuf->pos > h_env->limit)
        return 1;

    return 0;
}

#ifndef min
#define min(a, b) ((a) > (b) ? (b) : (a))
#endif /* not min */

/* HTML processing first pass */
void HTMLlineproc0(char* line, struct html_feed_environ* h_env, bool internal)
{
    Lineprop mode;
    int cmd;
    struct readbuffer* obuf = h_env->obuf;
    int indent, delta;
    struct HtmlTagParsed* tag;
    Str tokbuf;
    struct table* tbl = NULL;
    struct table_mode* tbl_mode = NULL;
    int tbl_width = 0;
    int is_hangul, prev_is_hangul = 0;

#ifdef DEBUG
    if (w3m_debug) {
        FILE* f = fopen("zzzproc1", "a");
        fprintf(f, "%c%c%c%c",
            (obuf->flag & RB_PREMODE) ? 'P' : ' ',
            (obuf->table_level >= 0) ? 'T' : ' ',
            (obuf->flag & RB_INTXTA) ? 'X' : ' ',
            (obuf->flag & (RB_SCRIPT | RB_STYLE)) ? 'S' : ' ');
        fprintf(f, "HTMLlineproc1(\"%s\",%d,%lx)\n", line, h_env->limit,
            (unsigned long)h_env);
        fclose(f);
    }
#endif

    tokbuf = Strnew();

table_start:
    if (obuf->table_level >= 0) {
        int level = min(obuf->table_level, MAX_TABLE - 1);
        tbl = tables[level];
        tbl_mode = &table_mode[level];
        tbl_width = table_width(h_env, level);
    }

    while (*line != '\0') {
        char *str, *p;
        int is_tag = false;
        int pre_mode = (obuf->table_level >= 0 && tbl_mode) ? tbl_mode->pre_mode : obuf->flag;
        int end_tag = (obuf->table_level >= 0 && tbl_mode) ? tbl_mode->end_tag : obuf->end_tag;

        if (*line == '<' || obuf->status != R_ST_NORMAL) {
            /*
             * Tag processing
             */
            if (obuf->status == R_ST_EOL)
                obuf->status = R_ST_NORMAL;
            else {
                read_token(h_env->tagbuf, &line, &obuf->status,
                    pre_mode & RB_PREMODE, obuf->status != R_ST_NORMAL);
                if (obuf->status != R_ST_NORMAL)
                    return;
            }
            if (h_env->tagbuf->length == 0)
                continue;
            str = Strdup(h_env->tagbuf)->ptr;
            if (*str == '<') {
                if (str[1] && REALLY_THE_BEGINNING_OF_A_TAG(str))
                    is_tag = true;
                else if (!(pre_mode & (RB_PLAIN | RB_INTXTA | RB_INSELECT | RB_SCRIPT | RB_STYLE | RB_TITLE))) {
                    line = Strnew_m_charp(str + 1, line, NULL)->ptr;
                    str = "&lt;";
                }
            }
        } else {
            read_token(tokbuf, &line, &obuf->status, pre_mode & RB_PREMODE, 0);
            if (obuf->status != R_ST_NORMAL) /* R_ST_AMP ? */
                obuf->status = R_ST_NORMAL;
            str = tokbuf->ptr;
            if (need_number) {
                str = Strnew_m_charp(getLinkNumberStr(-1)->ptr, str, NULL)->ptr;
                need_number = 0;
            }
        }

        if (pre_mode & (RB_PLAIN | RB_INTXTA | RB_INSELECT | RB_SCRIPT | RB_STYLE | RB_TITLE)) {
            if (is_tag) {
                p = str;
                if ((tag = parse_tag(&p, internal))) {
                    if (tag->tagid == end_tag || (pre_mode & RB_INSELECT && tag->tagid == HTML_N_FORM)
                        || (pre_mode & RB_TITLE
                            && (tag->tagid == HTML_N_HEAD
                                || tag->tagid == HTML_BODY)))
                        goto proc_normal;
                }
            }
            /* title */
            if (pre_mode & RB_TITLE) {
                feed_title(str);
                continue;
            }
            /* select */
            if (pre_mode & RB_INSELECT) {
                if (obuf->table_level >= 0)
                    goto proc_normal;
                feed_select(str);
                continue;
            }
            if (is_tag) {
                if (strncmp(str, "<!--", 4) && (p = strchr(str + 1, '<'))) {
                    str = Strnew_charp_n(str, p - str)->ptr;
                    line = Strnew_m_charp(p, line, NULL)->ptr;
                }
                is_tag = false;
                continue;
            }
            if (obuf->table_level >= 0)
                goto proc_normal;
            /* textarea */
            if (pre_mode & RB_INTXTA) {
                feed_textarea(str);
                continue;
            }
            /* script */
            if (pre_mode & RB_SCRIPT)
                continue;
            /* style */
            if (pre_mode & RB_STYLE)
                continue;
        }

    proc_normal:
        if (obuf->table_level >= 0 && tbl && tbl_mode) {
            /*
             * within table: in <table>..</table>, all input tokens
             * are fed to the table renderer, and then the renderer
             * makes HTML output.
             */
            switch (feed_table(tbl, str, tbl_mode, tbl_width, internal)) {
            case 0:
                /* </table> tag */
                obuf->table_level--;
                if (obuf->table_level >= MAX_TABLE - 1)
                    continue;
                end_table(tbl);
                if (obuf->table_level >= 0) {
                    struct table* tbl0 = tables[obuf->table_level];
                    str = Sprintf("<table_alt tid=%d>", tbl0->ntable)->ptr;
                    if (tbl0->row < 0)
                        continue;
                    pushTable(tbl0, tbl);
                    tbl = tbl0;
                    tbl_mode = &table_mode[obuf->table_level];
                    tbl_width = table_width(h_env, obuf->table_level);
                    feed_table(tbl, str, tbl_mode, tbl_width, true);
                    continue;
                    /* continue to the next */
                }
                if (obuf->flag & RB_DEL)
                    continue;
                /* all tables have been read */
                if (tbl->vspace > 0 && !(obuf->flag & RB_IGNORE_P)) {
                    int indent = h_env->envs[h_env->envc].indent;
                    flushline(h_env, obuf, indent, 0, h_env->limit);
                    do_blankline(h_env, obuf, indent, 0, h_env->limit);
                }
                save_fonteffect(h_env, obuf);
                initRenderTable();
                renderTable(tbl, tbl_width, h_env);
                restore_fonteffect(h_env, obuf);
                obuf->flag &= ~RB_IGNORE_P;
                if (tbl->vspace > 0) {
                    int indent = h_env->envs[h_env->envc].indent;
                    do_blankline(h_env, obuf, indent, 0, h_env->limit);
                    obuf->flag |= RB_IGNORE_P;
                }
                set_space_to_prevchar(obuf->prevchar);
                continue;
            case 1:
                /* <table> tag */
                break;
            default:
                continue;
            }
        }

        if (is_tag) {
            /*** Beginning of a new tag ***/
            if ((tag = parse_tag(&str, internal)))
                cmd = tag->tagid;
            else
                continue;
            /* process tags */
            if (HTMLtagproc1(tag, h_env) == 0) {
                /* preserve the tag for second-stage processing */
                if (tag->need_reconstruct)
                    h_env->tagbuf = parsedtag2str(tag);
                push_tag(obuf, h_env->tagbuf->ptr, cmd);
            } else {
                process_idattr(obuf, cmd, tag);
            }
            obuf->bp.init_flag = 1;
            clear_ignore_p_flag(obuf, cmd);
            if (cmd == HTML_TABLE)
                goto table_start;
            else {
                if (displayLinkNumber && cmd == HTML_A && !internal)
                    if (h_env->obuf->anchor.url)
                        need_number = 1;
                continue;
            }
        }

        if (obuf->flag & (RB_DEL | RB_S))
            continue;
        while (*str) {
            mode = get_mctype(str);
            delta = get_mcwidth(str);
            if (obuf->flag & (RB_SPECIAL & ~RB_NOBR)) {
                char ch = *str;
                if (!(obuf->flag & RB_PLAIN) && (*str == '&')) {
                    char* p = str;
                    int ech = getescapechar(&p);
                    if (ech == '\n' || ech == '\r') {
                        ch = '\n';
                        str = p - 1;
                    } else if (ech == '\t') {
                        ch = '\t';
                        str = p - 1;
                    }
                }
                if (ch != '\n')
                    obuf->flag &= ~RB_IGNORE_P;
                if (ch == '\n') {
                    str++;
                    if (obuf->flag & RB_IGNORE_P) {
                        obuf->flag &= ~RB_IGNORE_P;
                        continue;
                    }
                    if (obuf->flag & RB_PRE_INT)
                        PUSH(obuf, ' ');
                    else
                        flushline(h_env, obuf, h_env->envs[h_env->envc].indent,
                            1, h_env->limit);
                } else if (ch == '\t') {
                    do {
                        PUSH(obuf, ' ');
                    } while ((h_env->envs[h_env->envc].indent + obuf->pos)
                            % Tabstop
                        != 0);
                    str++;
                } else if (obuf->flag & RB_PLAIN) {
                    char* p = html_quote_char(*str);
                    if (p) {
                        push_charp(obuf, 1, p, PC_ASCII);
                        str++;
                    } else {
                        proc_mchar(obuf, 1, delta, &str, mode);
                    }
                } else {
                    if (*str == '&')
                        proc_escape(obuf, &str);
                    else
                        proc_mchar(obuf, 1, delta, &str, mode);
                }
                if (obuf->flag & (RB_SPECIAL & ~RB_PRE_INT))
                    continue;
            } else {
                if (!IS_SPACE(*str))
                    obuf->flag &= ~RB_IGNORE_P;
                if ((mode == PC_ASCII || mode == PC_CTRL) && IS_SPACE(*str)) {
                    if (*obuf->prevchar->ptr != ' ') {
                        PUSH(obuf, ' ');
                    }
                    str++;
                } else {
                    if (mode == PC_KANJI1)
                        is_hangul = wtf_is_hangul((wc_uchar*)str);
                    else
                        is_hangul = 0;
                    if (!SimplePreserveSpace && mode == PC_KANJI1 && !is_hangul && !prev_is_hangul && obuf->pos > h_env->envs[h_env->envc].indent && Strlastchar(obuf->line) == ' ') {
                        while (obuf->line->length >= 2 && !strncmp(obuf->line->ptr + obuf->line->length - 2, "  ", 2)
                            && obuf->pos >= h_env->envs[h_env->envc].indent) {
                            Strshrink(obuf->line, 1);
                            obuf->pos--;
                        }
                        if (obuf->line->length >= 3 && obuf->prev_ctype == PC_KANJI1 && Strlastchar(obuf->line) == ' ' && obuf->pos >= h_env->envs[h_env->envc].indent) {
                            Strshrink(obuf->line, 1);
                            obuf->pos--;
                        }
                    }
                    prev_is_hangul = is_hangul;
                    if (*str == '&')
                        proc_escape(obuf, &str);
                    else
                        proc_mchar(obuf, obuf->flag & RB_SPECIAL, delta, &str,
                            mode);
                }
            }
            if (need_flushline(h_env, obuf, mode)) {
                char* bp = obuf->line->ptr + obuf->bp.len;
                char* tp = bp - obuf->bp.tlen;
                int i = 0;

                if (tp > obuf->line->ptr && tp[-1] == ' ')
                    i = 1;

                indent = h_env->envs[h_env->envc].indent;
                if (obuf->bp.pos - i > indent) {
                    Str line;
                    append_tags(obuf); /* may reallocate the buffer */
                    bp = obuf->line->ptr + obuf->bp.len;
                    line = Strnew_charp(bp);
                    Strshrink(obuf->line, obuf->line->length - obuf->bp.len);
                    if (obuf->pos - i > h_env->limit)
                        obuf->flag |= RB_FILL;
                    back_to_breakpoint(obuf);
                    flushline(h_env, obuf, indent, 0, h_env->limit);
                    obuf->flag &= ~RB_FILL;
                    HTMLlineproc0(line->ptr, h_env, true);
                }
            }
        }
    }
    if (!(obuf->flag & (RB_SPECIAL | RB_INTXTA | RB_INSELECT))) {
        char* tp;
        int i = 0;

        if (obuf->bp.pos == obuf->pos) {
            tp = &obuf->line->ptr[obuf->bp.len - obuf->bp.tlen];
        } else {
            tp = &obuf->line->ptr[obuf->line->length];
        }

        if (tp > obuf->line->ptr && tp[-1] == ' ')
            i = 1;
        indent = h_env->envs[h_env->envc].indent;
        if (obuf->pos - i > h_env->limit) {
            obuf->flag |= RB_FILL;
            flushline(h_env, obuf, indent, 0, h_env->limit);
            obuf->flag &= ~RB_FILL;
        }
    }
}

/*
 * loadHTMLBuffer: read file and make new buffer
 */
Buffer*
loadHTMLBuffer(struct URLFile* f, Buffer* newBuf)
{
    FILE* src = NULL;
    Str tmp;

    if (newBuf == NULL)
        newBuf = newBuffer();
    if (newBuf->sourcefile == NULL && (f->scheme != SCM_LOCAL || newBuf->mailcap)) {
        tmp = tmpfname(TMPF_SRC, ".html");
        src = fopen(tmp->ptr, "w");
        if (src)
            newBuf->sourcefile = tmp->ptr;
    }

    loadHTMLstream(f, newBuf, src, false);

    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;
    if (n_textarea)
        formResetBuffer(newBuf, newBuf->formitem);
    if (src)
        fclose(src);

    return newBuf;
}

void loadHTMLstream(struct URLFile* f, Buffer* newBuf, FILE* src, int internal)
{
    Str html = Strnew();
    Str lineBuf2;

    // wc_ces doc_charset = DocumentCharset;
    // wc_ces charset = WC_CES_US_ASCII;
    // if (newBuf != NULL) {
    //     if (newBuf->document_charset)
    //         charset = /*doc_charset =*/newBuf->document_charset;
    // }
    // if (content_charset && UseContentCharset)
    //     doc_charset = content_charset;
    // else if (f->guess_type && !strcasecmp(f->guess_type, "application/xhtml+xml"))
    //     doc_charset = WC_CES_UTF_8;
    // meta_charset = 0;
    // cur_document_charset = charset;

    while ((lineBuf2 = StrmyUFgets(f)) && lineBuf2->length) {
        Strcat(html, lineBuf2);
    }

    struct UI ui = getUI();
    loadHTML(html, WC_CES_SHIFT_JIS /*WC_CES_US_ASCII*/, ui.vt->COLS, ui.use_graphic, internal, newBuf);
    //     struct TermEntry* t = getTermEntry();
    //     struct environment envs[MAX_ENV_LEVEL];
    //     long long linelen = 0;
    //     long long trbyte = 0;
    //     Str lineBuf2 = Strnew();
    //     wc_ces charset = WC_CES_US_ASCII;
    //     wc_ces  doc_charset = DocumentCharset;
    //     struct html_feed_environ htmlenv1;
    //     struct readbuffer obuf;
    //     int  image_flag;
    //     MySignalHandler (* prevtrap)(int _dummy) = NULL;
    //
    //     if (graph_ok(t)) {
    //         symbol_width = symbol_width0 = 1;
    //     } else {
    //         symbol_width0 = 0;
    //         get_symbol(DisplayCharset, &symbol_width0);
    //         symbol_width = WcOption.use_wide ? symbol_width0 : 1;
    //     }
    //
    //     init_title();
    //     init2();
    //     if (newBuf->image_flag)
    //         image_flag = newBuf->image_flag;
    //     else if (activeImage && displayImage && autoImage)
    //         image_flag = IMG_FLAG_AUTO;
    //     else
    //         image_flag = IMG_FLAG_SKIP;
    //
    //     init_henv(&htmlenv1, &obuf, envs, MAX_ENV_LEVEL, NULL, newBuf->width, 0);
    //
    //     htmlenv1.buf = newTextLineList();
    // #if defined(USE_M17N) || defined(USE_IMAGE)
    //     cur_baseURL = baseURL(newBuf);
    // #endif
    //
    //     if (sigsetjmp(AbortLoading, 1) != 0) {
    //         HTMLlineproc0("<br>Transfer Interrupted!<br>", &htmlenv1, true);
    //         goto phase2;
    //     }
    //     TRAP_ON;
    //
    //     if (newBuf != NULL) {
    //         if (newBuf->document_charset)
    //             charset = doc_charset = newBuf->document_charset;
    //     }
    //     if (content_charset && UseContentCharset)
    //         doc_charset = content_charset;
    //     else if (f->guess_type && !strcasecmp(f->guess_type, "application/xhtml+xml"))
    //         doc_charset = WC_CES_UTF_8;
    //     meta_charset = 0;
    //     if (IStype(f->stream) != IST_ENCODED)
    //         f->stream = newEncodedStream(f->stream, f->encoding);
    //     while ((lineBuf2 = StrmyUFgets(f)) && lineBuf2->length) {
    //         if (src)
    //             Strfputs(lineBuf2, src);
    //         linelen += lineBuf2->length;
    //         showProgress(current_content_length, &linelen, &trbyte);
    //         if (meta_charset) { /* <META> */
    //             if (content_charset == 0 && UseContentCharset) {
    //                 doc_charset = meta_charset;
    //                 charset = WC_CES_US_ASCII;
    //             }
    //             meta_charset = 0;
    //         }
    //         lineBuf2 = convertLine(f, lineBuf2, HTML_MODE, &charset, doc_charset);
    //         cur_document_charset = charset;
    //         HTMLlineproc0(lineBuf2->ptr, &htmlenv1, internal);
    //     }
    //     if (obuf.status != R_ST_NORMAL) {
    //         HTMLlineproc0("\n", &htmlenv1, internal);
    //     }
    //     obuf.status = R_ST_NORMAL;
    //     completeHTMLstream(&htmlenv1, &obuf);
    //     flushline(&htmlenv1, &obuf, 0, 2, htmlenv1.limit);
    // #if defined(USE_M17N) || defined(USE_IMAGE)
    //     cur_baseURL = NULL;
    // #endif
    //     cur_document_charset = 0;
    //     if (htmlenv1.title)
    //         newBuf->buffername = htmlenv1.title;
    // phase2:
    //     newBuf->trbyte = trbyte + linelen;
    //     TRAP_OFF;
    //     newBuf->document_charset = charset;
    //     newBuf->image_flag = image_flag;
    //     HTMLlineproc2(newBuf, htmlenv1.buf);
}

/*
 * loadHTMLString: read string and make new buffer
 */
Buffer*
loadHTMLString(Str page)
{
    struct URLFile f;
    MySignalHandler (*prevtrap)(int _dummy) = NULL;
    Buffer* newBuf;

    init_stream(&f, SCM_LOCAL, newStrStream(page));

    newBuf = newBuffer();
    if (sigsetjmp(AbortLoading, 1) != 0) {
        TRAP_OFF;
        discardBuffer(newBuf);
        UFclose(&f);
        return NULL;
    }
    TRAP_ON;

    newBuf->document_charset = InnerCharset;
    loadHTMLstream(&f, newBuf, NULL, true);
    newBuf->document_charset = WC_CES_US_ASCII;

    TRAP_OFF;
    UFclose(&f);
    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;
    newBuf->type = "text/html";
    newBuf->real_type = newBuf->type;
    if (n_textarea)
        formResetBuffer(newBuf, newBuf->formitem);
    return newBuf;
}

/*
 * loadBuffer: read file and make new buffer
 */
Buffer*
loadBuffer(struct URLFile* uf, Buffer* newBuf)
{
    FILE* src = NULL;
    wc_ces charset = WC_CES_US_ASCII;
    wc_ces doc_charset = DocumentCharset;
    Str lineBuf2;
    char pre_lbuf = '\0';
    int nlines;
    Str tmpf;
    long long linelen = 0, trbyte = 0;
    Lineprop* propBuffer = NULL;
    Linecolor* colorBuffer = NULL;
    MySignalHandler (*prevtrap)(int _dummy) = NULL;

    if (newBuf == NULL)
        newBuf = newBuffer();

    if (sigsetjmp(AbortLoading, 1) != 0) {
        goto _end;
    }
    TRAP_ON;

    if (newBuf->sourcefile == NULL && (uf->scheme != SCM_LOCAL || newBuf->mailcap)) {
        tmpf = tmpfname(TMPF_SRC, NULL);
        src = fopen(tmpf->ptr, "w");
        if (src)
            newBuf->sourcefile = tmpf->ptr;
    }
    if (newBuf->document_charset)
        charset = doc_charset = newBuf->document_charset;
    if (content_charset && UseContentCharset)
        doc_charset = content_charset;

    nlines = 0;
    if (IStype(uf->stream) != IST_ENCODED)
        uf->stream = newEncodedStream(uf->stream, uf->encoding);
    while ((lineBuf2 = StrmyISgets(uf->stream)) && lineBuf2->length) {
        if (src)
            Strfputs(lineBuf2, src);
        linelen += lineBuf2->length;
        showProgress(current_content_length, &linelen, &trbyte);
        lineBuf2 = convertLine(uf, lineBuf2, HEADER_MODE, &charset, doc_charset);
        if (squeezeBlankLine) {
            if (lineBuf2->ptr[0] == '\n' && pre_lbuf == '\n') {
                ++nlines;
                continue;
            }
            pre_lbuf = lineBuf2->ptr[0];
        }
        ++nlines;
        Strchop(lineBuf2);
        lineBuf2 = checkType(lineBuf2, &propBuffer, NULL);
        addnewline(newBuf, lineBuf2->ptr, propBuffer, colorBuffer,
            lineBuf2->length, -1, nlines);
    }
_end:
    TRAP_OFF;
    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;
    newBuf->trbyte = trbyte + linelen;
    newBuf->document_charset = charset;
    if (src)
        fclose(src);

    return newBuf;
}
