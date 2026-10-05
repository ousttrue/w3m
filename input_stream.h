#pragma once
#include "Str.h"
#include <openssl/types.h>
#include <stdio.h>

enum InputStreamType {
    IST_BASIC = 0,
    IST_FILE = 1,
    IST_STR = 2,
    IST_SSL = 3,
    IST_ENCODED = 4,
};

enum StreamEncoding {
    ENC_7BIT,
    ENC_BASE64,
    ENC_QUOTE,
    ENC_UUENCODE,
};

struct stream_buffer {
    unsigned char* buf;
    int size;
    int cur;
    int next;
};

typedef int (*InputStreamReadFunc)(void*, unsigned char*, int);
typedef void (*InputStreamCloseFunc)(void*);

struct input_stream {
    struct stream_buffer stream;
    enum InputStreamType type;
    bool iseos;
    bool unclose;
    InputStreamReadFunc read;
    InputStreamCloseFunc close;
    void* handle;
};

extern struct input_stream* IS_newFD(int des);
struct input_stream* IS_open(const char* path);
typedef int (*IS_fcloseFunc)(FILE*);
extern struct input_stream* IS_newFile(FILE* f, IS_fcloseFunc closep);
extern struct input_stream* IS_newCharpN(const char* ptr, int len);
inline static struct input_stream* IS_newStr(pStr str)
{
    return IS_newCharpN(str->ptr, str->len);
}
extern struct input_stream* IS_newSSL(SSL* ssl, int sock);
extern struct input_stream* IS_newEncoded(struct input_stream* is, enum StreamEncoding encoding);
extern int IS_close(struct input_stream* stream);
extern int IS_getc(struct input_stream* stream);
extern int IS_ungetc(struct input_stream* stream);
extern pStr IS_gets(struct input_stream* stream, bool crlf);
struct growbuf;
void IS_read2growbuf(struct input_stream* stream, struct growbuf* gb, char crnl);
int IS_read(struct input_stream* stream, unsigned char* dst, int bufsize);
extern int IS_FD(struct input_stream* stream);
extern int IS_isEnd(struct input_stream* stream);
extern void ssl_accept_this_site(const char* hostname);
extern struct Str ssl_get_certificate(SSL* ssl, const char* hostname);
int IS_ssl_socket(struct input_stream* s);
bool IS_canSaveTo(struct input_stream* stream, const char* path);
