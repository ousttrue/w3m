#include "writer.h"
#include <string.h>

void writeWriter(const struct Writer* writer, const char* str, int len)
{
    writer->write(str, len, writer->user);
}

void putsWriter(const struct Writer* writer, const char* str)
{
    writer->write(str, strlen(str), writer->user);
}

void putWriter(const struct Writer* writer, char ch)
{
    writer->write(&ch, 1, writer->user);
}

void flushWriter(const struct Writer* writer)
{
    writer->flush(writer->user);
}
