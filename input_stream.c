#include "input_stream.h"
#include "StreamBuffer.h"
#include "growbuf.h"
#include "mimehead.h"
#include "input_stream_impl.h"

#include <signal.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <openssl/ssl.h>

bool ist_drain(struct InputStream* ist)
{
    struct input_stream_base* sb = ist->handle;
    if (sb->iseos) {
        return false;
    }
    if (!MUST_BE_UPDATED(&sb->stream)) {
        return false;
    }

    sb->stream.cur = sb->stream.next = 0;
    int len = ist_read(ist, sb->stream.buf, sb->stream.size);
    if (len <= 0)
        sb->iseos = true;
    else
        sb->stream.next += len;

    return true;
}

static struct span ist_buffered_until(struct InputStream* ist, const char* needles)
{
    struct StreamBuffer* sb = ist->handle;
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
    struct input_stream_base* sb = ist->handle;
    sb->unclose = unclose;
}

#define POP_CHAR(bs) ((bs)->iseos ? '\0' : (bs)->stream.buf[(bs)->stream.cur++])

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
    case IST_ENCODED: {
        ens_close(ist->handle);
        break;
    }
    }
    signal(SIGINT, prevtrap);
    // }
}

bool ist_destroy(struct InputStream* ist)
{
    if (ist) {
        struct input_stream_base* base = ist->handle;
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
    struct input_stream_base* base = ist->handle;
    return POP_CHAR(base);
}

int ist_peek(struct InputStream* ist)
{
    int c = ist_getc(ist);
    struct input_stream_base* base = ist->handle;
    if (base->stream.cur > 0) {
        base->stream.cur--;
    }
    return c;
}

int ist_read(struct InputStream* ist, uint8_t* dst, int count)
{
    if (ist == NULL || count <= 0)
        return -1;

    struct input_stream_base* base = ist->handle;
    if (base->iseos)
        return 0;

    int len = buffer_read(&base->stream, dst, count);
    if (MUST_BE_UPDATED(&base->stream)) {
        int l;
        switch (ist_type(ist)) {
        case IST_BUFFER: {
            // donothing
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
        case IST_ENCODED: {
            l = ens_read(ist->handle, dst + len, count - len);
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

    struct input_stream_base* base = ist->handle;
    switch (ist->type) {
    case IST_FILE_DESC:
        return ((struct input_stream_fd*)ist->handle)->fd;
    case IST_FILE_PIPE:
        return fileno(((struct input_stream_fp*)ist->handle)->f);
    case IST_SOCK:
        return ((struct input_stream_sock*)ist->handle)->sock;
    case IST_ENCODED:
        return ist_fd(((struct input_stream_encoded*)ist->handle)->is);
    default:
        return -1;
    }
}

int ist_eos(struct InputStream* ist)
{
    ist_drain(ist);
    struct input_stream_base* base = ist->handle;
    return base->iseos;
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
