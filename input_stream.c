#include "input_stream.h"
#include "StreamBuffer.h"
#include "growbuf.h"
#include "mimehead.h"

#include <signal.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <openssl/ssl.h>

struct io_file_handle {
    FILE* f;
    /// fclose or pclose
    int (*close)(FILE*);
};

struct ssl_handle {
    struct ssl_st* ssl;
    int sock;
};

struct encoded_stream_handle {
    struct InputStream* is;
    struct growbuf* gb;
    int pos;
    enum StreamEncoding encoding;
};

union input_handle {
    int fd;
    struct io_file_handle file;
    struct ssl_handle ssl;
    struct encoded_stream_handle ens;
};

struct InputStream {
    enum InputStreamType type;
    struct StreamBuffer stream;
    bool iseos;
    bool unclose;
    union input_handle handle;
};

static bool MUST_BE_UPDATED(struct StreamBuffer* b)
{
    return b->cur == b->next;
}

bool ist_drain(struct InputStream* ist)
{
    if (ist->iseos) {
        return false;
    }
    if (!MUST_BE_UPDATED(&ist->stream)) {
        return false;
    }

    ist->stream.cur = ist->stream.next = 0;
    int len = ist_read(ist, ist->stream.buf, ist->stream.size);
    if (len <= 0)
        ist->iseos = true;
    else
        ist->stream.next += len;

    return true;
}

static struct span ist_buffered_until(struct InputStream* ist, const char* needles)
{
    struct StreamBuffer* sb = &ist->stream;
    bool found = false;
    int i = sb->cur;
    for (; !found && i < sb->next; ++i) {
        for (const char* p = needles; *p; ++p) {
            if (sb->buf[i] == *p) {
                found = true;
            }
        }
    }
    struct span span = {
        .ptr = sb->buf + sb->cur,
        .len = i - sb->cur,
    };
    sb->cur = i;
    return span;
}

enum InputStreamType ist_type(struct InputStream* ist)
{
    return ist->type;
}

void ist_set_unclose(struct InputStream* ist, bool unclose)
{
    ist->unclose = unclose;
}

#define SSL_BUF_SIZE 1536

#define POP_CHAR(bs) ((bs)->iseos ? '\0' : (bs)->stream.buf[(bs)->stream.cur++])

static void
basic_close(union input_handle* handle)
{
    close(handle->fd);
}

static int
basic_read(union input_handle* handle, uint8_t* buf, int len)
{
    return read(handle->fd, buf, len);
}

struct InputStream*
ist_from_fd(int des)
{
    if (des < 0)
        return NULL;
    struct InputStream* ist = malloc(sizeof(struct InputStream));
    alloc_buffer(&ist->stream, NULL, STREAM_BUF_SIZE);
    ist->iseos = false;
    ist->unclose = false;
    ist->type = IST_FILE_DESC;
    ist->handle.fd = des;
    return ist;
}

struct InputStream*
ist_from_path(const char* path)
{
    return ist_from_fd(open((path), O_RDONLY));
}

static void file_close(union input_handle* handle)
{
    handle->file.close(handle->file.f);
}

static int
file_read(union input_handle* handle, uint8_t* buf, int len)
{
    return fread(buf, 1, len, handle->file.f);
}

struct InputStream*
ist_from_fp(FILE* f, int (*closep)(FILE*))
{
    if (f == NULL)
        return NULL;
    struct InputStream* ist = malloc(sizeof(struct InputStream));
    alloc_buffer(&ist->stream, NULL, STREAM_BUF_SIZE);
    ist->iseos = false;
    ist->unclose = false;
    ist->type = IST_FILE_PIPE;
    ist->handle.file = (struct io_file_handle) {
        .f = f,
        .close = (closep) ? closep : fclose
    };
    return ist;
}

struct InputStream*
ist_from_buffer(const char* s, int len)
{
    if (s == NULL)
        return NULL;

    struct InputStream* ist = malloc(sizeof(struct InputStream));
    alloc_buffer(&ist->stream, (const uint8_t*)s, len);
    ist->iseos = false;
    ist->unclose = false;
    ist->type = IST_BUFFER;
    return ist;
}

static void ssl_close(union input_handle* handle)
{
    close(handle->ssl.sock);
    if (handle->ssl.ssl)
        SSL_free(handle->ssl.ssl);
}

static int ssl_read(union input_handle* handle, uint8_t* buf, int len)
{
    int status;
    if (handle->ssl.ssl) {
        for (;;) {
            status = SSL_read(handle->ssl.ssl, buf, len);
            if (status > 0)
                break;
            switch (SSL_get_error(handle->ssl.ssl, status)) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE: /* reads can trigger write errors; see SSL_get_error(3) */
                continue;
            default:
                break;
            }
            break;
        }
    } else {
        status = read(handle->ssl.sock, buf, len);
    }
    return status;
}
struct InputStream*
ist_from_socket(int sock, SSL* ssl)
{
    if (sock < 0)
        return NULL;
    struct InputStream* ist = malloc(sizeof(struct InputStream));
    alloc_buffer(&ist->stream, NULL, SSL_BUF_SIZE);
    ist->iseos = false;
    ist->unclose = false;
    ist->type = IST_SOCK;
    ist->handle.ssl = (struct ssl_handle) {
        .ssl = ssl,
        .sock = sock,
    };
    return ist;
}

static void
ens_close(union input_handle* handle)
{
    if (ist_destroy(handle->ens.is)) {
        handle->ens.is = NULL;
    }
    growbuf_destroy(handle->ens.gb);
}

static int
ens_read(union input_handle* handle, uint8_t* buf, int len)
{
    struct str_view gv = growbuf_str_view(handle->ens.gb);
    if (handle->ens.pos == growbuf_str_view(handle->ens.gb).len) {
        ist_gets_to_growbuf(handle->ens.is, handle->ens.gb, true);
        gv = growbuf_str_view(handle->ens.gb);
        if (gv.len == 0)
            return 0;

        if (handle->ens.encoding == ENC_BASE64) {
            gv = sv_chop(gv);
        } else if (handle->ens.encoding == ENC_UUENCODE) {
            if (gv.len >= 5 && !strncmp(gv.ptr, "begin", 5))
                ist_gets_to_growbuf(handle->ens.is, handle->ens.gb, true);
            gv = sv_chop(growbuf_str_view(handle->ens.gb));
        }

        struct growbuf* gbtmp = growbuf_create();
        char* p = (char*)gv.ptr;
        if (handle->ens.encoding == ENC_QUOTE)
            decodeQP_to_growbuf(gbtmp, &p);
        else if (handle->ens.encoding == ENC_BASE64)
            decodeB_to_growbuf(gbtmp, &p);
        else if (handle->ens.encoding == ENC_UUENCODE)
            decodeU_to_growbuf(gbtmp, &p);
        growbuf_destroy(handle->ens.gb);
        handle->ens.gb = gbtmp;
        handle->ens.pos = 0;
    }

    if (len > gv.len - handle->ens.pos)
        len = gv.len - handle->ens.pos;

    memcpy(buf, &gv.ptr[handle->ens.pos], len);
    handle->ens.pos += len;
    return len;
}

struct InputStream* ist_decode(struct InputStream* is, enum StreamEncoding encoding)
{
    if (is == NULL || (encoding != ENC_QUOTE && encoding != ENC_BASE64 && encoding != ENC_UUENCODE))
        return is;
    struct InputStream* stream = malloc(sizeof(struct InputStream));
    alloc_buffer(&stream->stream, NULL, STREAM_BUF_SIZE);
    stream->iseos = false;
    stream->unclose = false;
    stream->type = IST_ENCODED;
    stream->handle.ens = (struct encoded_stream_handle) {
        .is = is,
        .pos = 0,
        .encoding = encoding,
    };
    stream->handle.ens.gb = growbuf_create();
    return stream;
}

void ist_close(struct InputStream* ist)
{
    if (ist == NULL) {
        return;
    }

    void (*prevtrap)(int);
    prevtrap = signal(SIGINT, SIG_IGN);
    // ist->close(&ist->handle);
    switch (ist_type(ist)) {
    case IST_BUFFER: {
        // donothing
        break;
    }
    case IST_FILE_DESC: {
        basic_close(&ist->handle);
        break;
    }
    case IST_FILE_PIPE: {
        file_close(&ist->handle);
        break;
    }
    case IST_SOCK: {
        ssl_close(&ist->handle);
        break;
    }
    case IST_ENCODED: {
        ens_close(&ist->handle);
        break;
    }
    }
    signal(SIGINT, prevtrap);
    // }
}

bool ist_destroy(struct InputStream* ist)
{
    if (ist) {
        if (ist->unclose) {
            return false;
        }
        ist_close(ist);
        free(ist->stream.buf);
        free(ist);
    }
    return true;
}

int ist_getc(struct InputStream* ist)
{
    if (ist == NULL)
        return '\0';
    ist_drain(ist);
    return POP_CHAR(ist);
}

int ist_peek(struct InputStream* ist)
{
    int c = ist_getc(ist);
    struct StreamBuffer* sb = &ist->stream;
    if (sb->cur > 0) {
        sb->cur--;
    }
    return c;
}

int ist_read(struct InputStream* ist, uint8_t* dst, int count)
{
    if (ist == NULL || count <= 0)
        return -1;

    if (ist->iseos)
        return 0;

    int len = buffer_read(&ist->stream, dst, count);
    if (MUST_BE_UPDATED(&ist->stream)) {
        int l;
        switch (ist_type(ist)) {
        case IST_BUFFER: {
            // donothing
            break;
        }
        case IST_FILE_DESC: {
            l = basic_read(&ist->handle, dst + len, count - len);
            break;
        }
        case IST_FILE_PIPE: {
            l = file_read(&ist->handle, dst + len, count - len);
            break;
        }
        case IST_SOCK: {
            l = ssl_read(&ist->handle, dst + len, count - len);
            break;
        }
        case IST_ENCODED: {
            l = ens_read(&ist->handle, dst + len, count - len);
            break;
        }
        }

        if (l <= 0) {
            ist->iseos = true;
        } else {
            len += l;
        }
    }
    return len;
}

int ist_fd(struct InputStream* ist)
{
    if (ist == NULL)
        return -1;
    switch (ist->type) {
    case IST_FILE_DESC:
        return ist->handle.fd;
    case IST_FILE_PIPE:
        return fileno(ist->handle.file.f);
    case IST_SOCK:
        return ist->handle.ssl.sock;
    case IST_ENCODED:
        return ist_fd(ist->handle.ens.is);
    default:
        return -1;
    }
}

int ist_eos(struct InputStream* ist)
{
    ist_drain(ist);
    return ist->iseos;
}

void ist_gets_to_growbuf(struct InputStream* ist, struct growbuf* gb, bool check_crnl)
{
    growbuf_clear(gb);
    struct str_view gv;
    while (!ist_eos(ist)) {
        if (ist_drain(ist)) {
            continue;
        }
        gv = growbuf_str_view(gb);
        if (check_crnl && gv.len > 0 && gv.ptr[gv.len - 1] == '\r') {
            if (ist_peek(ist) == '\n') {
                growbuf_add_char(gb, '\n');
                ist_getc(ist);
            }
            break;
        }
        struct span span = (check_crnl)
            ? ist_buffered_until(ist, "\r\n")
            : ist_buffered_until(ist, "\n");
        growbuf_append(gb, span.ptr, span.len);
        gv = growbuf_str_view(gb);
        if (gv.len > 0 && gv.ptr[gv.len - 1] == '\n')
            break;
    }
    growbuf_add_char(gb, '\0');
    return;
}
