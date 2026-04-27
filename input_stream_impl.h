#pragma once
#include "input_stream.h"
#include <openssl/ssl.h>
#include "StreamBuffer.h"
#include "growbuf.h"

struct input_stream_base {
    struct StreamBuffer stream;
    bool iseos;
    bool unclose;
    struct growbuf* linebuf;
};

static inline int POP_CHAR(struct input_stream_base* bs)
{
    if (!bs->iseos) {
        if (bs->stream.cur < bs->stream.length) {
            return bs->stream.buf[bs->stream.cur++];
        }
    }
    return '\0';
}

struct InputStream {
    enum InputStreamType type;
    struct input_stream_base base;
    void* handle;
};

//
// BUFFER
//
struct InputStream* ist_from_buffer(const char* s, int len);

//
// FD
//
struct input_stream_fd;
struct InputStream* ist_from_fd(int des);
struct InputStream* ist_from_path(const char* path);
void fd_close(struct input_stream_fd* handle);
int fd_read(struct input_stream_fd* handle, uint8_t* buf, int len);
int fd_fd(struct input_stream_fd* handle);

//
// FILE
//
struct input_stream_fp;
struct InputStream* ist_from_fp(FILE* f, FpCloseFunc func);
void fp_close(struct input_stream_fp* handle);
int fp_read(struct input_stream_fp* handle, uint8_t* buf, int len);
int fp_fd(struct input_stream_fp* handle);

//
// SOCK
//
struct input_stream_sock;
struct InputStream* ist_from_socket(int sock, SSL* ssl);
void sock_close(struct input_stream_sock* handle);
int sock_read(struct input_stream_sock* handle, uint8_t* buf, int len);
int sock_fd(struct input_stream_sock* handle);

//
// ENCODED
//
struct input_stream_encoded;
struct InputStream* ist_decode(struct InputStream* is, enum StreamEncoding encoding);
void ens_close(struct input_stream_encoded* handle);
int ens_read(struct input_stream_encoded* handle, uint8_t* buf, int len);
int ens_fd(struct input_stream_encoded* handle);
