#include "input_stream_str.h"
#include "input_handle.h"
#include "mimehead.h"
#include "alloc.h"

Str StrISgets2(struct InputStream* stream, char crnl)
{
    if (stream == NULL)
        return NULL;

    struct growbuf gb;
    growbuf_init(&gb);
    ist_gets_to_growbuf(stream, &gb, crnl);

    Str s = Strnew_size(gb.length);
    Strcat_charp_n(s, (const char*)gb.ptr, gb.length);
    return s;
}

void ist_gets_to_growbuf(struct InputStream* ist, struct growbuf* gb, bool check_crnl)
{
    struct StreamBuffer* sb = &ist->stream;

    gb->length = 0;

    while (!ist->iseos) {
        if (ist_drain(ist)) {
            continue;
        }
        if (check_crnl && gb->length > 0 && gb->ptr[gb->length - 1] == '\r') {
            if (sb->buf[sb->cur] == '\n') {
                GROWBUF_ADD_CHAR(gb, '\n');
                ++sb->cur;
            }
            break;
        }
        int i;
        for (i = sb->cur; i < sb->next; ++i) {
            if (sb->buf[i] == '\n' || (check_crnl && sb->buf[i] == '\r')) {
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
ens_close(union input_handle* handle)
{
    ist_close(handle->ens.is);
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

        ist_gets_to_growbuf(handle->ens.is, &handle->ens.gb, true);
        if (handle->ens.gb.length == 0)
            return 0;
        if (handle->ens.encoding == ENC_BASE64)
            memchop(handle->ens.gb.ptr, &handle->ens.gb.length);
        else if (handle->ens.encoding == ENC_UUENCODE) {
            if (handle->ens.gb.length >= 5 && !strncmp(handle->ens.gb.ptr, "begin", 5))
                ist_gets_to_growbuf(handle->ens.is, &handle->ens.gb, true);
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
    growbuf_init_without_GC(&stream->handle.ens.gb);
    stream->read = ens_read;
    stream->close = ens_close;
    return stream;
}
