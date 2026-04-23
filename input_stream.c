#include "input_stream.h"
#include "input_handle.h"
#include "UrlFile.h"
#include "alloc.h"
#include "proto.h"

#include <openssl/ssl.h>
#include <signal.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

static bool MUST_BE_UPDATED(struct StreamBuffer* b)
{
    return b->cur == b->next;
}

static void
do_update(struct InputStream* ist)
{
    ist->stream.cur = ist->stream.next = 0;
    int len = (*ist->read)(&ist->handle, ist->stream.buf, ist->stream.size);
    if (len <= 0)
        ist->iseos = true;
    else
        ist->stream.next += len;
}

bool ist_drain(struct InputStream* s)
{
    if (s->iseos) {
        return false;
    }
    if (!MUST_BE_UPDATED(&s->stream)) {
        return false;
    }
    do_update(s);
    return true;
}

enum InputStreamType ist_type(struct InputStream* stream)
{
    return stream->type;
}

void ist_set_unclose(struct InputStream* stream, bool unclose)
{
    stream->unclose = unclose;
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
    struct InputStream* stream = NewWithoutGC(struct InputStream);
    alloc_buffer(&stream->stream, NULL, STREAM_BUF_SIZE);
    stream->iseos = false;
    stream->unclose = false;
    stream->type = IST_FD;
    stream->handle.fd = des;
    stream->read = basic_read;
    stream->close = basic_close;
    return stream;
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
    struct InputStream* stream = NewWithoutGC(struct InputStream);
    alloc_buffer(&stream->stream, NULL, STREAM_BUF_SIZE);
    stream->iseos = false;
    stream->unclose = false;
    stream->type = IST_FILE;
    stream->handle.file = (struct io_file_handle) {
        .f = f,
        .close = (closep) ? closep : fclose
    };
    stream->read = file_read;
    stream->close = file_close;
    return stream;
}

static int
nop_read(union input_handle* handle, uint8_t* buf, int len)
{
    return 0;
}

struct InputStream*
ist_from_buffer(const char* s, int len)
{
    if (s == NULL)
        return NULL;

    struct InputStream* stream = NewWithoutGC(struct InputStream);
    alloc_buffer(&stream->stream, (const uint8_t*)s, len);
    stream->iseos = false;
    stream->unclose = false;
    stream->type = IST_BUFFER;
    stream->read = nop_read;
    stream->close = NULL;
    return stream;
}

struct InputStream*
ist_from_tcp(SSL* ssl, int sock)
{
    if (sock < 0)
        return NULL;
    struct InputStream* stream = NewWithoutGC(struct InputStream);
    alloc_buffer(&stream->stream, NULL, SSL_BUF_SIZE);
    stream->iseos = false;
    stream->unclose = false;
    stream->type = IST_SSL;
    stream->handle.ssl = (struct ssl_handle) {
        .ssl = ssl,
        .sock = sock,
    };
    stream->read = ssl_read;
    stream->close = ssl_close;
    return stream;
}

bool ist_close(struct InputStream* stream)
{
    void (*prevtrap)(int);
    if (stream == NULL)
        return -1;
    if (stream->close != NULL) {
        if (stream->unclose) {
            return -1;
        }
        prevtrap = signal(SIGINT, SIG_IGN);
        stream->close(&stream->handle);
        signal(SIGINT, prevtrap);
    }
    xfree(stream->stream.buf);
    xfree(stream);
    return 0;
}

int ist_getc(struct InputStream* s)
{
    if (s == NULL)
        return '\0';
    ist_drain(s);
    return POP_CHAR(s);
}

int ist_peek(struct InputStream* s)
{
    int c = ist_getc(s);
    struct StreamBuffer* sb = &s->stream;
    if (sb->cur > 0) {
        sb->cur--;
    }
    return c;
}

int ist_read(struct InputStream* ist, char* dst, int count)
{
    if (ist == NULL || count <= 0)
        return -1;

    if (ist->iseos)
        return 0;

    int len = buffer_read(&ist->stream, dst, count);
    if (MUST_BE_UPDATED(&ist->stream)) {
        int l = (*ist->read)(&ist->handle, (uint8_t*)&dst[len], count - len);
        if (l <= 0) {
            ist->iseos = true;
        } else {
            len += l;
        }
    }
    return len;
}

int ist_fd(struct InputStream* stream)
{
    if (stream == NULL)
        return -1;
    switch (stream->type) {
    case IST_FD:
        return stream->handle.fd;
    case IST_FILE:
        return fileno(stream->handle.file.f);
    case IST_SSL:
        return stream->handle.ssl.sock;
    case IST_ENCODED:
        return ist_fd(stream->handle.ens.is);
    default:
        return -1;
    }
}

int ist_eos(struct InputStream* ist)
{
    ist_drain(ist);
    return ist->iseos;
}

void ssl_close(union input_handle* handle)
{
    close(handle->ssl.sock);
    if (handle->ssl.ssl)
        SSL_free(handle->ssl.ssl);
}

int ssl_read(union input_handle* handle, uint8_t* buf, int len)
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
