#include "StrWriter.h"

// static struct Writer new_writer(struct Writer* src)
// {
//     // copy interface
//     struct Writer w = *src;
//     // new data
//     w.data = Strnew();
//     return w;
// }

// static void str_delete(void* self)
// {
//     Strfree((pStr)self);
// }

static void str_clear(void* self)
{
    Strclear((pStr)self);
}

static uint8_t* str_begin(void* self)
{
    return (uint8_t*)((pStr)self)->ptr;
}

static uint8_t* str_end(void* self)
{
    return (uint8_t*)((pStr)self)->ptr + ((pStr)self)->len;
}

static void str_begin_end(void* self, const uint8_t* sp, const uint8_t* ep)
{
    Strcopy_begin_end((pStr)self, sp, ep);
}

static void str_push_str_len(void* self, const uint8_t* p, int len)
{
    Strcat_charp_n((pStr)self, (const char*)p, len);
}

static void str_push_str(void* self, const uint8_t* p)
{
    Strcat_charp((pStr)self, (const char*)p);
}

struct Writer makeWriter(pStr str)
{
    return (struct Writer) {
        .data = str,
        // static
        // .newWriter = new_writer,
        // member
        // .delete = str_delete,
        .clear = str_clear,
        .begin = str_begin,
        .end = str_end,
        .setBeginEnd = str_begin_end,
        .pushStrLen = str_push_str_len,
        .pushStr = str_push_str,
        // .pushChar = str_push_ch,
    };
}
