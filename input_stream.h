#pragma once
#include "ssl_stream.h"
#include <stdbool.h>
#include <stdio.h>

enum InputStreamType {
    IST_BASIC = 0,
    IST_FILE = 1,
    IST_STR = 2,
    IST_SSL = 3,
};

struct stream_buffer {
    uint8_t* buf;
    int size;
    int cur;
    int next;
};

inline static bool MUST_BE_UPDATED(struct stream_buffer* sb)
{
    return (sb->cur == sb->next);
}

typedef int (*FileCloseFunc)(FILE* stream);
struct io_file_handle {
    FILE* f;
    FileCloseFunc close;
};

struct input_stream {
    enum InputStreamType type;
    bool iseos;
    bool unclose;
    struct stream_buffer sb;
    union {
        int base;
        struct io_file_handle file;
        struct ssl_handle ssl;
    };
};

struct input_stream* newInputStream(int des);
struct input_stream* newFileStream(FILE* f, FileCloseFunc closep);
struct input_stream* newStrStream(Str s);
struct input_stream* newSSLStream(SSL* ssl, int sock);

int ISclose(struct input_stream* stream);
int ISgetc(struct input_stream* stream);
int ISundogetc(struct input_stream* stream);
Str StrISgets2(struct input_stream* stream, bool crnl);
int ISread_n(struct input_stream* stream, char* dst, int bufsize);
int ISfileno(struct input_stream* stream);
