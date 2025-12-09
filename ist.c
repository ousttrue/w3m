#include "ist.h"
#include "html.h"
#include "signal_jmp.h"
#include "mimehead.h"
#include <stdint.h>
#include <unistd.h>
#include <signal.h>

#define STREAM_BUF_SIZE 8192
#define SSL_BUF_SIZE 1536

static void
init_base_stream(struct base_stream* base, int bufsize)
{
    init_buffer(base, NULL, bufsize);
}

static int
basic_read(int* handle, char* buf, int len)
{
    return read(*(int*)handle, buf, len);
}

static void
basic_close(int* handle)
{
    close(*(int*)handle);
    xfree(handle);
}

union input_stream*
newInputStream(int des)
{
    union input_stream* stream;
    if (des < 0)
        return NULL;
    stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, STREAM_BUF_SIZE);
    stream->base.type = IST_BASIC;
    stream->base.handle = NewWithoutGC(int);
    *(int*)stream->base.handle = des;
    stream->base.read = (int (*)(void*, void*, int))basic_read;
    stream->base.close = (void (*)(void*))basic_close;
    return stream;
}

//
// FILE
//
static void
file_close(struct io_file_handle* handle)
{
    handle->close(handle->f);
    xfree(handle);
}

static int
file_read(struct io_file_handle* handle, char* buf, int len)
{
    return fread(buf, 1, len, handle->f);
}

union input_stream*
newFileStream(FILE* f, void (*closep)())
{
    union input_stream* stream;
    if (f == NULL)
        return NULL;
    stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, STREAM_BUF_SIZE);
    stream->file.type = IST_FILE;
    stream->file.handle = NewWithoutGC(struct io_file_handle);
    stream->file.handle->f = f;
    if (closep)
        stream->file.handle->close = (void (*)(void*))closep;
    else
        stream->file.handle->close = (void (*)(void*))fclose;
    stream->file.read = (int (*)())file_read;
    stream->file.close = (void (*)())file_close;
    return stream;
}

//
// str
//
static void
init_str_stream(struct base_stream* base, Str s)
{
    init_buffer(base, s->ptr, s->length);
}

static int
str_read(Str handle, char* buf, int len)
{
    return 0;
}

union input_stream*
newStrStream(Str s)
{
    union input_stream* stream;
    if (s == NULL)
        return NULL;
    stream = NewWithoutGC(union input_stream);
    init_str_stream(&stream->base, s);
    stream->str.type = IST_STR;
    stream->str.handle = NULL;
    stream->str.read = (int (*)())str_read;
    stream->str.close = NULL;
    return stream;
}

//
// SSL
//
static void
ssl_close(struct ssl_handle* handle)
{
    close(handle->sock);
    if (handle->ssl)
        SSL_free(handle->ssl);
    xfree(handle);
}

static int
ssl_read(struct ssl_handle* handle, char* buf, int len)
{
    int status;
    if (handle->ssl) {
        for (;;) {
            status = SSL_read(handle->ssl, buf, len);
            if (status > 0)
                break;
            switch (SSL_get_error(handle->ssl, status)) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE: /* reads can trigger write errors; see SSL_get_error(3) */
                continue;
            default:
                break;
            }
            break;
        }
    } else
        status = read(handle->sock, buf, len);
    return status;
}

union input_stream*
newSSLStream(SSL* ssl, int sock)
{
    union input_stream* stream;
    if (sock < 0)
        return NULL;
    stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, SSL_BUF_SIZE);
    stream->ssl.type = IST_SSL;
    stream->ssl.handle = NewWithoutGC(struct ssl_handle);
    stream->ssl.handle->ssl = ssl;
    stream->ssl.handle->sock = sock;
    stream->ssl.read = (int (*)())ssl_read;
    stream->ssl.close = (void (*)())ssl_close;
    return stream;
}

//
// EncodedStream
//
static void
ens_close(struct ens_handle* handle)
{
    ISclose(handle->is);
    growbuf_clear(&handle->gb);
    xfree(handle);
}

#define MUST_BE_UPDATED(bs) ((bs)->stream.cur == (bs)->stream.next)

static void
do_update(struct base_stream* base)
{
    int len;
    base->stream.cur = base->stream.next = 0;
    len = (*base->read)(base->handle, base->stream.buf, base->stream.size);
    if (len <= 0)
        base->iseos = true;
    else
        base->stream.next += len;
}

static void ISgets_to_growbuf(union input_stream* stream, struct growbuf* gb, char crnl)
{
    struct base_stream* base = &stream->base;
    struct stream_buffer* sb = &base->stream;
    int i;

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
ens_read(struct ens_handle* handle, char* buf, int len)
{
    if (handle->pos == handle->gb.length) {
        char* p;
        struct growbuf gbtmp;

        ISgets_to_growbuf(handle->is, &handle->gb, true);
        if (handle->gb.length == 0)
            return 0;
        if (handle->encoding == ENC_BASE64)
            memchop(handle->gb.ptr, &handle->gb.length);
        else if (handle->encoding == ENC_UUENCODE) {
            if (handle->gb.length >= 5 && !strncmp(handle->gb.ptr, "begin", 5))
                ISgets_to_growbuf(handle->is, &handle->gb, true);
            memchop(handle->gb.ptr, &handle->gb.length);
        }
        growbuf_init_without_GC(&gbtmp);
        p = handle->gb.ptr;
        if (handle->encoding == ENC_QUOTE)
            decodeQP_to_growbuf(&gbtmp, &p);
        else if (handle->encoding == ENC_BASE64)
            decodeB_to_growbuf(&gbtmp, &p);
        else if (handle->encoding == ENC_UUENCODE)
            decodeU_to_growbuf(&gbtmp, &p);
        growbuf_clear(&handle->gb);
        handle->gb = gbtmp;
        handle->pos = 0;
    }

    if (len > handle->gb.length - handle->pos)
        len = handle->gb.length - handle->pos;

    memcpy(buf, &handle->gb.ptr[handle->pos], len);
    handle->pos += len;
    return len;
}

union input_stream*
newEncodedStream(union input_stream* is, char encoding)
{
    union input_stream* stream;
    if (is == NULL || (encoding != ENC_QUOTE && encoding != ENC_BASE64 && encoding != ENC_UUENCODE))
        return is;
    stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, STREAM_BUF_SIZE);
    stream->ens.type = IST_ENCODED;
    stream->ens.handle = NewWithoutGC(struct ens_handle);
    stream->ens.handle->is = is;
    stream->ens.handle->pos = 0;
    stream->ens.handle->encoding = encoding;
    growbuf_init_without_GC(&stream->ens.handle->gb);
    stream->ens.read = (int (*)())ens_read;
    stream->ens.close = (void (*)())ens_close;
    return stream;
}

//
// interface
//
int ISclose(union input_stream* stream)
{
    MySignalHandler prevtrap;
    if (stream == NULL)
        return -1;
    if (stream->base.close != NULL) {
        if (stream->base.unclose) {
            return -1;
        }
        prevtrap = mySignal(SIGINT, SIG_IGN);
        stream->base.close(stream->base.handle);
        mySignal(SIGINT, prevtrap);
    }
    xfree(stream->base.stream.buf);
    xfree(stream);
    return 0;
}

#define POP_CHAR(bs) ((bs)->iseos ? '\0' : (bs)->stream.buf[(bs)->stream.cur++])

int ISgetc(union input_stream* stream)
{
    struct base_stream* base;
    if (stream == NULL)
        return '\0';
    base = &stream->base;
    if (!base->iseos && MUST_BE_UPDATED(base))
        do_update(base);
    return POP_CHAR(base);
}

static Str StrISgets2(union input_stream* stream, bool crnl)
{
    struct growbuf gb;

    if (stream == NULL)
        return NULL;
    growbuf_init(&gb);
    ISgets_to_growbuf(stream, &gb, crnl);
    return growbuf_to_Str(&gb);
}
Str StrISgets(union input_stream* stream) { return StrISgets2(stream, false); }
Str StrmyISgets(union input_stream* stream) { return StrISgets2(stream, true); }

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

int ISread_n(union input_stream* stream, char* dst, int count)
{
    int len, l;
    struct base_stream* base;

    if (stream == NULL || count <= 0)
        return -1;
    if ((base = &stream->base)->iseos)
        return 0;

    len = buffer_read(&base->stream, dst, count);
    if (MUST_BE_UPDATED(base)) {
        l = (*base->read)(base->handle, &dst[len], count - len);
        if (l <= 0) {
            base->iseos = true;
        } else {
            len += l;
        }
    }
    return len;
}
