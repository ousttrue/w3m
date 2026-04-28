#include "file.h"
#include "http_client.h"
#include "global.h"
#include "display.h"
#include "http_request.h"
#include "http_auth.h"
#include "auth.h"
#include "html_loader.h"
#include "filepath.h"
#include "UrlFile.h"
#include "input_stream.h"
#include "indep.h"
#include "alloc.h"
#include "content_type.h"
#include "term_tty.h"
#include "siteconf.h"
#include "form.h"
#include "terms.h"
#include "html_feed_environ.h"
#include "anchor.h"
#include "downloadlist.h"
#include "main.h"
#include "mailcap.h"
#include "etc.h"
#include "buffer.h"
#include "textlist.h"
#include "symbol.h"
#include "line_input.h"
#include "myctype.h"
// #include "signal_util.h"
#include "html.h"
#include "local_cgi.h"
#include "wc_util.h"
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>

typedef struct Buffer* (*LoadProc)(struct CmdArgs* args, struct URLFile*, struct Buffer*);

static struct Buffer*
loadSomething(struct CmdArgs* args, struct URLFile* f, LoadProc loadproc, struct Buffer* defaultbuf)
{
    struct Buffer* buf = loadproc(args, f, defaultbuf);
    if (buf == NULL)
        return NULL;

    if (buf->buffername == NULL || buf->buffername[0] == '\0') {
        buf->buffername = http_response_get(&buf->http_response, "Subject:");
        if (buf->buffername == NULL && buf->filename != NULL)
            buf->buffername = conv_from_system(lastFileName(buf->filename));
    }
    if (buf->currentURL.scheme == SCM_UNKNOWN)
        buf->currentURL = f->url;
    if (f->url.scheme == SCM_FILE && buf->sourcefile == NULL)
        buf->sourcefile = buf->filename;
    if (loadproc == loadHTMLBuffer
        || loadproc == loadImageBuffer)
        buf->type = "text/html";
    else
        buf->type = "text/plain";
    return buf;
}

#define IS_DIRECTORY(m) (((m) & S_IFMT) == S_IFDIR)

int dir_exist(const char* path)
{
    if (path == NULL || *path == '\0')
        return 0;
    struct stat stbuf;
    if (stat(path, &stbuf) == -1)
        return 0;
    return IS_DIRECTORY(stbuf.st_mode);
}

static int
is_dump_text_type(const char* type)
{
    struct mailcap* mcap;
    return (type && (mcap = searchExtViewer(type)) && (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)));
}

static int
is_text_type(const char* type)
{
    return (type == NULL || type[0] == '\0' || strncasecmp(type, "text/", 5) == 0 || (strncasecmp(type, "application/", 12) == 0 && strstr(type, "xhtml") != NULL) || strncasecmp(type, "message/", sizeof("message/") - 1) == 0);
}

static int
is_plain_text_type(const char* type)
{
    return ((type && strcasecmp(type, "text/plain") == 0) || (is_text_type(type) && !is_dump_text_type(type)));
}

static int
setModtime(const char* path, time_t modtime)
{
    struct utimbuf t;
    struct stat st;
    if (stat(path, &st) == 0)
        t.actime = st.st_atime;
    else
        t.actime = time(NULL);
    t.modtime = modtime;
    return utime(path, &t);
}

static bool checkSaveFile(int des, const char* path2)
{
    if (des < 0)
        return true;

    if (*path2 == '|' && PermitSaveToPipe)
        return true;

    struct stat st1, st2;
    if ((fstat(des, &st1) == 0) && (stat(path2, &st2) == 0))
        if (st1.st_ino == st2.st_ino)
            return false;

    return true;
}

static bool doFileSave(struct CmdArgs* args, struct URLFile uf, const char* defstr)
{
    Str msg;
    Str filen;
    const char *p, *q;
    pid_t pid;
    const char* lock;
    const char* tmpf = NULL;

    if (fmInitialized) {
        p = searchKeyData();
        if (p == NULL || *p == '\0') {
            /* FIXME: gettextize? */
            p = inputLineHist(args, "(Download)Save file to: ",
                defstr, IN_FILENAME, HistorySave);
            if (p == NULL || *p == '\0')
                return false;
            p = conv_to_system(p);
        }
        if (!checkOverWrite(args, p))
            return false;

        if (!checkSaveFile(ist_fd(uf.stream), p)) {
            /* FIXME: gettextize? */
            msg = Sprintf("Can't save. Load file and %s are identical.",
                conv_from_system(p));
            disp_err_message(args, msg->ptr, FALSE);
            return false;
        }
        /*
         * if (save2tmp(uf, p) < 0) {
         * msg = Sprintf("Can't save to %s", conv_from_system(p));
         * disp_err_message(msg->ptr, FALSE);
         * }
         */
        lock = tmpfname(TMPF_DFL, ".lock");

        symlink(p, lock);
        flush_tty();
        pid = fork();
        if (!pid) {
            if ((uf.compression != CMP_NOCOMPRESS) && AutoUncompress) {
                struct Uncompressed uncompressed = uncompressed_pipe(&uf, compression_from_type(uf.compression));
                if (uncompressed.pipe) {
                    unlink(uncompressed.tmpf);
                    uf.stream = ist_from_fp(uncompressed.pipe, fclose);
                    uf.url.scheme = SCM_FILE;
                }
            }
            setup_child(FALSE, 0, ist_fd(uf.stream));
            bool success = ist_save2tmp(uf.stream, uf.url.scheme, p);
            if (success && PreserveTimestamp && uf.modtime != -1)
                setModtime(p, uf.modtime);
            UFclose(&uf);
            unlink(lock);
            if (!success)
                exit(1);
            exit(0);
        }
        addDownloadList(pid, parsedURL2Str(&uf.url)->ptr, p, lock, current_content_length);
    } else {
        q = searchKeyData();
        if (q == NULL || *q == '\0') {
            /* FIXME: gettextize? */
            printf("(Download)Save file to: ");
            fflush(stdout);
            filen = Strfgets(stdin);
            if (filen->length == 0)
                return false;
            q = filen->ptr;
        }
        for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
            ;
        ((char*)p)[1] = '\0';
        if (*q == '\0')
            return false;
        p = expandPath(q);
        if (!checkOverWrite(args, p))
            return false;
        if (!checkSaveFile(ist_fd(uf.stream), p)) {
            /* FIXME: gettextize? */
            printf("Can't save. Load file and %s are identical.", p);
            return false;
        }
        if (uf.compression != CMP_NOCOMPRESS && AutoUncompress) {
            struct Uncompressed uncompressed = uncompressed_pipe(&uf, compression_from_type(uf.compression));
            if (uncompressed.pipe) {
                unlink(uncompressed.tmpf);
                uf.stream = ist_from_fp(uncompressed.pipe, fclose);
                uf.url.scheme = SCM_FILE;
            }
        }
        if (!ist_save2tmp(uf.stream, uf.url.scheme, p)) {
            /* FIXME: gettextize? */
            printf("Can't save to %s\n", p);
            return false;
        }
        if (PreserveTimestamp && uf.modtime != -1)
            setModtime(p, uf.modtime);
    }
    return true;
}

#define DO_EXTERNAL ((struct Buffer * (*)(struct CmdArgs * args, struct URLFile*, struct Buffer*)) doExternal)

static struct Buffer* page_loaded(struct CmdArgs* args, Str page, wc_ces charset, struct Url url,
    const char* t, const char* real_type, struct Buffer* t_buf, struct URLFile f, int flag)
{
    if (page) {
        if (image_source)
            return NULL;
        const char* tmpf = tmpfname(TMPF_SRC, ".html");
        FILE* src = fopen(tmpf, "w");
        if (src) {
            Str s = Strnew_wc_output(wc_Str_conv_strict(WcOption, page->ptr, page->length, InnerCharset, charset));
            Strfputs(s, src);
            fclose(src);
        }
        if (do_download) {
            if (!src)
                return NULL;
            const char* file = alloc_guess_filename(url.file);
            doFileCopy(args, tmpf, file);
            unlink(tmpf);
            return NO_BUFFER;
        }
        struct Buffer* b = loadHTMLString(page);
        if (b) {
            copyParsedURL(&b->currentURL, &url);
            b->real_scheme = url.scheme;
            b->real_type = t;
            if (src)
                b->sourcefile = tmpf;
            b->document_charset = charset;
        }
        return b;
    }

    if (real_type == NULL)
        real_type = t;

    struct Buffer* (*proc)(struct CmdArgs* args, struct URLFile*, struct Buffer*) = loadBuffer;

    current_content_length = 0;
    const char* p;
    if (t_buf && (p = http_response_get(&t_buf->http_response, "Content-Length:")) != NULL)
        current_content_length = strtoll(p, NULL, 10);

    if (do_download) {
        /* download only */
        const char* file;
        // TRAP_OFF;
        if (url.scheme == SCM_FILE) {
            struct stat st;
            if (PreserveTimestamp && !stat(url.real_file, &st))
                f.modtime = st.st_mtime;
            file = conv_from_system(http_response_guess_save_name(NULL, url.real_file));
        } else
            file = http_response_guess_save_name(&t_buf->http_response, url.file);
        doFileSave(args, f, file);
        UFclose(&f);
        return NO_BUFFER;
    }

    if ((f.compression != CMP_NOCOMPRESS) && AutoUncompress) {
        struct Uncompressed uncompressed = uncompressed_pipe(&f, compression_from_type(f.compression));
        if (uncompressed.pipe) {
            url.real_file = uncompressed.tmpf;
            f.stream = ist_from_fp(uncompressed.pipe, fclose);
            f.url.scheme = SCM_FILE;
        }
    } else if (f.compression != CMP_NOCOMPRESS) {
        if ((is_text_type(t) || searchExtViewer(t))) {
            if (t_buf == NULL)
                t_buf = newBuffer(INIT_BUFFER_WIDTH);
            struct Uncompressed uncompressed = uncompressed_pipe(&f, compression_from_type(f.compression));
            if (uncompressed.pipe) {
                t_buf->sourcefile = uncompressed.tmpf;
                f.stream = ist_from_fp(uncompressed.pipe, fclose);
                f.url.scheme = SCM_FILE;
            }
            struct ContentTypeWithExt ce = compression_from_path_to_content_type(url.file);
        } else {
            struct CompressionDecoder* d = compression_from_type(f.compression);
            t = d ? d->mime_type : NULL; // compress_application_type(f.compression);
            f.compression = CMP_NOCOMPRESS;
        }
    }
    if (image_source) {
        struct Buffer* b = NULL;
        if (ist_save2tmp(f.stream, f.url.scheme, image_source)) {
            b = newBuffer(INIT_BUFFER_WIDTH);
            b->sourcefile = image_source;
            b->real_type = t;
        }
        UFclose(&f);
        // TRAP_OFF;
        return b;
    }

    if (is_html_type(t))
        proc = loadHTMLBuffer;
    else if (is_plain_text_type(t))
        proc = loadBuffer;
    else if (activeImage && displayImage && !useExtImageViewer && !strncasecmp(t, "image/", 6))
        proc = loadImageBuffer;

    if (t_buf == NULL)
        t_buf = newBuffer(INIT_BUFFER_WIDTH);
    copyParsedURL(&t_buf->currentURL, &url);
    t_buf->filename = url.real_file ? url.real_file : url.file ? conv_to_system(url.file)
                                                               : NULL;
    if (flag & RG_FRAME) {
        t_buf->bufferprop |= BP_FRAME;
    }
    t_buf->ssl_certificate = f.ssl_certificate;
    frame_source = flag & RG_FRAME_SRC;

    struct Buffer* b;
    if (proc == DO_EXTERNAL) {
        b = doExternal(args, f, t, t_buf);
    } else {
        b = loadSomething(args, &f, proc, t_buf);
    }

    UFclose(&f);
    frame_source = 0;
    if (b && b != NO_BUFFER) {
        b->real_scheme = f.url.scheme;
        b->real_type = real_type;
        if (url.label) {
            if (proc == loadHTMLBuffer) {
                struct Anchor* a;
                a = searchURLLabel(b, url.label);
                if (a != NULL) {
                    gotoLine(b, a->start.line);
                    if (label_topline)
                        b->topLine = lineSkip(b, b->topLine,
                            b->currentLine->linenumber
                                - b->topLine->linenumber,
                            FALSE);
                    b->pos = a->start.pos;
                    arrangeCursor(b);
                }
            } else { /* plain text */
                int l = atoi(url.label);
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
    // TRAP_OFF;
    return b;
}

struct Buffer* loadGeneralFile(struct CmdArgs* args, const char* path, struct Url* current, struct Form* post,
    const char* referer, enum UrlOptionFlags flag)
{
    struct HttpMessageSession* m = NULL;
    struct URLFile* f = NULL;
    struct Buffer* b = NULL;
    const char *t = "text/plain", *p, *real_type = NULL;
    struct Buffer* t_buf = NULL;
    int searchHeader = SearchHeader;
    int searchHeader_through = TRUE;
    // SignalFunc prevtrap = NULL;
    TextList* extra_header = newTextList();
    Str uname = NULL;
    Str pwd = NULL;
    Str realm = NULL;
    int add_auth_cookie_flag;
    const char* tmpf;
    Str page = NULL;
    wc_ces charset = WC_CES_US_ASCII;
    struct Url* auth_pu;

    const char* tpath = path;
    add_auth_cookie_flag = 0;

    struct HttpClient http;
    http_init(&http, current, flag);
    http_redirect(&http, path, post, referer);

load_doc:
    //
    {
        struct Url pu = parseURL2(tpath, http_current(&http));
        const char* sc_redirect = query_SCONF_SUBSTITUTE_URL(&pu);
        if (sc_redirect && *sc_redirect) {
            enum HttpReidrectionStatus redirection = http_redirect(&http, sc_redirect, NULL, referer);
            if (redirection != HTTP_REDIRECTION_OK) {
                return 0;
            }

            tpath = sc_redirect;
            // http.post = NULL;
            add_auth_cookie_flag = 0;
            // http.current = New(struct Url);
            // *http.current = pu;
            // f.status = HTST_NORMAL;
            goto load_doc;
        }
    }
    // TRAP_OFF;
    m = http_open(&http, args, extra_header);
    f = &m->transport;
    // of = NULL;
    content_charset = 0;
    if (f->stream == NULL) {
        switch (f->url.scheme) {
        case SCM_FILE: {
            struct stat st;
            if (stat(f->url.real_file, &st) < 0)
                return NULL;
            if (S_ISDIR(st.st_mode)) {
                if (UseExternalDirBuffer) {
                    Str cmd = Sprintf("%s?dir=%s#current",
                        DirBufferCommand, f->url.file);
                    b = loadGeneralFile(args, cmd->ptr, NULL, NULL, NO_REFERER, 0);
                    if (b != NULL && b != NO_BUFFER) {
                        copyParsedURL(&b->currentURL, &f->url);
                        b->filename = b->currentURL.real_file;
                    }
                    return b;
                } else {
                    page = loadLocalDir(f->url.real_file);
                    t = "local:directory";
                    charset = SystemCharset;
                }
            }
        } break;
        case SCM_UNKNOWN:
            /* FIXME: gettextize? */
            disp_err_message(args, Sprintf("Unknown URI: %s", parsedURL2Str(&f->url)->ptr)->ptr,
                FALSE);
            break;
        default:
            break;
        }
        if (page && page->length > 0) {
            return page_loaded(args, page, charset, f->url, t, real_type, t_buf, *f, http.flag);
        }
        return NULL;
    }

    if (m->transport_status == HTST_MISSING) {
        // TRAP_OFF;
        UFclose(f);
        return NULL;
    }

    /* openURL() succeeded */
    // if (SETJMP(AbortLoading) != 0) {
    //     /* transfer interrupted */
    //     TRAP_OFF;
    //     if (b)
    //         discardBuffer(b);
    //     UFclose(f);
    //     return NULL;
    // }

    b = NULL;
    if (f->is_cgi) {
        /* local CGI */
        searchHeader = TRUE;
        searchHeader_through = FALSE;
    }
    if (header_string)
        header_string = NULL;
    // TRAP_ON;
    if (f->url.scheme == SCM_HTTP || f->url.scheme == SCM_HTTPS) {
        if (fmInitialized) {
            term_cbreak();
            /* FIXME: gettextize? */
            message(Sprintf("%s contacted. Waiting for reply...", f->url.host)->ptr, 0, 0);
            refresh();
        }
        if (t_buf == NULL)
            t_buf = newBuffer(INIT_BUFFER_WIDTH);
        t_buf->http_response = http_response_header(f->stream, f->url.scheme);
        f->compression = http_response_process(&t_buf->http_response, args, &f->url);
        if (((t_buf->http_response.status_code >= 301 && t_buf->http_response.status_code <= 303)
                || t_buf->http_response.status_code == 307)
            && (p = http_response_get(&t_buf->http_response, "Location:")) != NULL) {
            // document moved
            // 301: Moved Permanently
            // 302: Found
            // 303: See Other
            // 307: Temporary Redirect (HTTP/1.1)

            enum HttpReidrectionStatus redirection = http_redirect(&http, url_encode(p, NULL, 0), NULL, referer);
            if (redirection != HTTP_REDIRECTION_OK) {
                return 0;
            }

            tpath = url_encode(p, NULL, 0);
            // http.post = NULL;
            UFclose(f);
            // http.current = New(struct Url);
            // copyParsedURL(http.current, &f.url);
            t_buf = newBuffer(INIT_BUFFER_WIDTH);
            t_buf->bufferprop |= BP_REDIRECTED;
            m->transport_status = HTST_NORMAL;
            goto load_doc;
        }
        t = http_response_get_content_type(&t_buf->http_response, &content_charset);
        if (t == NULL && f->url.file != NULL) {
            if (!((t_buf->http_response.status_code >= 400 && t_buf->http_response.status_code <= 407) || (t_buf->http_response.status_code >= 500 && t_buf->http_response.status_code <= 505)))
                t = guessContentType(f->url.file);
        }
        if (t == NULL)
            t = "text/plain";
        if (add_auth_cookie_flag && realm && uname && pwd) {
            /* If authorization is required and passed */
            add_auth_user_passwd(&f->url, qstr_unquote(realm)->ptr, uname, pwd,
                0);
            add_auth_cookie_flag = 0;
        }
        if ((p = http_response_get(&t_buf->http_response, "WWW-Authenticate:")) != NULL && t_buf->http_response.status_code == 401) {
            /* Authentication needed */
            struct http_auth hauth;
            if (findAuthentication(&t_buf->http_response, &hauth, "WWW-Authenticate:") != NULL
                && (realm = get_auth_param(hauth.param, "realm")) != NULL) {
                auth_pu = &f->url;
                getAuthCookie(args, &hauth, "Authorization:", extra_header,
                    auth_pu, &m->req, m->req.post, &uname, &pwd);
                if (uname == NULL) {
                    /* abort */
                    // TRAP_OFF;
                    return page_loaded(args, page, charset, f->url, t, real_type, t_buf, *f, http.flag);
                }
                UFclose(f);
                add_auth_cookie_flag = 1;
                m->transport_status = HTST_NORMAL;
                return page_loaded(args, page, charset, f->url, t, real_type, t_buf, *f, http.flag);
            }
        }
        if ((p = http_response_get(&t_buf->http_response, "Proxy-Authenticate:")) != NULL && t_buf->http_response.status_code == 407) {
            /* Authentication needed */
            struct http_auth hauth;
            if (findAuthentication(&t_buf->http_response, &hauth, "Proxy-Authenticate:")
                    != NULL
                && (realm = get_auth_param(hauth.param, "realm")) != NULL) {
                auth_pu = schemeToProxy(f->url.scheme);
                getAuthCookie(args, &hauth, "Proxy-Authorization:",
                    extra_header, auth_pu, &m->req, m->req.post,
                    &uname, &pwd);
                if (uname == NULL) {
                    /* abort */
                    // TRAP_OFF;
                    return page_loaded(args, page, charset, f->url, t, real_type, t_buf, *f, http.flag);
                }
                UFclose(f);
                add_auth_cookie_flag = 1;
                m->transport_status = HTST_NORMAL;
                add_auth_user_passwd(auth_pu, qstr_unquote(realm)->ptr, uname, pwd, 1);
                goto load_doc;
            }
        }
        /* XXX: RFC2617 3.2.3 Authentication-Info: ? */

        if (m->transport_status == HTST_CONNECT) {
            // of = &f;
            goto load_doc;
        }

        f->modtime = mymktime(http_response_get(&t_buf->http_response, "Last-Modified:"));
    } else if (searchHeader) {
        searchHeader = SearchHeader = FALSE;
        if (t_buf == NULL)
            t_buf = newBuffer(INIT_BUFFER_WIDTH);
        t_buf->http_response = http_response_header(f->stream, f->url.scheme);
        if (searchHeader_through && !t_buf->header_source) {
            t_buf->header_source = http_response_save_header_source(&t_buf->http_response);
        }
        f->compression = http_response_process(&t_buf->http_response, args, &f->url);
        if (f->is_cgi && (p = http_response_get(&t_buf->http_response, "Location:")) != NULL) {
            /* document moved */
            enum HttpReidrectionStatus redirection = http_redirect(&http, url_encode(remove_space(p), NULL, 0), NULL, referer);
            if (redirection != HTTP_REDIRECTION_OK) {
                return 0;
            }

            tpath = url_encode(remove_space(p), NULL, 0);
            // http.post = NULL;
            UFclose(f);
            add_auth_cookie_flag = 0;
            // http.current = New(struct Url);
            // copyParsedURL(http.current, &f->url);
            t_buf = newBuffer(INIT_BUFFER_WIDTH);
            t_buf->bufferprop |= BP_REDIRECTED;
            m->transport_status = HTST_NORMAL;
            goto load_doc;
        }
        t = http_response_get_content_type(&t_buf->http_response, &content_charset);
        if (t == NULL)
            t = "text/plain";
    } else if (DefaultType) {
        t = DefaultType;
        DefaultType = NULL;
    } else {
        t = guessContentType(f->url.file);
        if (t == NULL)
            t = "text/plain";
        real_type = t;
        if (f->guess_type)
            t = f->guess_type;
    }

    /* XXX: can we use guess_type to give the type to loadHTMLstream
     *      to support default utf8 encoding for XHTML here? */
    f->guess_type = (char*)t;

    return page_loaded(args, page, charset, f->url, t, real_type, t_buf, *f, http.flag);
}

extern char* NullLine;
extern Lineprop NullProp[];

/*
 * loadHTMLBuffer: read file and make new buffer
 */
struct Buffer*
loadHTMLBuffer(struct CmdArgs* args, struct URLFile* f, struct Buffer* newBuf)
{
    if (newBuf == NULL)
        newBuf = newBuffer(INIT_BUFFER_WIDTH);

    FILE* src = NULL;
    if (newBuf->sourcefile == NULL && (f->url.scheme != SCM_FILE || newBuf->mailcap)) {
        const char* tmpf = tmpfname(TMPF_SRC, ".html");
        src = fopen(tmpf, "w");
        if (src)
            newBuf->sourcefile = tmpf;
    }

    loadHTMLstream(f, newBuf, src, newBuf->bufferprop & BP_FRAME);

    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;
    if (n_textarea)
        formResetBuffer(newBuf, newBuf->formitem);
    if (src)
        fclose(src);

    return newBuf;
}

void init_henv(struct html_feed_environ* h_env, struct readbuffer* obuf,
    struct environment* envs, int nenv, TextLineList* buf,
    int limit, int indent)
{
    envs[0].indent = indent;

    obuf->line = Strnew();
    obuf->cprop = 0;
    obuf->pos = 0;
    obuf->prevchar = Strnew_size(8);
    set_space_to_prevchar(obuf->prevchar);
    obuf->flag = RB_IGNORE_P;
    obuf->flag_sp = 0;
    obuf->status = R_ST_NORMAL;
    obuf->table_level = -1;
    obuf->nobr_level = 0;
    obuf->q_level = 0;
    memset((void*)&obuf->anchor, 0, sizeof(obuf->anchor));
    obuf->img_alt = 0;
    obuf->input_alt.hseq = 0;
    obuf->input_alt.fid = -1;
    obuf->input_alt.in = 0;
    obuf->input_alt.type = NULL;
    obuf->input_alt.name = NULL;
    obuf->input_alt.value = NULL;
    obuf->in_bold = 0;
    obuf->in_italic = 0;
    obuf->in_under = 0;
    obuf->in_strike = 0;
    obuf->in_ins = 0;
    obuf->prev_ctype = PC_ASCII;
    obuf->tag_sp = 0;
    obuf->fontstat_sp = 0;
    obuf->top_margin = 0;
    obuf->bottom_margin = 0;
    obuf->bp.init_flag = 1;
    set_breakpoint(obuf, 0);

    h_env->buf = buf;
    h_env->f = NULL;
    h_env->obuf = obuf;
    h_env->tagbuf = Strnew();
    h_env->limit = limit;
    h_env->maxlimit = 0;
    h_env->envs = envs;
    h_env->nenv = nenv;
    h_env->envc = 0;
    h_env->envc_real = 0;
    h_env->title = NULL;
    h_env->blank_lines = 0;
}

/*
 * loadHTMLString: read string and make new buffer
 */
struct Buffer*
loadHTMLString(Str page)
{
    // SignalFunc prevtrap = NULL;
    struct Buffer* newBuf;

    struct URLFile f = init_stream(SCM_FILE, ist_from_buffer(page->ptr, page->length));

    newBuf = newBuffer(INIT_BUFFER_WIDTH);
    // if (SETJMP(AbortLoading) != 0) {
    //     TRAP_OFF;
    //     discardBuffer(newBuf);
    //     UFclose(&f);
    //     return NULL;
    // }
    // TRAP_ON;

    newBuf->document_charset = InnerCharset;
    loadHTMLstream(&f, newBuf, NULL, TRUE);
    newBuf->document_charset = WC_CES_US_ASCII;

    // TRAP_OFF;
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
struct Buffer*
loadBuffer(struct CmdArgs* args, struct URLFile* uf, struct Buffer* volatile newBuf)
{
    FILE* volatile src = NULL;
    wc_ces charset = WC_CES_US_ASCII;
    wc_ces volatile doc_charset = DocumentCharset;
    Str lineBuf2;
    volatile char pre_lbuf = '\0';
    int nlines;
    const char* tmpf;
    int64_t linelen = 0, trbyte = 0;
    Lineprop* propBuffer = NULL;
    Linecolor* colorBuffer = NULL;
    // SignalFunc prevtrap = NULL;

    if (newBuf == NULL)
        newBuf = newBuffer(INIT_BUFFER_WIDTH);

    // if (SETJMP(AbortLoading) != 0) {
    //     goto _end;
    // }
    // TRAP_ON;

    if (newBuf->sourcefile == NULL && (uf->url.scheme != SCM_FILE || newBuf->mailcap)) {
        tmpf = tmpfname(TMPF_SRC, NULL);
        src = fopen(tmpf, "w");
        if (src)
            newBuf->sourcefile = tmpf;
    }
    if (newBuf->document_charset)
        charset = doc_charset = newBuf->document_charset;
    if (content_charset && UseContentCharset)
        doc_charset = content_charset;

    nlines = 0;
    while (true) {
        struct str_view gv = ist_gets(uf->stream, true);
        if (gv.len == 0) {
            break;
        }
        lineBuf2 = Strnew_charp_n(gv.ptr, gv.len);
        if (src)
            Strfputs(lineBuf2, src);
        linelen += lineBuf2->length;
        showProgress(&linelen, &trbyte);
        if (frame_source)
            continue;
        lineBuf2 = convertLine((const uint8_t*)lineBuf2->ptr, lineBuf2->length, PAGER_MODE, &charset, doc_charset, false);
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
            lineBuf2->length, FOLD_BUFFER_WIDTH, nlines);
    }

_end:
    // TRAP_OFF;
    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;
    newBuf->trbyte = trbyte + linelen;
    newBuf->document_charset = charset;
    if (src)
        fclose(src);

    return newBuf;
}

struct Buffer*
loadImageBuffer(struct CmdArgs* args, struct URLFile* uf, struct Buffer* newBuf)
{
    const struct Url* pu = newBuf ? &newBuf->currentURL : NULL;

    loadImage(newBuf, IMG_FLAG_STOP);
    struct Image image = {
        .url = parsedURL2RefererStr(&uf->url)->ptr,
        .width = -1,
        .height = -1,
        .cache = NULL,
    };
    struct ImageCache* cache = getImage(&image, (struct Url*)pu, IMG_FLAG_AUTO);
    struct stat st;
    if (!(pu && pu->is_nocache) && cache->loaded & IMG_FLAG_LOADED && !stat(cache->file, &st)) {
        // goto image_buffer;
    } else {

        // SignalFunc prevtrap = NULL;

        // TRAP_ON;
        // if (!ist_save2tmp(uf->stream, uf->url.scheme, cache->file)) {
        //     TRAP_OFF;
        //     return NULL;
        // }
        // TRAP_OFF;

        cache->loaded = IMG_FLAG_LOADED;
        cache->index = 0;
    }

    if (newBuf == NULL)
        newBuf = newBuffer(INIT_BUFFER_WIDTH);
    cache->loaded |= IMG_FLAG_DONT_REMOVE;
    if (newBuf->sourcefile == NULL && uf->url.scheme != SCM_FILE)
        newBuf->sourcefile = cache->file;

    const char* tmpf = tmpfname(TMPF_SRC, ".html");
    FILE* src = fopen(tmpf, "w");
    if (src == NULL)
        return NULL;
    newBuf->mailcap_source = tmpf;

    Str tmp = Sprintf("<img src=\"%s\"><br><br>", html_quote(image.url));
    struct URLFile f = init_stream(SCM_FILE, ist_from_buffer(tmp->ptr, tmp->length));
    loadHTMLstream(&f, newBuf, src, TRUE);
    UFclose(&f);
    if (src)
        fclose(src);

    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;
    newBuf->image_flag = IMG_FLAG_AUTO;
    return newBuf;
}

static Str
conv_symbol(struct Line* l)
{
    Str tmp = NULL;
    char *p = l->lineBuf, *ep = p + l->len;
    Lineprop* pr = l->propBuf;
    int w;
    char** symbol = NULL;

    for (; p < ep; p++, pr++) {
        if (*pr & PC_SYMBOL) {
            char c = ((char)wtf_get_code((wc_uchar*)p) & 0x7f) - SYMBOL_BASE;
            int len = get_mclen(p);
            if (tmp == NULL) {
                tmp = Strnew_size(l->len);
                Strcopy_charp_n(tmp, l->lineBuf, p - l->lineBuf);
                w = (*pr & PC_KANJI) ? 2 : 1;
                symbol = get_symbol(DisplayCharset, &w);
            }
            Strcat_charp(tmp, symbol[(unsigned char)c % N_SYMBOL]);
            p += len - 1;
            pr += len - 1;
        } else if (tmp != NULL)
            Strcat_char(tmp, *p);
    }
    if (tmp)
        return tmp;
    else
        return Strnew_charp_n(l->lineBuf, l->len);
}

/*
 * saveBuffer: write buffer to file
 */
static void
_saveBuffer(struct Buffer* buf, struct Line* l, FILE* f, int cont)
{
    Str tmp;
    int is_html = FALSE;
    int set_charset = !DisplayCharset;
    wc_ces charset = DisplayCharset ? DisplayCharset : WC_CES_US_ASCII;

    is_html = is_html_type(buf->type);

pager_next:
    for (; l != NULL; l = l->next) {
        if (is_html)
            tmp = conv_symbol(l);
        else
            tmp = Strnew_charp_n(l->lineBuf, l->len);
        tmp = Strnew_wc_output(wc_Str_conv(WcOption, tmp->ptr, tmp->length, InnerCharset, charset));
        Strfputs(tmp, f);
        if (Strlastchar(tmp) != '\n' && !(cont && l->next && l->next->bpos))
            putc('\n', f);
    }
    if (buf->pagerSource && !(buf->bufferprop & BP_CLOSE)) {
        l = getNextPage(buf, PagerMax);
        if (set_charset)
            charset = buf->document_charset;
        goto pager_next;
    }
}

void saveBuffer(struct Buffer* buf, FILE* f, int cont)
{
    _saveBuffer(buf, buf->firstLine, f, cont);
}

void saveBufferBody(struct Buffer* buf, FILE* f, int cont)
{
    struct Line* l = buf->firstLine;

    while (l != NULL && l->real_linenumber == 0)
        l = l->next;
    _saveBuffer(buf, l, f, cont);
}

struct Buffer*
loadcmdout(struct CmdArgs* args, const char* cmd,
    struct Buffer* (*loadproc)(struct CmdArgs* args, struct URLFile*, struct Buffer*), struct Buffer* defaultbuf)
{
    if (cmd == NULL || *cmd == '\0')
        return NULL;

    FILE* f = popen(cmd, "r");
    if (f == NULL)
        return NULL;

    struct URLFile uf = init_stream(SCM_UNKNOWN, ist_from_fp(f, pclose));
    struct Buffer* buf = loadproc(args, &uf, defaultbuf);
    UFclose(&uf);
    return buf;
}

/*
 * getshell: execute shell command and get the result into a buffer
 */
#define SHELLBUFFERNAME "*Shellout*"
struct Buffer*
getshell(struct CmdArgs* args, const char* cmd)
{
    struct Buffer* buf = loadcmdout(args, cmd, loadBuffer, NULL);
    if (buf == NULL)
        return NULL;
    buf->filename = cmd;
    buf->buffername = Sprintf("%s %s", SHELLBUFFERNAME,
        conv_from_system(cmd))
                          ->ptr;
    return buf;
}

/*
 * getpipe: execute shell command and connect pipe to the buffer
 */
struct Buffer*
getpipe(const char* cmd)
{
    FILE *f, *popen(const char*, const char*);
    struct Buffer* buf;

    if (cmd == NULL || *cmd == '\0')
        return NULL;
    f = popen(cmd, "r");
    if (f == NULL)
        return NULL;
    buf = newBuffer(INIT_BUFFER_WIDTH);
    buf->pagerSource = ist_from_fp(f, pclose);
    buf->filename = cmd;
    buf->buffername = Sprintf("%s %s", PIPEBUFFERNAME,
        conv_from_system(cmd))
                          ->ptr;
    buf->bufferprop |= BP_PIPE;
    buf->document_charset = WC_CES_US_ASCII;
    return buf;
}

#define CPIPEBUFFERNAME "*stream(closed)*"
struct Line* getNextPage(struct Buffer* buf, int plen)
{
    struct Line* volatile top = buf->topLine, * volatile last = buf->lastLine, * volatile cur = buf->currentLine;
    int i;
    int volatile nlines = 0;
    int64_t linelen = 0, trbyte = buf->trbyte;
    Str lineBuf2;
    char volatile pre_lbuf = '\0';
    struct URLFile uf;
    wc_ces charset;
    wc_ces volatile doc_charset = DocumentCharset;
    wc_uint8 old_auto_detect = WcOption.auto_detect;
    int volatile squeeze_flag = FALSE;
    Lineprop* propBuffer = NULL;

    Linecolor* colorBuffer = NULL;
    // SignalFunc prevtrap = NULL;

    if (buf->pagerSource == NULL)
        return NULL;

    if (last != NULL) {
        nlines = last->real_linenumber;
        pre_lbuf = *(last->lineBuf);
        if (pre_lbuf == '\0')
            pre_lbuf = '\n';
        buf->currentLine = last;
    }

    charset = buf->document_charset;
    if (buf->document_charset != WC_CES_US_ASCII)
        doc_charset = buf->document_charset;
    else if (UseContentCharset) {
        content_charset = 0;
        http_response_get_content_type(&buf->http_response, &content_charset);
        if (content_charset)
            doc_charset = content_charset;
    }
    WcOption.auto_detect = buf->auto_detect;

    // if (SETJMP(AbortLoading) != 0) {
    //     goto pager_end;
    // }
    // TRAP_ON;

    uf = init_stream(SCM_UNKNOWN, NULL);
    for (i = 0; i < plen; i++) {
        struct str_view gv = ist_gets(buf->pagerSource, true);
        if (gv.len == 0)
            return NULL;
        lineBuf2 = Strnew_charp_n(gv.ptr, gv.len);
        if (lineBuf2->length == 0) {
            /* Assume that `cmd == buf->filename' */
            if (buf->filename)
                buf->buffername = Sprintf("%s %s",
                    CPIPEBUFFERNAME,
                    conv_from_system(buf->filename))
                                      ->ptr;
            else if (getenv("MAN_PN") == NULL)
                buf->buffername = CPIPEBUFFERNAME;
            buf->bufferprop |= BP_CLOSE;
            break;
        }
        linelen += lineBuf2->length;
        showProgress(&linelen, &trbyte);
        lineBuf2 = convertLine((const uint8_t*)lineBuf2->ptr, lineBuf2->length, PAGER_MODE, &charset, doc_charset, false);
        if (squeezeBlankLine) {
            squeeze_flag = FALSE;
            if (lineBuf2->ptr[0] == '\n' && pre_lbuf == '\n') {
                ++nlines;
                --i;
                squeeze_flag = TRUE;
                continue;
            }
            pre_lbuf = lineBuf2->ptr[0];
        }
        ++nlines;
        Strchop(lineBuf2);
        lineBuf2 = checkType(lineBuf2, &propBuffer, &colorBuffer);
        addnewline(buf, lineBuf2->ptr, propBuffer, colorBuffer,
            lineBuf2->length, FOLD_BUFFER_WIDTH, nlines);
        if (!top) {
            top = buf->firstLine;
            cur = top;
        }
        if (buf->lastLine->real_linenumber - buf->firstLine->real_linenumber
            >= PagerMax) {
            struct Line* l = buf->firstLine;
            do {
                if (top == l)
                    top = l->next;
                if (cur == l)
                    cur = l->next;
                if (last == l)
                    last = NULL;
                l = l->next;
            } while (l && l->bpos);
            buf->firstLine = l;
            if (l)
                buf->firstLine->prev = NULL;
        }
    }
pager_end:
    // TRAP_OFF;

    buf->trbyte = trbyte + linelen;
    buf->document_charset = charset;
    WcOption.auto_detect = old_auto_detect;
    buf->topLine = top;
    buf->currentLine = cur;
    if (!last)
        last = buf->firstLine;
    else if (last && (last->next || !squeeze_flag))
        last = last->next;
    return last;
}

struct Buffer*
doExternal(struct CmdArgs* args, struct URLFile uf, const char* type, struct Buffer* defaultbuf)
{
    Str command;
    struct mailcap* mcap;
    int mc_stat;
    struct Buffer* buf = NULL;
    const char *header, *src = NULL;
    const char* ext = NULL;

    if (!(mcap = searchExtViewer(type)))
        return NULL;

    if (mcap->nametemplate) {
        Str tmpf = unquote_mailcap(mcap->nametemplate, NULL, "", NULL, NULL);
        if (tmpf->ptr[0] == '.')
            ext = tmpf->ptr;
    }
    const char* tmpf = tmpfname(TMPF_DFL, (ext && *ext) ? ext : NULL);

    header = http_response_get(&defaultbuf->http_response, "Content-Type:");
    if (header)
        header = conv_to_system(header);
    command = unquote_mailcap(mcap->viewer, type, tmpf, header, &mc_stat);
    if (!(mc_stat & MCSTAT_REPNAME)) {
        Str tmp = Sprintf("(%s) < %s", command->ptr, shell_quote(tmpf));
        command = tmp;
    }

    if (!(mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)) && !(mcap->flags & MAILCAP_NEEDSTERMINAL) && BackgroundExtViewer) {
        flush_tty();
        if (!fork()) {
            setup_child(FALSE, 0, ist_fd(uf.stream));
            if (!ist_save2tmp(uf.stream, uf.url.scheme, tmpf))
                exit(1);
            UFclose(&uf);
            myExec(command->ptr);
        }
        return NO_BUFFER;
    } else {
        if (!ist_save2tmp(uf.stream, uf.url.scheme, tmpf)) {
            return NULL;
        }
    }
    if (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)) {
        if (defaultbuf == NULL)
            defaultbuf = newBuffer(INIT_BUFFER_WIDTH);
        if (defaultbuf->sourcefile)
            src = defaultbuf->sourcefile;
        else
            src = tmpf;
        defaultbuf->sourcefile = NULL;
        defaultbuf->mailcap = mcap;
    }
    if (mcap->flags & MAILCAP_HTMLOUTPUT) {
        buf = loadcmdout(args, command->ptr, loadHTMLBuffer, defaultbuf);
        if (buf && buf != NO_BUFFER) {
            buf->type = "text/html";
            buf->mailcap_source = buf->sourcefile;
            buf->sourcefile = src;
        }
    } else if (mcap->flags & MAILCAP_COPIOUSOUTPUT) {
        buf = loadcmdout(args, command->ptr, loadBuffer, defaultbuf);
        if (buf && buf != NO_BUFFER) {
            buf->type = "text/plain";
            buf->mailcap_source = buf->sourcefile;
            buf->sourcefile = src;
        }
    } else {
        if (mcap->flags & MAILCAP_NEEDSTERMINAL || !BackgroundExtViewer) {
            fmTerm();
            mySystem(command->ptr, 0);
            fmInit();
            if (CurrentTab && Currentbuf)
                displayBuffer(args, B_FORCE_REDRAW);
        } else {
            mySystem(command->ptr, 1);
        }
        buf = NO_BUFFER;
    }
    if (buf && buf != NO_BUFFER) {
        if ((buf->buffername == NULL || buf->buffername[0] == '\0') && buf->filename)
            buf->buffername = conv_from_system(lastFileName(buf->filename));
        buf->edit = mcap->edit;
        buf->mailcap = mcap;
    }
    return buf;
}

static bool checkCopyFile(const char* path1, const char* path2)
{
    if (*path2 == '|' && PermitSaveToPipe)
        return true;
    struct stat st1, st2;
    if ((stat(path1, &st1) == 0) && (stat(path2, &st2) == 0))
        if (st1.st_ino == st2.st_ino)
            return false;
    return true;
}

bool doFileCopy(struct CmdArgs* args, const char* tmpf, const char* defstr)
{
    Str msg;
    Str filen;
    const char *p, *q = NULL;
    pid_t pid;
    const char* lock;
    struct stat st;
    int64_t size = 0;
    int is_pipe = FALSE;

    if (fmInitialized) {
        p = searchKeyData();
        if (p == NULL || *p == '\0') {
            /* FIXME: gettextize? */
            q = inputLineHist(args, "(Download)Save file to: ", defstr, IN_COMMAND, HistorySave);
            if (q == NULL || *q == '\0')
                return FALSE;
            p = conv_to_system(q);
        }
        if (*p == '|' && PermitSaveToPipe)
            is_pipe = TRUE;
        else {
            if (q) {
                p = unescape_spaces(Strnew_charp(q))->ptr;
                p = conv_to_system(p);
            }
            p = expandPath(p);
            if (!checkOverWrite(args, p))
                return false;
        }
        if (!checkCopyFile(tmpf, p)) {
            /* FIXME: gettextize? */
            msg = Sprintf("Can't copy. %s and %s are identical.",
                conv_from_system(tmpf), conv_from_system(p));
            disp_err_message(args, msg->ptr, FALSE);
            return false;
        }
        {
            if (!MoveFile(tmpf, p)) {
                /* FIXME: gettextize? */
                msg = Sprintf("Can't save to %s", conv_from_system(p));
                disp_err_message(args, msg->ptr, FALSE);
            }
            return false;
        }
        lock = tmpfname(TMPF_DFL, ".lock");
        symlink(p, lock);
        flush_tty();
        pid = fork();
        if (!pid) {
            setup_child(FALSE, 0, -1);
            if (MoveFile(tmpf, p) && PreserveTimestamp && !is_pipe && !stat(tmpf, &st))
                setModtime(p, st.st_mtime);
            unlink(lock);
            exit(0);
        }
        if (!stat(tmpf, &st))
            size = st.st_size;
        addDownloadList(pid, conv_from_system(tmpf), p, lock, size);
    } else {
        q = searchKeyData();
        if (q == NULL || *q == '\0') {
            /* FIXME: gettextize? */
            printf("(Download)Save file to: ");
            fflush(stdout);
            filen = Strfgets(stdin);
            if (filen->length == 0)
                return false;
            q = filen->ptr;
        }
        for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
            ;
        ((char*)p)[1] = '\0';
        if (*q == '\0')
            return false;
        p = q;
        if (*p == '|' && PermitSaveToPipe)
            is_pipe = TRUE;
        else {
            p = expandPath(p);
            if (!checkOverWrite(args, p))
                return false;
        }
        if (!checkCopyFile(tmpf, p)) {
            /* FIXME: gettextize? */
            printf("Can't copy. %s and %s are identical.", tmpf, p);
            return false;
        }
        if (!MoveFile(tmpf, p)) {
            /* FIXME: gettextize? */
            printf("Can't save to %s\n", p);
            return false;
        }
        if (PreserveTimestamp && !is_pipe && !stat(tmpf, &st))
            setModtime(p, st.st_mtime);
    }
    return true;
}
