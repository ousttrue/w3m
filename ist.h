#pragma once
#include <gcstr/alloc.h>
#include <gcstr/Str.h>

#include <stdio.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>
#include <stdbool.h>

struct stream_buffer {
    unsigned char* buf;
    int size;
    int cur;
    int next;
};

struct io_file_handle {
    FILE* f;
    void (*close)(void*);
};

struct ssl_handle {
    SSL* ssl;
    int sock;
};

union input_stream;

struct ens_handle {
    union input_stream* is;
    struct growbuf gb;
    int pos;
    char encoding;
};

enum InputStreamType {
    IST_BASIC,
    IST_FILE,
    IST_STR,
    IST_SSL,
    IST_ENCODED,
};

struct base_stream {
    struct stream_buffer stream;
    void* handle;
    enum InputStreamType type;
    char iseos;
    int (*read)(void*, void*, int);
    void (*close)(void*);
    bool unclose;
};

struct file_stream {
    struct stream_buffer stream;
    struct io_file_handle* handle;
    enum InputStreamType type;
    char iseos;
    int (*read)();
    void (*close)();
    bool unclose;
};

struct str_stream {
    struct stream_buffer stream;
    Str handle;
    enum InputStreamType type;
    char iseos;
    int (*read)();
    void (*close)();
    bool unclose;
};

struct ssl_stream {
    struct stream_buffer stream;
    struct ssl_handle* handle;
    enum InputStreamType type;
    char iseos;
    int (*read)();
    void (*close)();
    bool unclose;
};

struct encoded_stream {
    struct stream_buffer stream;
    struct ens_handle* handle;
    enum InputStreamType type;
    char iseos;
    int (*read)();
    void (*close)();
    bool unclose;
};

union input_stream {
    struct base_stream base;
    struct file_stream file;
    struct str_stream str;
    struct ssl_stream ssl;
    struct encoded_stream ens;
};

union input_stream* newInputStream(int des);
int ISclose(union input_stream* stream);
int ISgetc(union input_stream* stream);
Str StrISgets(union input_stream* stream);
Str StrmyISgets(union input_stream* stream);
int ISread_n(union input_stream* stream, char* dst, int bufsize);

// zig
void init_buffer(struct base_stream* base, char* buf, size_t bufsize);
