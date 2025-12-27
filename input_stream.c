#include "input_stream.h"
#include "w3m_rc.h"
#include "linein.h"
#include "message.h"
#include "fm.h"
#include "myctype.h"
#include "input_stream.h"
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define uchar unsigned char

#define STREAM_BUF_SIZE 8192

#define MUST_BE_UPDATED(bs) ((bs)->stream.cur == (bs)->stream.next)

#define POP_CHAR(bs) ((bs)->iseos ? '\0' : (bs)->stream.buf[(bs)->stream.cur++])

static void basic_close(int* handle);
static int basic_read(int* handle, char* buf, int len);

static void file_close(struct io_file_handle* handle);
static int file_read(struct io_file_handle* handle, char* buf, int len);

static int str_read(Str handle, char* buf, int len);

static int ens_read(struct ens_handle* handle, char* buf, int len);
static void ens_close(struct ens_handle* handle);

static void memchop(char* p, int* len);

static void
do_update(struct base_stream* base)
{
    int len;
    base->stream.cur = base->stream.next = 0;
    len = (*base->read)(base->handle, base->stream.buf, base->stream.size);
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
    sb->buf = NewWithoutGC_N(uchar, bufsize);
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

#ifdef USE_SSL

#endif

int ISclose(union input_stream* stream)
{
    void (*prevtrap)(int);
    if (stream == NULL)
        return -1;
    if (stream->base.close != NULL) {
        if (stream->base.type & IST_UNCLOSE) {
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
    struct growbuf gb;

    if (stream == NULL)
        return NULL;
    growbuf_init(&gb);
    ISgets_to_growbuf(stream, &gb, crnl);
    return growbuf_to_Str(&gb);
}

void ISgets_to_growbuf(union input_stream* stream, struct growbuf* gb, char crnl)
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

#ifdef unused
int ISread(union input_stream* stream, Str buf, int count)
{
    int len;

    if (count + 1 > buf->area_size) {
        char* newptr = GC_MALLOC_ATOMIC(count + 1);
        memcpy(newptr, buf->ptr, buf->length);
        newptr[buf->length] = '\0';
        buf->ptr = newptr;
        buf->area_size = count + 1;
    }
    len = ISread_n(stream, buf->ptr, count);
    buf->length = (len > 0) ? len : 0;
    buf->ptr[buf->length] = '\0';
    return (len > 0) ? 1 : 0;
}
#endif

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
    switch (stream->base.type & ~IST_UNCLOSE) {
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
    if (!base->iseos && MUST_BE_UPDATED(base))
        do_update(base);
    return base->iseos;
}

static void
basic_close(int* handle)
{
    close(*(int*)handle);
    xfree(handle);
}

static int
basic_read(int* handle, char* buf, int len)
{
#ifdef __MINGW32_VERSION
    return recv(*(int*)handle, buf, len, 0);
#else
    return read(*(int*)handle, buf, len);
#endif
}

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

static int
str_read(Str handle, char* buf, int len)
{
    return 0;
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
    return stream;
}
