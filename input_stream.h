#pragma once
#include <w3m.h>
#include "growbuf.h"
#include "stream_encoding.h"
#include "StreamBuffer.h"

#include <libwc/wc_types.h>

#include <stdio.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

typedef int (*ReadFunc)(void* handle, uint8_t* buf, int size);
typedef void (*CloseFunc)(void* handle);

enum InputStreamType {
    IST_BASIC = 0,
    IST_FILE = 1,
    IST_STR = 2,
    IST_SSL = 3,
    IST_ENCODED = 4,
};

#define IST_UNCLOSE 0x10

struct BaseStream {
    enum InputStreamType type;
    struct StreamBuffer stream;
    bool iseos;
    ReadFunc read;
    CloseFunc close;
    void* handle;
};

struct io_file_handle {
    FILE* f;
    int (*close)(FILE*);
};
struct FileStream {
    enum InputStreamType type;
    struct StreamBuffer stream;
    bool iseos;
    ReadFunc read;
    CloseFunc close;
    struct io_file_handle* handle;
};

struct StrStream {
    enum InputStreamType type;
    struct StreamBuffer stream;
    bool iseos;
    ReadFunc read;
    CloseFunc close;
    void* handle;
};

struct ssl_handle {
    SSL* ssl;
    int sock;
};
struct SslStream {
    enum InputStreamType type;
    struct StreamBuffer stream;
    bool iseos;
    ReadFunc read;
    CloseFunc close;
    struct ssl_handle* handle;
};

struct ens_handle {
    union input_stream* is;
    struct growbuf gb;
    int pos;
    char encoding;
};
struct EncodedStream {
    enum InputStreamType type;
    struct StreamBuffer stream;
    bool iseos;
    ReadFunc read;
    CloseFunc close;
    struct ens_handle* handle;
};

union input_stream {
    struct BaseStream base;
    struct FileStream file;
    struct StrStream str;
    struct SslStream ssl;
    struct EncodedStream ens;
};

typedef union input_stream* InputStream;

InputStream newInputStream(int des);
#define openIS(path) newInputStream(open((path), O_RDONLY))
InputStream newFileStream(FILE* f, void (*closep)());
InputStream newStrStream(const char* s, int len);
InputStream newSSLStream(SSL* ssl, int sock);
InputStream newEncodedStream(InputStream is, enum StreamEncoding encoding);
int ISclose(InputStream stream);
int ISgetc(InputStream stream);
int ISundogetc(InputStream stream);
void ISgets_to_growbuf(InputStream stream, struct growbuf* gb, char crnl);
int ISread_n(InputStream stream, char* dst, int bufsize);
int ISfileno(InputStream stream);
int ISeos(InputStream stream);

#define IStype(stream) ((stream)->base.type)
#define iseos(stream) ((stream)->base.iseos)
#define ssl_socket_of(stream) ((stream)->ssl.handle->sock)
