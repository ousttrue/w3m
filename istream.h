#pragma once
#include "indep.h" // growbuf
#include "ssl_stream.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>

#define IST_UNCLOSE 0x10

typedef struct stream_buffer* StreamBuffer;

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

typedef struct base_stream* BaseStream;
typedef struct file_stream* FileStream;
typedef struct str_stream* StrStream;
typedef struct encoded_stream* EncodedStrStream;

typedef union input_stream* InputStream;

extern InputStream newInputStream(int des);
extern InputStream newFileStream(FILE* f, void (*closep)());
extern InputStream newStrStream(Str s);
#ifdef USE_SSL
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
#endif

void init_base_stream(BaseStream base, int bufsize);
