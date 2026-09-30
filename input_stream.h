#pragma once
#include "config.h"
#include "indep.h"
#include "growbuf.h"
#include <fcntl.h>
#include <stdio.h>
#include <openssl/ssl.h>

struct stream_buffer {
    unsigned char* buf;
    int size, cur, next;
};

typedef struct stream_buffer* StreamBuffer;

struct ssl_handle {
    SSL* ssl;
    int sock;
};

struct ens_handle {
    union input_stream* is;
    struct growbuf gb;
    int pos;
    char encoding;
};

struct base_stream {
    struct stream_buffer stream;
    void* handle;
    char type;
    char iseos;
    int (*read)(int*, unsigned char*, int);
    int (*close)(int*);
};

struct file_stream {
    struct stream_buffer stream;
    FILE* handle;
    char type;
    char iseos;
    int (*read)(FILE*, char*, int);
    int (*close)(FILE*);
};

struct str_stream {
    struct stream_buffer stream;
    Str handle;
    char type;
    char iseos;
    int (*read)(Str, char*, int);
    int (*close)(int);
};

struct ssl_stream {
    struct stream_buffer stream;
    struct ssl_handle* handle;
    char type;
    char iseos;
    int (*read)(struct ssl_handle*, char*, int);
    int (*close)(struct ssl_handle*);
};

struct encoded_stream {
    struct stream_buffer stream;
    struct ens_handle* handle;
    char type;
    char iseos;
    int (*read)(struct ens_handle*, char*, int);
    int (*close)(struct ens_handle*);
};

union input_stream {
    struct base_stream base;
    struct file_stream file;
    struct str_stream str;
    struct ssl_stream ssl;
    struct encoded_stream ens;
};

typedef struct base_stream* BaseStream;

extern union input_stream* newInputStream(int des);
extern union input_stream* newFileStream(FILE* f, int (*closep)(FILE*));
extern union input_stream* newStrStream(Str s);
extern union input_stream* newSSLStream(SSL* ssl, int sock);
extern union input_stream* newEncodedStream(union input_stream* is, char encoding);
extern int ISclose(union input_stream* stream);
extern int ISgetc(union input_stream* stream);
extern int ISundogetc(union input_stream* stream);
extern Str StrISgets2(union input_stream* stream, char crnl);
#define StrISgets(stream) StrISgets2(stream, FALSE)
#define StrmyISgets(stream) StrISgets2(stream, TRUE)
void ISgets_to_growbuf(union input_stream* stream, struct growbuf* gb, char crnl);
#ifdef unused
extern int ISread(union input_stream* stream, Str buf, int count);
#endif
int ISread_n(union input_stream* stream, unsigned char* dst, int bufsize);
extern int ISfileno(union input_stream* stream);
extern int ISeos(union input_stream* stream);
extern void ssl_accept_this_site(const char* hostname);
extern Str ssl_get_certificate(SSL* ssl, const char* hostname);

#define IST_BASIC 0
#define IST_FILE 1
#define IST_STR 2
#define IST_SSL 3
#define IST_ENCODED 4
#define IST_UNCLOSE 0x10

#define IStype(stream) ((stream)->base.type)
#define iseos(stream) ((stream)->base.iseos)
#define str_of(stream) ((stream)->str.handle)
#define ssl_socket_of(stream) ((stream)->ssl.handle->sock)
#define ssl_of(stream) ((stream)->ssl.handle->ssl)

#define openIS(path) newInputStream(open((path), O_RDONLY))
