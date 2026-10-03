#pragma once
#include "writer.h"
#include <stdlib.h>
#include <string.h>

struct ArrayData {
    uint8_t* buf;
    int len;
    int capacity;
};

static inline void array_clear(void* _self)
{
    struct ArrayData* self = (struct ArrayData*)_self;
    self->len = 0;
}

static inline uint8_t* array_begin(void* _self)
{
    struct ArrayData* self = (struct ArrayData*)_self;
    return self->buf;
}

static inline uint8_t* array_end(void* _self)
{
    struct ArrayData* self = (struct ArrayData*)_self;
    return self->buf + self->len;
}

static inline void array_begin_end(void* _self, const uint8_t* sp, const uint8_t* ep)
{
    struct ArrayData* self = (struct ArrayData*)_self;
    int len = ep - sp;
    if (len > self->capacity) {
        exit(-1);
    }
    memcpy(self->buf, sp, len);
    self->len = len;
}

static inline void array_push_str_len(void* _self, const uint8_t* p, int len)
{
    struct ArrayData* self = (struct ArrayData*)_self;
    if (self->len + len > self->capacity) {
        exit(-1);
    }
    memcpy(self->buf, p, len);
    self->len += len;
}

static inline void array_push_str(void* _self, const uint8_t* p)
{
    int len = strlen((const char*)p);
    struct ArrayData* self = (struct ArrayData*)_self;
    if (self->len + len > self->capacity) {
        exit(-1);
    }
    memcpy(self->buf, p, len);
    self->len += len;
}

inline static struct Writer arrayWriter(struct ArrayData* data)
{
    return (struct Writer) {
        .data = data,
        // member
        // .delete = array_clear,
        .clear = array_clear,
        .begin = array_begin,
        .end = array_end,
        .setBeginEnd = array_begin_end,
        .pushStrLen = array_push_str_len,
        .pushStr = array_push_str,
    };
}
