#include "writer.h"
#include <string.h>
#include <assert.h>

void writeWriter(const struct Writer* writer, const char* str, int len)
{
    writer->write(str, len, writer->user);
}

void putsWriter(const struct Writer* writer, const char* str)
{
    if(str){
        writer->write(str, strlen(str), writer->user);
    }
}

void putWriter(const struct Writer* writer, char ch)
{
    writer->write(&ch, 1, writer->user);
}

void flushWriter(const struct Writer* writer)
{
    writer->flush(writer->user);
}

static void array_writer(const char* buf, int len, void* user)
{
    struct ArrayInfo* info = (struct ArrayInfo*)user;
    assert(info->pos + len < info->len);
    memcpy(info->buf + info->pos, buf, len);
    info->pos += len;
}

static void array_flush(void* user)
{
    // NOP
}

void makeArrayWriter(struct Writer* writer, struct ArrayInfo* info)
{
    writer->user = info;
    writer->write = &array_writer;
    writer->flush = &array_flush;
}
