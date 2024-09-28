#pragma once
#include "Str.h"
#include "encoding_type.h"
#include <openssl/types.h>

enum IST_TYPE {
  IST_BASIC = 0,
  IST_FILE = 1,
  IST_STR = 2,
  IST_SSL = 3,
  IST_ENCODED = 4,
};
union input_stream;

struct stream_buffer {
  unsigned char *buf;
  int size, cur, next;
};

typedef int (*ReadFunc)(void *handle, unsigned char *buf, int size);
typedef void (*CloseFunc)(void *handle);

struct base_stream {
  struct stream_buffer stream;
  void *handle;
  enum IST_TYPE type;
  // ftp ?
  bool unclose;
  bool iseos;
  ReadFunc read;
  CloseFunc close;
};

struct file_stream {
  struct stream_buffer stream;
  struct io_file_handle *handle;
  enum IST_TYPE type;
  bool unclose;
  bool iseos;
  ReadFunc read;
  CloseFunc close;
};

struct str_stream {
  struct stream_buffer stream;
  Str handle;
  enum IST_TYPE type;
  bool unclose;
  bool iseos;
  ReadFunc read;
  CloseFunc close;
};

struct ssl_stream {
  struct stream_buffer stream;
  struct ssl_handle *handle;
  enum IST_TYPE type;
  bool unclose;
  bool iseos;
  ReadFunc read;
  CloseFunc close;
};

struct encoded_stream {
  struct stream_buffer stream;
  struct ens_handle *handle;
  enum IST_TYPE type;
  bool unclose;
  bool iseos;
  ReadFunc read;
  CloseFunc close;
};

union input_stream {
  struct base_stream base;
  struct file_stream file;
  struct str_stream str;
  struct ssl_stream ssl;
  struct encoded_stream ens;
};

union input_stream *newInputStream(int des);
union input_stream *newFileStream(FILE *f, void (*closep)());
union input_stream *newStrStream(Str s);
union input_stream *newSSLStream(SSL *ssl, int sock);
union input_stream *newEncodedStream(union input_stream *is,
                                     enum ENCODING_TYPE encoding);
int ISclose(union input_stream *stream);

int ISgetc(union input_stream *stream);
int ISundogetc(union input_stream *stream);
Str StrISgets2(union input_stream *stream, char crnl);
#define StrISgets(stream) StrISgets2(stream, false)
#define StrmyISgets(stream) StrISgets2(stream, true)
struct growbuf;
void ISgets_to_growbuf(union input_stream *stream, struct growbuf *gb,
                       char crnl);
int ISread_n(union input_stream *stream, char *dst, int bufsize);
int ISfileno(union input_stream *stream);
int ISeos(union input_stream *stream);
void ssl_accept_this_site(char *hostname);
Str ssl_get_certificate(SSL *ssl, char *hostname);
enum IST_TYPE IStype(union input_stream *stream);
int ssl_socket_of(union input_stream *stream);
union input_stream *openIS(const char *path);
