#pragma once

#include <stdint.h>

#include "indep.h"
#include <stdio.h>
#ifdef USE_SSL
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

struct stream_buffer {
    unsigned char* buf;
    int size, cur, next;
};

typedef struct stream_buffer* StreamBuffer;

struct io_file_handle {
    FILE* f;
    void (*close)(void*);
};

#ifdef USE_SSL
struct ssl_handle {
    SSL* ssl;
    int sock;
};
#endif

union input_stream;

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

#ifdef USE_SSL
struct ssl_stream {
    struct stream_buffer stream;
    struct ssl_handle* handle;
    char type;
    char iseos;
    int (*read)();
    void (*close)();
};
#endif /* USE_SSL */

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
#ifdef USE_SSL
    struct ssl_stream ssl;
#endif /* USE_SSL */
    struct encoded_stream ens;
};

typedef struct base_stream* BaseStream;
typedef struct file_stream* FileStream;
typedef struct str_stream* StrStream;
#ifdef USE_SSL
typedef struct ssl_stream* SSLStream;
#endif /* USE_SSL */
typedef struct encoded_stream* EncodedStrStream;

typedef union input_stream* InputStream;

extern InputStream newInputStream(int des);
extern InputStream newFileStream(FILE* f, void (*closep)());
extern InputStream newStrStream(Str s);
#ifdef USE_SSL
extern InputStream newSSLStream(SSL* ssl, int sock);
#endif
extern int ISclose(InputStream stream);
extern int ISgetc(InputStream stream);
extern int ISundogetc(InputStream stream);
extern Str StrISgets2(InputStream stream, char crnl);
#define StrISgets(stream) StrISgets2(stream, FALSE)
#define StrmyISgets(stream) StrISgets2(stream, TRUE)
void ISgets_to_growbuf(InputStream stream, struct growbuf* gb, char crnl);
#ifdef unused
extern int ISread(InputStream stream, Str buf, int count);
#endif
int ISread_n(InputStream stream, char* dst, int bufsize);
extern int ISfileno(InputStream stream);
extern int ISeos(InputStream stream);
#ifdef USE_SSL
extern void ssl_accept_this_site(char* hostname);
extern Str ssl_get_certificate(SSL* ssl, char* hostname);
#endif

#define IST_BASIC 0
#define IST_FILE 1
#define IST_STR 2
#define IST_SSL 3
#define IST_ENCODED 4
#define IST_UNCLOSE 0x10
