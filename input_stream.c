#include "input_stream.h"
#include "UrlFile.h"
#include "alloc.h"
#include "mimehead.h"
#include "proto.h"

#include <signal.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define STREAM_BUF_SIZE 8192
#define SSL_BUF_SIZE 1536

#define MUST_BE_UPDATED(bs) ((bs)->stream.cur == (bs)->stream.next)

#define POP_CHAR(bs) ((bs)->iseos ? '\0' : (bs)->stream.buf[(bs)->stream.cur++])

static void
do_update(struct InputStream* base)
{
    base->stream.cur = base->stream.next = 0;
    int len = (*base->read)(&base->handle, base->stream.buf, base->stream.size);
    if (len <= 0)
        base->iseos = true;
    else
        base->stream.next += len;
}

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
newInputStream(int des)
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
newFileStream(FILE* f, int (*closep)(FILE*))
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
newStrStream(const char* s, int len)
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
newSSLStream(SSL* ssl, int sock)
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

static void
ens_close(union input_handle* handle)
{
    ISclose(handle->ens.is);
    growbuf_clear(&handle->ens.gb);
}

static void
memchop(char* p, int* len)
{
    char* q;
    for (q = p + *len; q > p; --q) {
        if (q[-1] != '\n' && q[-1] != '\r')
            break;
    }
    if (q != p + *len)
        *q = '\0';
    *len = q - p;
    return;
}

static int
ens_read(union input_handle* handle, uint8_t* buf, int len)
{
    if (handle->ens.pos == handle->ens.gb.length) {
        struct growbuf gbtmp;

        ISgets_to_growbuf(handle->ens.is, &handle->ens.gb, true);
        if (handle->ens.gb.length == 0)
            return 0;
        if (handle->ens.encoding == ENC_BASE64)
            memchop(handle->ens.gb.ptr, &handle->ens.gb.length);
        else if (handle->ens.encoding == ENC_UUENCODE) {
            if (handle->ens.gb.length >= 5 && !strncmp(handle->ens.gb.ptr, "begin", 5))
                ISgets_to_growbuf(handle->ens.is, &handle->ens.gb, true);
            memchop(handle->ens.gb.ptr, &handle->ens.gb.length);
        }
        growbuf_init_without_GC(&gbtmp);
        char* p = (char*)handle->ens.gb.ptr;
        if (handle->ens.encoding == ENC_QUOTE)
            decodeQP_to_growbuf(&gbtmp, &p);
        else if (handle->ens.encoding == ENC_BASE64)
            decodeB_to_growbuf(&gbtmp, &p);
        else if (handle->ens.encoding == ENC_UUENCODE)
            decodeU_to_growbuf(&gbtmp, &p);
        growbuf_clear(&handle->ens.gb);
        handle->ens.gb = gbtmp;
        handle->ens.pos = 0;
    }

    if (len > handle->ens.gb.length - handle->ens.pos)
        len = handle->ens.gb.length - handle->ens.pos;

    memcpy(buf, &handle->ens.gb.ptr[handle->ens.pos], len);
    handle->ens.pos += len;
    return len;
}

struct InputStream* newEncodedStream(struct InputStream* is, enum StreamEncoding encoding)
{
    if (is == NULL || (encoding != ENC_QUOTE && encoding != ENC_BASE64 && encoding != ENC_UUENCODE))
        return is;
    struct InputStream* stream = NewWithoutGC(struct InputStream);
    alloc_buffer(&stream->stream, NULL, STREAM_BUF_SIZE);
    stream->iseos = false;
    stream->unclose = false;
    stream->type = IST_ENCODED;
    stream->handle.ens = (struct encoded_stream_handle) {
        .is = is,
        .pos = 0,
        .encoding = encoding,
    };
    growbuf_init_without_GC(&stream->handle.ens.gb);
    stream->read = ens_read;
    stream->close = ens_close;
    return stream;
}

int ISclose(struct InputStream* stream)
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

int ISgetc(struct InputStream* base)
{
    if (base == NULL)
        return '\0';
    if (!base->iseos && MUST_BE_UPDATED(base))
        do_update(base);
    return POP_CHAR(base);
}

int ISundogetc(struct InputStream* stream)
{
    if (stream == NULL)
        return -1;
    struct StreamBuffer* sb = &stream->stream;
    if (sb->cur > 0) {
        sb->cur--;
        return 0;
    }
    return -1;
}

Str StrISgets2(struct InputStream* stream, char crnl)
{
    if (stream == NULL)
        return NULL;

    struct growbuf gb;
    growbuf_init(&gb);
    ISgets_to_growbuf(stream, &gb, crnl);

    Str s = Strnew_size(gb.length);
    Strcat_charp_n(s, (const char*)gb.ptr, gb.length);
    return s;
}

void ISgets_to_growbuf(struct InputStream* base, struct growbuf* gb, char crnl)
{
    struct StreamBuffer* sb = &base->stream;

    gb->length = 0;

    while (!base->iseos) {
        if (MUST_BE_UPDATED(base)) {
            do_update(base);
            continue;
        }
        if (crnl && gb->length > 0 && gb->ptr[gb->length - 1] == '\r') {
            if (sb->buf[sb->cur] == '\n') {
                GROWBUF_ADD_CHAR(gb, '\n');
                ++sb->cur;
            }
            break;
        }
        int i;
        for (i = sb->cur; i < sb->next; ++i) {
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
    return;
}

int ISread_n(struct InputStream* base, char* dst, int count)
{
    if (base == NULL || count <= 0)
        return -1;

    if (base->iseos)
        return 0;

    int len = buffer_read(&base->stream, dst, count);
    if (MUST_BE_UPDATED(base)) {
        int l = (*base->read)(&base->handle, (uint8_t*)&dst[len], count - len);
        if (l <= 0) {
            base->iseos = true;
        } else {
            len += l;
        }
    }
    return len;
}

int ISfd(struct InputStream* stream)
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
        return ISfd(stream->handle.ens.is);
    default:
        return -1;
    }
}

int ISeos(struct InputStream* base)
{
    if (!base->iseos && MUST_BE_UPDATED(base))
        do_update(base);
    return base->iseos;
}
