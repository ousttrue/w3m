#include "input_stream.h"
#include "fileutil.h"
#include "mysignal.h"
#include "textlist.h"
#include "tcp_socket.h"
#include "file.h"
#include "local_cgi.h"
#include "html_form.h"
#include "indep.h"
#include "etc.h"
#include "ftp.h"
#include "compression.h"
#include "growbuf.h"
#include "alloc.h"
#include "w3m_rc.h"
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <openssl/ssl.h>

enum InputStreamType {
    IST_BASIC = 0,
    IST_FILE = 1,
    IST_STR = 2,
    IST_SSL = 3,
};

struct stream_buffer {
    uint8_t* buf;
    int size;
    int cur;
    int next;
};

inline static bool MUST_BE_UPDATED(struct stream_buffer* sb)
{
    return (sb->cur == sb->next);
}

struct io_file_handle {
    FILE* f;
    FileCloseFunc close;
};

struct input_stream {
    enum InputStreamType type;
    bool iseos;
    bool unclose;
    struct stream_buffer sb;
    union {
        int base;
        struct io_file_handle file;
        struct ssl_handle ssl;
    };
};

bool is_isend(struct input_stream* is)
{
    return is->iseos;
}

void is_set_unclose(struct input_stream* is, bool unclose)
{
    is->unclose = unclose;
}

#define STREAM_BUF_SIZE 8192

static void sb_init(struct stream_buffer* sb, const uint8_t* init, int init_size)
{
    *sb = (struct stream_buffer) {
        .buf = NewWithoutGC_N(uint8_t, init_size),
        .size = init_size,
        .cur = 0,
        .next = 0,
    };
    if (init) {
        memcpy(sb->buf, init, init_size);
        sb->next = init_size;
    }
}

static int raw_read(struct input_stream* is, uint8_t* p, size_t len)
{
    switch (is->type) {
    case IST_BASIC:
        return read(is->base, p, len);
    case IST_FILE:
        return fread(p, 1, len, is->file.f);
    case IST_STR:
        return 0;
    case IST_SSL:
        return ssl_read(&is->ssl, (char*)p, len);
    default:
        return -1;
    }
}

static void
do_update(struct input_stream* is)
{
    is->sb.cur = is->sb.next = 0;
    int len = raw_read(is, is->sb.buf, is->sb.size);
    if (len <= 0)
        is->iseos = true;
    else
        is->sb.next += len;
}

static int
buffer_read(struct stream_buffer* sb, char* obuf, int count)
{
    int len = sb->next - sb->cur;
    if (len > 0) {
        if (len > count)
            len = count;
        bcopy((const void*)&sb->buf[sb->cur], obuf, len);
        sb->cur += len;
    }
    return len;
}

struct input_stream* is_from_fd(int fd)
{
    if (fd < 0)
        return NULL;
    struct input_stream* is = NewWithoutGC(struct input_stream);
    *is = (struct input_stream) {
        .type = IST_BASIC,
        .iseos = false,
        .unclose = false,
        .base = fd,
    };
    sb_init(&is->sb, NULL, STREAM_BUF_SIZE);
    return is;
}

struct input_stream* is_from_file(FILE* f, FileCloseFunc closep)
{
    if (f == NULL)
        return NULL;
    struct input_stream* is = NewWithoutGC(struct input_stream);
    *is = (struct input_stream) {
        .type = IST_FILE,
        .iseos = false,
        .unclose = false,
        .file = (struct io_file_handle) {
            .f = f,
            .close = closep ? closep : fclose,
        },
    };
    sb_init(&is->sb, NULL, STREAM_BUF_SIZE);
    return is;
}

struct input_stream* is_from_str(Str s)
{
    if (s == NULL)
        return NULL;
    struct input_stream* is = NewWithoutGC(struct input_stream);
    *is = (struct input_stream) {
        .type = IST_STR,
        .iseos = false,
        .unclose = false,
    };
    sb_init(&is->sb, (const uint8_t*)s->ptr, s->length);
    return is;
}

#define SSL_BUF_SIZE 1536

struct input_stream* is_from_ssl(SSL* ssl, int sock)
{
    if (sock < 0)
        return NULL;

    struct input_stream* is = NewWithoutGC(struct input_stream);
    *is = (struct input_stream) {
        .type = IST_SSL,
        .iseos = false,
        .unclose = false,
        .ssl = (struct ssl_handle) {
            .sock = sock,
            .ssl = ssl,
        },
    };
    sb_init(&is->sb, NULL, SSL_BUF_SIZE);
    return is;
}

int is_close(struct input_stream* is)
{
    if (is == NULL)
        return -1;

    if (is->unclose) {
        return -1;
    }

    void (*prevtrap)(int);
    prevtrap = mySignal(SIGINT, SIG_IGN);
    switch (is->type) {
    case IST_BASIC:
        close(is->base);
        break;
    case IST_FILE:
        is->file.close(is->file.f);
        break;
    case IST_STR:
        break;
    case IST_SSL:
        ssl_close(&is->ssl);
        break;
    default:
        assert(false);
        break;
    }
    mySignal(SIGINT, prevtrap);

    xfree(is->sb.buf);
    xfree(is);
    return 0;
}

int is_getc(struct input_stream* is)
{
    if (is == NULL)
        return 0;

    if (!is->iseos && MUST_BE_UPDATED(&is->sb))
        do_update(is);

    // #define POP_CHAR(bs) ((bs)->iseos ? '\0' : (bs)->is.buf[(bs)->is.cur++])
    if (is->iseos) {
        return 0;
    }
    return is->sb.buf[is->sb.cur++];
}

int is_undo_getc(struct input_stream* is)
{
    if (is == NULL)
        return -1;
    if (is->sb.cur > 0) {
        is->sb.cur--;
        return 0;
    }
    return -1;
}

struct growbuf;
static void is_to_growbuf(struct input_stream* is, struct growbuf* gb, bool crnl)
{
    // struct base_stream* base = &is->base;
    struct stream_buffer* sb = &is->sb;
    gb->length = 0;
    while (!is->iseos) {
        if (MUST_BE_UPDATED(sb)) {
            do_update(is);
            continue;
        }
        if (crnl && gb->length > 0 && gb->ptr[gb->length - 1] == '\r') {
            if (sb->buf[sb->cur] == '\n') {
                GROWBUF_ADD_CHAR(gb, '\n');
                ++sb->cur;
            }
            break;
        }
        int i = sb->cur;
        for (; i < sb->next; ++i) {
            if (sb->buf[i] == '\n' || (crnl && sb->buf[i] == '\r')) {
                ++i;
                break;
            }
        }
        growbuf_append(gb, &sb->buf[sb->cur], i - sb->cur);
        sb->cur = i;
        if (gb->length > 0 && gb->ptr[gb->length - 1] == '\n')
            break;
    }

    growbuf_reserve(gb, gb->length + 1);
    gb->ptr[gb->length] = '\0';
}

Str is_get_str(struct input_stream* is, bool crnl)
{
    if (is == NULL)
        return NULL;

    struct growbuf gb;
    growbuf_init(&gb);
    is_to_growbuf(is, &gb, crnl);
    return growbuf_to_Str(&gb);
}

int is_read(struct input_stream* is, char* dst, int count)
{
    if (is == NULL || count <= 0)
        return -1;

    if (is->iseos)
        return 0;

    int len = buffer_read(&is->sb, dst, count);
    if (MUST_BE_UPDATED(&is->sb)) {
        int l = raw_read(is, (uint8_t*)&dst[len], count - len);
        if (l <= 0) {
            is->iseos = true;
        } else {
            len += l;
        }
    }
    return len;
}

int is_file_no(struct input_stream* is)
{
    if (is == NULL)
        return -1;
    switch (is->type) {
    case IST_BASIC:
        return is->base;
    case IST_FILE:
        return fileno(is->file.f);
    case IST_SSL:
        return is->ssl.sock;
    default:
        return -1;
    }
}

#define NOT_REGULAR(m) (((m) & S_IFMT) != S_IFREG)

struct input_stream* examineFile(const char* path)
{
    if (path == NULL || *path == '\0') {
        return NULL;
    }

    struct stat stbuf;
    if (stat(path, &stbuf) != 0) {
        return NULL;
    }
    if (NOT_REGULAR(stbuf.st_mode)) {
        return NULL;
    }

    struct input_stream* stream = is_from_fd(open(path, O_RDONLY));
    return stream;
}

struct input_stream* decompress_stream(struct input_stream* stream, const char* path)
{
    struct CompressionDecoder* d = compression_from_path(path);
    if (!d) {
        return stream;
    }
    return uncompress_stream(stream, d->type, NULL);
}

bool is_save2tmp(struct input_stream* stream, const char* tmpf)
{
    FILE* ff = fopen(tmpf, "wb");
    if (ff == NULL) {
        return false;
    }
    static JMP_BUF env_bak;
    static JMP_BUF AbortLoading;
    memcpy(env_bak, AbortLoading, sizeof(JMP_BUF));
    if (SETJMP(AbortLoading) != 0) {
        goto _end;
    }

    int retval = 0;
    int64_t linelen = 0;
    int64_t trbyte = 0;
    MySignalHandler (*volatile prevtrap)(SIGNAL_ARG) = NULL;
    char* buf = NULL;
    TRAP_ON;
    {
        buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
        int count;
        while ((count = is_read(stream, buf, SAVE_BUF_SIZE)) > 0) {
            if (fwrite(buf, 1, count, ff) != count) {
                retval = -2;
                goto _end;
            }
            linelen += count;
            showProgress(&linelen, &trbyte, 0);
        }
    }
_end:
    bcopy(env_bak, AbortLoading, sizeof(JMP_BUF));
    TRAP_OFF;
    xfree(buf);
    fclose(ff);
    return retval;
}

void UFhalfclose(struct input_stream* stream, enum UrlScheme scheme)
{
    switch (scheme) {
    case SCM_FTP:
        closeFTP();
        break;
    default:
        is_close(stream);
        break;
    }
}

/* add index_file if exists */
static struct input_stream*
add_index_file(struct Url* pu, struct input_stream* stream)
{
    char *p, *q;
    struct TextList* index_file_list = NULL;
    TextListItem* ti;

    if (non_null(getRuntime()->index_file))
        index_file_list = make_domain_list(getRuntime()->index_file);
    if (index_file_list == NULL) {
        stream = NULL;
    } else {
        for (ti = index_file_list->first; ti; ti = ti->next) {
            p = Strnew_m_charp(pu->file, "/", file_quote(ti->ptr), NULL)->ptr;
            p = cleanupName(p);
            q = cleanupName(file_unquote(p));
            stream = decompress_stream(examineFile(q), q);
            if (stream != NULL) {
                pu->file = p;
                pu->real_file = q;
                break;
            }
        }
    }
    return stream;
}

static void
write_from_file(int sock, char* file)
{
    FILE* fd;
    int c;
    char buf[1];
    fd = fopen(file, "r");
    if (fd != NULL) {
        while ((c = fgetc(fd)) != EOF) {
            buf[0] = c;
            write(sock, buf, 1);
        }
        fclose(fd);
    }
}

struct ContentAndStream openURL(struct Url url, struct FormList* request,
    struct LoadOption option, struct input_stream* ouf)
{
    struct ContentAndStream us = {
        .content = New(struct Content),
        .stream = ouf,
    };
    *us.content = (struct Content) {
        .url = url,
        .hr = (struct HttpRequest) {
            .command = HR_COMMAND_GET,
            .flag = 0,
            .referer = option.referer,
            .request = request,
        },
        .content_type = "text/plain",
        0,
    };

    if (us.content->url.scheme == SCM_LOCAL && !us.content->url.file) {
        if (us.content->url.label) {
            /* #hogege is not a label but a filename */
            Str tmp2 = Strnew_charp("#");
            Strcat_charp(tmp2, us.content->url.label);
            us.content->url.file = tmp2->ptr;
            us.content->url.real_file = cleanupName(file_unquote(us.content->url.file));
            us.content->url.label = NULL;
        } else {
            /* given URL must be null string */
            return us;
        }
    }

    if (getRuntime()->LocalhostOnly && us.content->url.host && !is_localhost(us.content->url.host))
        us.content->url.host = NULL;

    us.content->url_str = parsedURL2Str(&us.content->url)->ptr;
    us.content->url.is_nocache = (option.flag & RG_NOCACHE);
    // uf.ext = filename_extension(pu->file, 1);

    Str tmp;
    int sock;
    char *p, *q;
    SSL* sslh = NULL;

    switch (us.content->url.scheme) {
    case SCM_LOCAL:
    case SCM_LOCAL_CGI:
        if (request && request->body)
            /* local CGI: POST */
            us.stream = is_from_file(
                localcgi_post(us.content->url.real_file, us.content->url.query,
                    request, option.referer),
                fclose);
        else
            /* lodal CGI: GET */
            us.stream = is_from_file(
                localcgi_get(us.content->url.real_file, us.content->url.query,
                    option.referer),
                fclose);
        if (us.stream) {
            us.content->url.scheme = SCM_LOCAL_CGI;
            us.content->is_cgi = true;
            return us;
        }
        us.stream = decompress_stream(examineFile(us.content->url.real_file), us.content->url.real_file);
        if (us.stream == NULL) {
            if (dir_exist(us.content->url.real_file)) {
                us.stream = add_index_file(&us.content->url, us.stream);
                if (us.stream == NULL) {
                    return us;
                }
            } else if (getRuntime()->document_root != NULL) {
                tmp = Strnew_charp(getRuntime()->document_root);
                if (Strlastchar(tmp) != '/' && us.content->url.file[0] != '/')
                    Strcat_char(tmp, '/');
                Strcat_charp(tmp, us.content->url.file);
                p = cleanupName(tmp->ptr);
                q = cleanupName(file_unquote(p));
                if (dir_exist(q)) {
                    us.content->url.file = p;
                    us.content->url.real_file = q;
                    us.stream = add_index_file(&us.content->url, us.stream);
                    if (us.stream == NULL) {
                        return us;
                    }
                } else {
                    us.stream = examineFile(q);
                    us.stream = decompress_stream(us.stream, q);
                    if (us.stream) {
                        us.content->url.file = p;
                        us.content->url.real_file = q;
                    }
                }
            }
        }
        return us;
    case SCM_FTP:
    case SCM_FTPDIR:
        if (us.content->url.file == NULL)
            us.content->url.file = allocStr("/", -1);
        if (non_null(getRuntime()->FTP_proxy) && getRuntime()->use_proxy && us.content->url.host != NULL && !check_no_proxy(us.content->url.host)) {
            us.content->hr.flag |= HR_FLAG_PROXY;
            sock = tcp_open(FTP_proxy_parsed.host,
                schemeNumToName(FTP_proxy_parsed.scheme),
                FTP_proxy_parsed.port);
            if (sock < 0) {
                return us;
            }
            us.content->url.scheme = SCM_HTTP;
            tmp = HTTPrequest(&us.content->url, option.base_url, &us.content->hr, option.extra_header);
            write(sock, tmp->ptr, tmp->length);
        } else {
            struct FtpFile file = openFTPStream(&us.content->url);
            us.stream = file.is;
            us.content->modtime = file.modtime;
            return us;
        }
        break;
    case SCM_HTTP:
    case SCM_HTTPS:
        if (us.content->url.file == NULL)
            us.content->url.file = allocStr("/", -1);
        if (request && request->method == FORM_METHOD_POST && request->body)
            us.content->hr.command = HR_COMMAND_POST;
        if (request && request->method == FORM_METHOD_HEAD)
            us.content->hr.command = HR_COMMAND_HEAD;
        if ((
                (us.content->url.scheme == SCM_HTTPS) ? non_null(getRuntime()->HTTPS_proxy) : non_null(getRuntime()->HTTP_proxy))
            && getRuntime()->use_proxy && us.content->url.host != NULL && !check_no_proxy(us.content->url.host)) {
            us.content->hr.flag |= HR_FLAG_PROXY;
            if (us.content->url.scheme == SCM_HTTPS && us.status == HTST_CONNECT) {
                sock = ouf->ssl.sock;

                if (!(sslh = openSSLHandle(sock, us.content->url.host,
                          &us.content->ssl_certificate))) {
                    us.status = HTST_MISSING;
                    return us;
                }
            } else if (us.content->url.scheme == SCM_HTTPS) {
                sock = tcp_open(HTTPS_proxy_parsed.host,
                    schemeNumToName(HTTPS_proxy_parsed.scheme),
                    HTTPS_proxy_parsed.port);
                sslh = NULL;
            } else {
                sock = tcp_open(HTTP_proxy_parsed.host,
                    schemeNumToName(HTTP_proxy_parsed.scheme),
                    HTTP_proxy_parsed.port);
                sslh = NULL;
            }
            if (sock < 0) {
                return us;
            }
            if (us.content->url.scheme == SCM_HTTPS) {
                if (us.status == HTST_NORMAL) {
                    us.content->hr.command = HR_COMMAND_CONNECT;
                    tmp = HTTPrequest(&us.content->url, option.base_url, &us.content->hr, option.extra_header);
                    us.status = HTST_CONNECT;
                } else {
                    us.content->hr.flag |= HR_FLAG_LOCAL;
                    tmp = HTTPrequest(&us.content->url, option.base_url, &us.content->hr, option.extra_header);
                    us.status = HTST_NORMAL;
                }
            } else {
                tmp = HTTPrequest(&us.content->url, option.base_url, &us.content->hr, option.extra_header);
                us.status = HTST_NORMAL;
            }
        } else {
            sock = tcp_open(us.content->url.host, schemeNumToName(us.content->url.scheme), us.content->url.port);
            if (sock < 0) {
                us.status = HTST_MISSING;
                return us;
            }
            if (us.content->url.scheme == SCM_HTTPS) {
                if (!(sslh = openSSLHandle(sock, us.content->url.host,
                          &us.content->ssl_certificate))) {
                    us.status = HTST_MISSING;
                    return us;
                }
            }
            us.content->hr.flag |= HR_FLAG_LOCAL;
            tmp = HTTPrequest(&us.content->url, option.base_url, &us.content->hr, option.extra_header);
            us.status = HTST_NORMAL;
        }
        if (us.content->url.scheme == SCM_HTTPS) {
            us.stream = is_from_ssl(sslh, sock);
            if (sslh)
                SSL_write(sslh, tmp->ptr, tmp->length);
            else
                write(sock, tmp->ptr, tmp->length);
            // if (w3m_reqlog) {
            //     FILE* ff = fopen(w3m_reqlog, "a");
            //     if (ff == NULL)
            //         return uf;
            //     if (sslh)
            //         fputs("HTTPS: request via SSL\n", ff);
            //     else
            //         fputs("HTTPS: request without SSL\n", ff);
            //     fwrite(tmp->ptr, sizeof(char), tmp->length, ff);
            //     fclose(ff);
            // }
            if (us.content->hr.command == HR_COMMAND_POST && request->enctype == FORM_ENCTYPE_MULTIPART) {
                if (sslh)
                    SSL_write_from_file(sslh, request->body);
                else
                    write_from_file(sock, request->body);
            }
            return us;
        } else {
            write(sock, tmp->ptr, tmp->length);
            // if (w3m_reqlog) {
            //     FILE* ff = fopen(w3m_reqlog, "a");
            //     if (ff == NULL)
            //         return uf;
            //     fwrite(tmp->ptr, sizeof(char), tmp->length, ff);
            //     fclose(ff);
            // }
            if (us.content->hr.command == HR_COMMAND_POST && request->enctype == FORM_ENCTYPE_MULTIPART)
                write_from_file(sock, request->body);
        }
        break;
    case SCM_UNKNOWN:
    default:
        return us;
    }

    us.stream = is_from_fd(sock);
    return us;
}

void is_readall_to_file(struct input_stream* stream, FILE* src)
{
    for (Str lineBuf2 = is_get_str(stream, false);
        lineBuf2 && lineBuf2->length;
        lineBuf2 = is_get_str(stream, false)) {
        Strfputs(lineBuf2, src);
    }
    is_close(stream);
}

Str is_readall(struct input_stream* stream)
{
    Str s = Strnew();
    for (Str lineBuf2 = is_get_str(stream, false);
        lineBuf2 && lineBuf2->length;
        lineBuf2 = is_get_str(stream, false)) {
        Strcat_charp_n(s, lineBuf2->ptr, lineBuf2->length);
    }
    is_close(stream);
    return s;
}
