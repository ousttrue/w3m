#pragma once
#include <stdint.h>

struct Writer;
typedef struct Writer (*NewWriterFunc)(struct Writer* interface);
typedef void (*Writer_SetBeginEndFunc)(void* self, const uint8_t* sp, const uint8_t* ep);
typedef void (*Writer_PushStrN)(void* self, const uint8_t* sp, int len);
typedef void (*Writer_PushStr)(void* self, const uint8_t* sp);
typedef void (*Writer_PushChar)(void* self, uint8_t ch);
typedef uint8_t* (*Writer_Ptr)(void* self);
typedef void (*Writer_Clear)(void* self);

struct Writer {
    void* data;
    // static
    NewWriterFunc newWriter;
    // member
    Writer_Clear delete;
    Writer_Clear clear;
    Writer_Ptr begin;
    Writer_Ptr end;
    Writer_SetBeginEndFunc setBeginEnd;
    Writer_PushStrN pushStrLen;
    Writer_PushStr pushStr;
    Writer_PushChar pushChar;
};

#define CALL0(w, method) (w)->method((w)->data)
#define CALL1(w, method, arg0) (w)->method((w)->data, arg0)
#define CALL2(w, method, arg0, arg1) (w)->method((w)->data, arg0, arg1)
