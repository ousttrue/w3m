#pragma once
#include "growbuf.h"
#include "stream_buffer.h"
#include "ssl_stream.h"
#include <stdbool.h>
#include <stdio.h>

typedef int (*FileCloseFunc)(FILE* stream);
struct io_file_handle {
    FILE* f;
    FileCloseFunc close;
};

struct base_stream {
    enum InputStreamType type;
    bool iseos;
    bool unclose;
    struct stream_buffer stream;
    void* handle;
};

struct file_stream {
    enum InputStreamType type;
    bool iseos;
    bool unclose;
    struct stream_buffer stream;
    struct io_file_handle* handle;
};

struct str_stream {
    enum InputStreamType type;
    bool iseos;
    bool unclose;
    struct stream_buffer stream;
    Str handle;
};

union input_stream {
    struct base_stream base;
    struct file_stream file;
    struct str_stream str;
    struct ssl_stream ssl;
};

union input_stream* newInputStream(int des);
union input_stream* newFileStream(FILE* f, FileCloseFunc closep);
union input_stream* newStrStream(Str s);
union input_stream* newSSLStream(SSL* ssl, int sock);

int ISclose(union input_stream* stream);
int ISgetc(union input_stream* stream);
int ISundogetc(union input_stream* stream);
Str StrISgets2(union input_stream* stream, bool crnl);
void ISgets_to_growbuf(union input_stream* stream, struct growbuf* gb, char crnl);
int ISread_n(union input_stream* stream, char* dst, int bufsize);
int ISfileno(union input_stream* stream);
int ISeos(union input_stream* stream);
void init_base_stream(struct base_stream* base, int bufsize);
