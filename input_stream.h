#pragma once
#include "growbuf.h"
#include <fcntl.h>
#include <stdio.h>
#include <openssl/ssl.h>

struct stream_buffer {
    unsigned char* buf;
    int size, cur, next;
};

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

enum InputStreamType {
    IST_BASIC = 0,
    IST_FILE = 1,
    IST_STR = 2,
    IST_SSL = 3,
    IST_ENCODED = 4,
};

struct base_stream {
    struct stream_buffer stream;
    enum InputStreamType type;
    bool iseos;
    bool unclose;
    void* handle;
    int (*read)(int*, unsigned char*, int);
    int (*close)(int*);
};

struct file_stream {
    struct stream_buffer stream;
    enum InputStreamType type;
    bool iseos;
    bool unclose;
    FILE* handle;
    int (*read)(FILE*, char*, int);
    int (*close)(FILE*);
};

struct str_stream {
    struct stream_buffer stream;
    enum InputStreamType type;
    bool iseos;
    bool unclose;
    Str handle;
    int (*read)(Str, char*, int);
    int (*close)(int);
};

struct ssl_stream {
    struct stream_buffer stream;
    enum InputStreamType type;
    bool iseos;
    bool unclose;
    struct ssl_handle* handle;
    int (*read)(struct ssl_handle*, char*, int);
    int (*close)(struct ssl_handle*);
};

struct encoded_stream {
    struct stream_buffer stream;
    enum InputStreamType type;
    bool iseos;
    bool unclose;
    struct ens_handle* handle;
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
#define StrISgets(stream) StrISgets2(stream, false)
#define StrmyISgets(stream) StrISgets2(stream, true)
void ISgets_to_growbuf(union input_stream* stream, struct growbuf* gb, char crnl);
#ifdef unused
extern int ISread(union input_stream* stream, Str buf, int count);
#endif
int ISread_n(union input_stream* stream, unsigned char* dst, int bufsize);
extern int ISfileno(union input_stream* stream);
extern int ISeos(union input_stream* stream);
extern void ssl_accept_this_site(const char* hostname);
extern Str ssl_get_certificate(SSL* ssl, const char* hostname);

static inline enum InputStreamType IStype(union input_stream* s) { return (s->base.type); }
static inline void IStypeUnclose(union input_stream* s, bool unclose)
{
    s->base.unclose = unclose;
}
static inline bool ISisUnclose(union input_stream* s)
{
    return s->base.unclose;
}

#define iseos(stream) ((stream)->base.iseos)
#define str_of(stream) ((stream)->str.handle)
#define ssl_socket_of(stream) ((stream)->ssl.handle->sock)
#define ssl_of(stream) ((stream)->ssl.handle->ssl)

#define openIS(path) newInputStream(open((path), O_RDONLY))
