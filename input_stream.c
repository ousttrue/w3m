#include "input_stream.h"
#include "alloc.h"
#include "ssl_stream.h"
#include "stream_buffer.h"
#include "w3m_rc.h"
#include "fm.h"
#include "input_stream.h"
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <assert.h>

#define STREAM_BUF_SIZE 8192

static int raw_read(struct base_stream* base, uint8_t* p, size_t len)
{
    switch (base->type) {
    case IST_BASIC:
        return read(*(int*)base->handle, p, len);
    case IST_FILE:
        return fread(p, 1, len, ((struct file_stream*)base)->handle->f);
    case IST_STR:
        return 0;
    case IST_SSL:
        return ssl_read(((struct ssl_stream*)base)->handle, (char*)p, len);
    default:
        return -1;
    }
}

static void
do_update(struct base_stream* base)
{
    base->stream.cur = base->stream.next = 0;
    int len = raw_read(base, base->stream.buf, base->stream.size);
    if (len <= 0)
        base->iseos = TRUE;
    else
        base->stream.next += len;
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

static void
init_buffer(struct base_stream* base, char* buf, int bufsize)
{
    struct stream_buffer* sb = &base->stream;
    sb->size = bufsize;
    sb->cur = 0;
    sb->buf = NewWithoutGC_N(uint8_t, bufsize);
    if (buf) {
        memcpy(sb->buf, buf, bufsize);
        sb->next = bufsize;
    } else {
        sb->next = 0;
    }
    base->iseos = FALSE;
}

void init_base_stream(struct base_stream* base, int bufsize)
{
    init_buffer(base, NULL, bufsize);
}

static void
init_str_stream(struct base_stream* base, Str s)
{
    init_buffer(base, s->ptr, s->length);
}

union input_stream*
newInputStream(int des)
{
    if (des < 0)
        return NULL;
    union input_stream* stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, STREAM_BUF_SIZE);
    stream->base.type = IST_BASIC;
    stream->base.unclose = false;
    stream->base.handle = NewWithoutGC(int);
    *(int*)stream->base.handle = des;
    return stream;
}

union input_stream*
newFileStream(FILE* f, FileCloseFunc closep)
{
    if (f == NULL)
        return NULL;
    union input_stream* stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, STREAM_BUF_SIZE);
    stream->file.type = IST_FILE;
    stream->base.unclose = false;
    stream->file.handle = NewWithoutGC(struct io_file_handle);
    stream->file.handle->f = f;
    if (closep)
        stream->file.handle->close = closep;
    else
        stream->file.handle->close = fclose;
    return stream;
}

union input_stream*
newStrStream(Str s)
{
    if (s == NULL)
        return NULL;
    union input_stream* stream = NewWithoutGC(union input_stream);
    init_str_stream(&stream->base, s);
    stream->str.type = IST_STR;
    stream->base.unclose = false;
    stream->str.handle = NULL;
    return stream;
}

#define SSL_BUF_SIZE 1536

union input_stream*
newSSLStream(SSL* ssl, int sock)
{
    if (sock < 0)
        return NULL;

    union input_stream* stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, SSL_BUF_SIZE);
    ssl_stream_init(&stream->ssl, sock, ssl);
    stream->base.unclose = false;
    return stream;
}

int ISclose(union input_stream* stream)
{
    if (stream == NULL)
        return -1;

    if (stream->base.unclose) {
        return -1;
    }

    void (*prevtrap)(int);
    prevtrap = mySignal(SIGINT, SIG_IGN);
    switch (stream->base.type) {
    case IST_BASIC:
        close(*(int*)stream->base.handle);
        break;
    case IST_FILE:
        stream->file.handle->close(stream->file.handle->f);
        break;
    case IST_STR:
        break;
    case IST_SSL:
        ssl_close(stream->ssl.handle);
        break;
    default:
        assert(false);
        break;
    }
    mySignal(SIGINT, prevtrap);

    xfree(stream->base.stream.buf);
    xfree(stream);
    return 0;
}

#define POP_CHAR(bs) ((bs)->iseos ? '\0' : (bs)->stream.buf[(bs)->stream.cur++])
int ISgetc(union input_stream* stream)
{
    if (stream == NULL)
        return '\0';
    struct base_stream* base = &stream->base;
    if (!base->iseos && MUST_BE_UPDATED(&base->stream))
        do_update(base);
    return POP_CHAR(base);
}

int ISundogetc(union input_stream* stream)
{
    struct stream_buffer* sb;
    if (stream == NULL)
        return -1;
    sb = &stream->base.stream;
    if (sb->cur > 0) {
        sb->cur--;
        return 0;
    }
    return -1;
}

Str StrISgets2(union input_stream* stream, bool crnl)
{
    if (stream == NULL)
        return NULL;

    struct growbuf gb;
    growbuf_init(&gb);
    ISgets_to_growbuf(stream, &gb, crnl);
    return growbuf_to_Str(&gb);
}

void ISgets_to_growbuf(union input_stream* stream, struct growbuf* gb, char crnl)
{
    struct base_stream* base = &stream->base;
    struct stream_buffer* sb = &base->stream;

    gb->length = 0;
    while (!base->iseos) {
        if (MUST_BE_UPDATED(&base->stream)) {
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
    return;
}

int ISread_n(union input_stream* stream, char* dst, int count)
{
    if (stream == NULL || count <= 0)
        return -1;

    struct base_stream* base;
    if ((base = &stream->base)->iseos)
        return 0;

    int len = buffer_read(&base->stream, dst, count);
    if (MUST_BE_UPDATED(&base->stream)) {
        int l = raw_read(base, (uint8_t*)&dst[len], count - len);
        if (l <= 0) {
            base->iseos = TRUE;
        } else {
            len += l;
        }
    }
    return len;
}

int ISfileno(union input_stream* stream)
{
    if (stream == NULL)
        return -1;
    switch (stream->base.type) {
    case IST_BASIC:
        return *(int*)stream->base.handle;
    case IST_FILE:
        return fileno(stream->file.handle->f);
    case IST_SSL:
        return stream->ssl.handle->sock;
    default:
        return -1;
    }
}

int ISeos(union input_stream* stream)
{
    struct base_stream* base = &stream->base;
    if (!base->iseos && MUST_BE_UPDATED(&base->stream))
        do_update(base);
    return base->iseos;
}
