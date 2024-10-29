#pragma once
#include "text/Str.h"
#include <openssl/types.h>
#include <stdint.h>

extern Str header_string;

enum IST_TYPE {
  IST_BASIC = 0,
  IST_FILE = 1,
  IST_STR = 2,
  IST_SSL = 3,
  IST_ENCODED = 4,
#ifdef _WIN32
  IST_WS = 5,
#endif
};

typedef int (*ReadFunc)(void *handle, unsigned char *buf, int size);
typedef void (*CloseFunc)(void *handle);

union input_stream;
union input_stream *newInputStream(int des);
union input_stream *newFileStream(FILE *f, int (*closep)(FILE *));
union input_stream *newStrStream(Str s);
#ifdef _WIN32
#include <winsock2.h>
union input_stream *newWinsockStream(SOCKET sock);
#endif
union input_stream *newSSLStream(SSL *ssl, int sock);

int ISclose(union input_stream *stream);
int ISgetc(union input_stream *stream);
int ISundogetc(union input_stream *stream);
Str StrISgets2(union input_stream *stream, char crnl);
Str StrISgets(union input_stream *stream);
Str StrmyISgets(union input_stream *stream);
Str StrISreadAll(union input_stream *stream);
int ISread_n(union input_stream *stream, char *dst, int bufsize);
int ISfileno(union input_stream *stream);
int ISeos(union input_stream *stream);
enum IST_TYPE IStype(union input_stream *stream);
union input_stream *openIS(const char *path);
void ssl_accept_this_site(const char *hostname);
int ssl_socket_of(union input_stream *stream);
const char *ssl_certificate(union input_stream *stream);
void ssl_set_certificate(union input_stream *stream,
                         const char *ssl_certificate);
union input_stream;
const char *guessContentType(const char *filename);
union input_stream *examineFile(const char *path);

int save2tmp(union input_stream *stream, const char *tmpf);
void close_for_ftp(union input_stream *is);
union input_stream *newInputFtp(int sock);
