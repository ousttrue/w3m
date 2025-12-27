#pragma once
#include "growbuf.h"
#include "stream_buffer.h"
#include "ssl_stream.h"
#include <stdbool.h>
#include <stdio.h>

#define IST_UNCLOSE 0x10

struct io_file_handle {
    FILE* f;
    void (*close)(void*);
};

struct base_stream {
    struct stream_buffer stream;
    void* handle;
    char type;
    char iseos;
    int (*read)(void*, void*, int);
    void (*close)(void*);
};

struct file_stream {
    struct stream_buffer stream;
    struct io_file_handle* handle;
    char type;
    char iseos;
    int (*read)();
    void (*close)();
};

struct str_stream {
    struct stream_buffer stream;
    Str handle;
    char type;
    char iseos;
    int (*read)();
    void (*close)();
};

struct encoded_stream {
    struct stream_buffer stream;
    struct ens_handle* handle;
    char type;
    char iseos;
    int (*read)();
    void (*close)();
};

union input_stream {
    struct base_stream base;
    struct file_stream file;
    struct str_stream str;
    struct ssl_stream ssl;
    struct encoded_stream ens;
};
union input_stream* newSSLStream(SSL* ssl, int sock);

union input_stream* newInputStream(int des);
union input_stream* newFileStream(FILE* f, void (*closep)());
union input_stream* newStrStream(Str s);
int ISclose(union input_stream* stream);
int ISgetc(union input_stream* stream);
int ISundogetc(union input_stream* stream);
Str StrISgets2(union input_stream* stream, bool crnl);
void ISgets_to_growbuf(union input_stream* stream, struct growbuf* gb, char crnl);
int ISread_n(union input_stream* stream, char* dst, int bufsize);
int ISfileno(union input_stream* stream);
int ISeos(union input_stream* stream);
void init_base_stream(struct base_stream* base, int bufsize);
