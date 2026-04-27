#include "input_stream.h"
#include "input_stream_impl.h"
#include <signal.h>

static void ist_drain(struct InputStream* ist)
{
    struct input_stream_base* sb = &ist->base;
    if (sb->iseos) {
        return;
    }
    if (!MUST_BE_UPDATED(&sb->stream)) {
        return;
    }

    sb->stream.cur = 0;
    sb->stream.length = 0;
    int len = ist_read(ist, sb->stream.buf, sb->stream.capacity);
    if (len <= 0) {
        sb->iseos = true;
    } else {
        sb->stream.length = len;
    }
}

static struct span ist_buffered_until(struct InputStream* ist, const char* needles)
{
    struct StreamBuffer* sb = &ist->base.stream;
    bool found = false;
    int i = sb->cur;
    for (; !found && i < sb->length; ++i) {
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
    struct input_stream_base* sb = &ist->base;
    sb->unclose = unclose;
}

void ist_close(struct InputStream* ist)
{
    if (ist == NULL) {
        return;
    }

    void (*prevtrap)(int);
    prevtrap = signal(SIGINT, SIG_IGN);
    switch (ist_type(ist)) {
    case IST_BUFFER: {
        // donothing
        break;
    }
    case IST_FILE_DESC: {
        fd_close(ist->handle);
        break;
    }
    case IST_FILE_PIPE: {
        fp_close(ist->handle);
        break;
    }
    case IST_SOCK: {
        sock_close(ist->handle);
        break;
    }
    }
    signal(SIGINT, prevtrap);
}

bool ist_destroy(struct InputStream* ist)
{
    if (ist) {
        struct input_stream_base* base = &ist->base;
        if (base->unclose) {
            return false;
        }
        ist_close(ist);
        free(base->stream.buf);
        free(ist);
    }
    return true;
}

int ist_getc(struct InputStream* ist)
{
    if (ist == NULL)
        return '\0';
    ist_drain(ist);
    struct input_stream_base* base = &ist->base;
    return POP_CHAR(base);
}

int ist_peek(struct InputStream* ist)
{
    int c = ist_getc(ist);
    struct input_stream_base* base = &ist->base;
    if (base->stream.cur > 0) {
        base->stream.cur--;
    }
    return c;
}

int ist_read(struct InputStream* ist, uint8_t* dst, int count)
{
    if (ist == NULL || count <= 0)
        return -1;

    struct input_stream_base* base = &ist->base;
    if (base->iseos)
        return 0;

    int len = buffer_read(&base->stream, dst, count);
    if (MUST_BE_UPDATED(&base->stream)) {
        int l;
        switch (ist_type(ist)) {
        case IST_BUFFER: {
            // donothing
            l = 0;
            break;
        }
        case IST_FILE_DESC: {
            l = fd_read(ist->handle, dst + len, count - len);
            break;
        }
        case IST_FILE_PIPE: {
            l = fp_read(ist->handle, dst + len, count - len);
            break;
        }
        case IST_SOCK: {
            l = sock_read(ist->handle, dst + len, count - len);
            break;
        }
        }

        if (l <= 0) {
            base->iseos = true;
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

    struct input_stream_base* base = &ist->base;
    switch (ist->type) {
    case IST_FILE_DESC:
        return fd_fd(ist->handle);
    case IST_FILE_PIPE:
        return fp_fd(ist->handle);
    case IST_SOCK:
        return sock_fd(ist->handle);
    default:
        return -1;
    }
}

bool ist_eos(struct InputStream* ist)
{
    ist_drain(ist);
    struct input_stream_base* base = &ist->base;
    return base->iseos;
    return false;
}

struct str_view ist_gets(struct InputStream* ist, bool check_crnl)
{
    struct input_stream_base* base = &ist->base;
    struct growbuf* gb = base->linebuf;
    growbuf_clear(gb);

    while (true) {
        ist_drain(ist);
        if (ist->base.iseos) {
            break;
        }

        int ch = ist_getc(ist);
        if (ch > 0) {
            growbuf_add_char(gb, ch);
            if (check_crnl && ch == '\r') {
                if (ist_peek(ist) == '\n') {
                    ist_getc(ist);
                    break;
                }
            } else if (ch == '\n') {
                break;
            } else {
            }
        } else {
            break;
        }
    }
    growbuf_add_char(gb, '\0');

    struct span span = growbuf_span(gb);
    span = sv_chop(span);
    return (struct str_view) {
        .ptr = (char*)span.ptr,
        .len = span.len,
    };
}
