#include "input_stream.h"
#include "growbuf.h"
#include "alloc.h"
#include "w3m_rc.h"
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <assert.h>

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
static void is_to_growbuf(struct input_stream* is, struct growbuf* gb, char crnl)
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
