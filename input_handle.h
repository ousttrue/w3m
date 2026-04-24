#pragma once
#include "input_stream.h"
#include <stdio.h>
#include "stream_encoding.h"
#include "StreamBuffer.h"

struct io_file_handle {
    FILE* f;
    /// fclose or pclose
    int (*close)(FILE*);
};

struct ssl_handle {
    struct ssl_st* ssl;
    int sock;
};

struct encoded_stream_handle {
    struct InputStream* is;
    struct growbuf *gb;
    int pos;
    enum StreamEncoding encoding;
};

union input_handle {
    int fd;
    struct io_file_handle file;
    struct ssl_handle ssl;
    struct encoded_stream_handle ens;
};

typedef int (*ReadFunc)(union input_handle* handle, uint8_t* buf, int size);
typedef void (*CloseFunc)(union input_handle* handle);

struct InputStream {
    enum InputStreamType type;
    struct StreamBuffer stream;
    bool iseos;
    bool unclose;
    ReadFunc read;
    CloseFunc close;
    union input_handle handle;
};
