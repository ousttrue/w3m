#pragma once
#include "compression.h"
#include "url.h"
#include "growbuf.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <openssl/types.h>

extern char AutoUncompress;
extern char PreserveTimestamp;

struct stream_buffer {
    unsigned char* buf;
    int size, cur, next;
};

typedef int (*FileCloseFunc)(FILE*);
struct io_file_handle {
    FILE* f;
    FileCloseFunc close;
};

enum StreamEncoding {
    ENC_7BIT = 0,
    ENC_BASE64 = 1,
    ENC_QUOTE = 2,
    ENC_UUENCODE = 3,
};

union input_stream;
struct ens_handle {
    union input_stream* is;
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
struct base_stream {
    struct stream_buffer stream;
    void* handle;
    enum InputStreamType type;
    char iseos;
    int (*read)(void*, void*, int);
    void (*close)(void*);
};

struct file_stream {
    struct stream_buffer stream;
    struct io_file_handle* handle;
    enum InputStreamType type;
    char iseos;
    int (*read)();
    void (*close)();
};

struct str_stream {
    struct stream_buffer stream;
    Str handle;
    enum InputStreamType type;
    char iseos;
    int (*read)();
    void (*close)();
};

struct ssl_stream {
    struct stream_buffer stream;
    struct ssl_handle* handle;
    enum InputStreamType type;
    char iseos;
    int (*read)();
    void (*close)();
};

struct encoded_stream {
    struct stream_buffer stream;
    struct ens_handle* handle;
    enum InputStreamType type;
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

typedef struct base_stream* BaseStream;
typedef struct file_stream* FileStream;
typedef struct str_stream* StrStream;
typedef struct ssl_stream* SSLStream;
typedef struct encoded_stream* EncodedStrStream;

typedef union input_stream* InputStream;

InputStream newInputStream(int des);
inline static InputStream openIS(const char* path) { return newInputStream(open((path), O_RDONLY)); }
InputStream newFileStream(FILE* f, FileCloseFunc closep);
InputStream newStrStream(Str s);
InputStream newSSLStream(SSL* ssl, int sock);
InputStream newEncodedStream(InputStream is, enum StreamEncoding encoding);
int ISclose(InputStream stream);
int ISgetc(InputStream stream);
int ISundogetc(InputStream stream);
Str StrISgets2(InputStream stream, char crnl);
inline static Str StrISgets(union input_stream* stream) { return StrISgets2(stream, false); }
inline static Str StrmyISgets(union input_stream* stream) { return StrISgets2(stream, true); }
void ISgets_to_growbuf(InputStream stream, struct growbuf* gb, char crnl);
int ISread_n(InputStream stream, char* dst, int bufsize);
int ISfileno(InputStream stream);
bool ISeos(InputStream stream);
void ssl_accept_this_site(char* hostname);

inline static enum InputStreamType IStype(union input_stream* stream) { return ((stream)->base.type); }
inline static bool is_eos(union input_stream* stream) { return ISeos(stream); }
inline static bool iseos(union input_stream* stream) { return ((stream)->base.iseos); }
inline static FILE* file_of(union input_stream* stream) { return ((stream)->file.handle->f); }
inline static void set_close(union input_stream* stream, FileCloseFunc closep)
{
    if (IStype(stream) == IST_FILE) {
        stream->file.handle->close = closep;
    }
}
inline static Str str_of(union input_stream* stream) { return ((stream)->str.handle); }

int save2tmp(union input_stream* s, const char* tmpf);
Str readAll(union input_stream* stream);
int checkSaveFile(union input_stream* stream, const char* path);
