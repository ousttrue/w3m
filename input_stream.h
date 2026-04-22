#pragma once
#include "growbuf.h"
#include "stream_encoding.h"
#include "StreamBuffer.h"
#include <stdio.h>

typedef int (*ReadFunc)(void* handle, uint8_t* buf, int size);
typedef void (*CloseFunc)(void* handle);

enum InputStreamType {
    IST_BUFFER = 0,
    IST_FD = 1,
    IST_FILE = 2,
    IST_SSL = 3,
    IST_ENCODED = 4,
};

#define IST_UNCLOSE 0x10

struct io_file_handle {
    FILE* f;
    int (*close)(FILE*);
};

struct ssl_handle {
    struct ssl_st* ssl;
    int sock;
};

struct encoded_stream_handle {
    struct InputStream* is;
    struct growbuf gb;
    int pos;
    enum StreamEncoding encoding;
};

struct InputStream {
    enum InputStreamType type;
    struct StreamBuffer stream;
    bool iseos;
    ReadFunc read;
    CloseFunc close;
    union {
        int fd;
        struct io_file_handle file;
        struct ssl_handle ssl;
        struct encoded_stream_handle ens;
    } handle;
};
static inline int ssl_socket_of(struct InputStream* stream)
{
    if (stream->type != IST_SSL) {
        return 0;
    }
    return stream->handle.ssl.sock;
}

struct InputStream* newInputStream(int des);
#define openIS(path) newInputStream(open((path), O_RDONLY))
struct InputStream* newFileStream(FILE* f, int (*closep)(FILE*));
struct InputStream* newStrStream(const char* s, int len);
struct InputStream* newSSLStream(struct ssl_st* ssl, int sock);
struct InputStream* newEncodedStream(struct InputStream* is, enum StreamEncoding encoding);
int ISclose(struct InputStream* stream);
int ISgetc(struct InputStream* stream);
int ISundogetc(struct InputStream* stream);
void ISgets_to_growbuf(struct InputStream* stream, struct growbuf* gb, char crnl);
int ISread_n(struct InputStream* stream, char* dst, int bufsize);
int ISfd(struct InputStream* stream);
int ISeos(struct InputStream* stream);
