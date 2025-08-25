#pragma once

typedef void (*WriterFunc)(const char* p, int len, void* user);
typedef void (*FlushFunc)(void* user);

struct Writer {
    void* user;
    WriterFunc write;
    FlushFunc flush;
};

void writeWriter(const struct Writer* writer, const char* str, int len);
void putsWriter(const struct Writer* writer, const char* str);
void putWriter(const struct Writer* writer, char ch);
void flushWriter(const struct Writer* writer);
