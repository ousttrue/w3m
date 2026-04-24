#include "input_stream.h"
#include "input_stream_str.h"
#include "input_handle.h"
#include "mimehead.h"
#include "alloc.h"

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
        struct InputSpan span = (check_crnl)
            ? ist_buffered_until(ist, "\r\n")
            : ist_buffered_until(ist, "\n");
        growbuf_append(gb, span.ptr, span.length);
        gv = growbuf_str_view(gb);
        if (gv.len > 0 && gv.ptr[gv.len - 1] == '\n')
            break;
    }
    growbuf_add_char(gb, '\0');
    return;
}

static void
ens_close(union input_handle* handle)
{
    ist_close(handle->ens.is);
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
    stream->handle.ens.gb = growbuf_create();
    stream->read = ens_read;
    stream->close = ens_close;
    return stream;
}
