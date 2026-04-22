#pragma once
#include <w3m.h>
#include "growbuf.h"
#include "stream_encoding.h"

#include <libwc/wc_types.h>

#include <stdio.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

struct StreamBuffer {
    uint8_t* buf;
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

struct BaseStream {
    struct StreamBuffer stream;
    void* handle;
    char type;
    char iseos;
    int (*read)(void*, void*, int);
    void (*close)(void*);
};

struct FileStream {
    struct StreamBuffer stream;
    struct io_file_handle* handle;
    char type;
    char iseos;
    int (*read)();
    void (*close)();
};

struct StrStream {
    struct StreamBuffer stream;
    void* handle;
    char type;
    char iseos;
    int (*read)();
    void (*close)();
};

struct SslStream {
    struct StreamBuffer stream;
    struct ssl_handle* handle;
    char type;
    char iseos;
    int (*read)();
    void (*close)();
};

struct EncodedStream {
    struct StreamBuffer stream;
    struct ens_handle* handle;
    char type;
    char iseos;
    int (*read)();
    void (*close)();
};

union input_stream {
    struct BaseStream base;
    struct FileStream file;
    struct StrStream str;
    struct SslStream ssl;
    struct EncodedStream ens;
};

typedef union input_stream* InputStream;

extern InputStream newInputStream(int des);
extern InputStream newFileStream(FILE* f, void (*closep)());
extern InputStream newStrStream(const char* s, int len);
extern InputStream newSSLStream(SSL* ssl, int sock);
extern InputStream newEncodedStream(InputStream is, enum StreamEncoding encoding);
extern int ISclose(InputStream stream);
extern int ISgetc(InputStream stream);
extern int ISundogetc(InputStream stream);

void ISgets_to_growbuf(InputStream stream, struct growbuf* gb, char crnl);
int ISread_n(InputStream stream, char* dst, int bufsize);
extern int ISfileno(InputStream stream);
extern int ISeos(InputStream stream);
extern void ssl_accept_this_site(const char* hostname);

#define IST_BASIC 0
#define IST_FILE 1
#define IST_STR 2
#define IST_SSL 3
#define IST_ENCODED 4
#define IST_UNCLOSE 0x10

#define IStype(stream) ((stream)->base.type)
#define is_eos(stream) ISeos(stream)
#define iseos(stream) ((stream)->base.iseos)
#define file_of(stream) ((stream)->file.handle->f)
#define set_close(stream, closep) ((IStype(stream) == IST_FILE) ? ((stream)->file.handle->close = (closep)) : 0)
#define str_of(stream) ((stream)->str.handle)
#define ssl_socket_of(stream) ((stream)->ssl.handle->sock)
#define ssl_of(stream) ((stream)->ssl.handle->ssl)
#define openIS(path) newInputStream(open((path), O_RDONLY))
