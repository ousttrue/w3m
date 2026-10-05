#pragma once
#include "growbuf.h"
#include <fcntl.h>
#include <stdio.h>
#include <openssl/ssl.h>
#include <assert.h>

struct stream_buffer {
    unsigned char* buf;
    int size;
    int cur;
    int next;
};

struct ssl_handle {
    SSL* ssl;
    int sock;
};

enum StreamEncoding {
    ENC_7BIT,
    ENC_BASE64,
    ENC_QUOTE,
    ENC_UUENCODE,
};

struct ens_handle {
    struct input_stream* is;
    struct growbuf gb;
    int pos;
    enum StreamEncoding encoding;
};

enum InputStreamType {
    IST_BASIC = 0,
    IST_FILE = 1,
    IST_STR = 2,
    IST_SSL = 3,
    IST_ENCODED = 4,
};

typedef int (*InputStreamReadFunc)(void*, unsigned char*, int);
typedef void (*InputStreamCloseFunc)(void*);

struct input_stream {
    struct stream_buffer stream;
    enum InputStreamType type;
    bool iseos;
    bool unclose;
    // void* type eraser. use cast. no union
    InputStreamReadFunc read;
    InputStreamCloseFunc close;
    void* handle;
    // int* fd;
    // FILE* file;
    // pStr str;
    // struct ssl_handle* ssl;
    // struct ens_handle* ens;
};

extern struct input_stream* newInputStream(int des);
static inline struct input_stream* openIS(const char* path)
{
    return newInputStream(open((path), O_RDONLY));
}
typedef int (*FileCloseFunc)(FILE*);
extern struct input_stream* newFileStream(FILE* f, FileCloseFunc closep);
extern struct input_stream* newStrStream(pStr s);
extern struct input_stream* newSSLStream(SSL* ssl, int sock);
extern struct input_stream* newEncodedStream(struct input_stream* is, enum StreamEncoding encoding);
extern int ISclose(struct input_stream* stream);
extern int ISgetc(struct input_stream* stream);
extern int ISundogetc(struct input_stream* stream);
extern pStr StrISgets2(struct input_stream* stream, char crnl);
#define StrISgets(stream) StrISgets2(stream, false)
#define StrmyISgets(stream) StrISgets2(stream, true)
void ISgets_to_growbuf(struct input_stream* stream, struct growbuf* gb, char crnl);
int ISread_n(struct input_stream* stream, unsigned char* dst, int bufsize);
extern int ISfileno(struct input_stream* stream);
extern int ISeos(struct input_stream* stream);
extern void ssl_accept_this_site(const char* hostname);
extern pStr ssl_get_certificate(SSL* ssl, const char* hostname);
static inline int ssl_socket_of(struct input_stream* s)
{
    assert(s->type == IST_SSL);
    return ((struct ssl_handle*)s->handle)->sock;
}
bool canSaveFile(struct input_stream* stream, const char* path);
