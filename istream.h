#pragma once

#include "growbuf.h"
#include "encoding_type.h"
#include <stdio.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

///
/// input_stream
///
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

typedef struct base_stream *BaseStream;
typedef struct file_stream *FileStream;
typedef struct str_stream *StrStream;
typedef struct ssl_stream *SSLStream;
typedef struct encoded_stream *EncodedStrStream;

typedef union input_stream *InputStream;

extern InputStream newInputStream(int des);
extern InputStream newFileStream(FILE *f, void (*closep)());
extern InputStream newStrStream(Str s);
extern InputStream newSSLStream(SSL *ssl, int sock);
extern InputStream newEncodedStream(InputStream is,
                                    enum ENCODING_TYPE encoding);
extern int ISclose(InputStream stream);
extern int ISgetc(InputStream stream);
extern int ISundogetc(InputStream stream);
extern Str StrISgets2(InputStream stream, char crnl);
#define StrISgets(stream) StrISgets2(stream, false)
#define StrmyISgets(stream) StrISgets2(stream, true)
void ISgets_to_growbuf(InputStream stream, struct growbuf *gb, char crnl);
#ifdef unused
extern int ISread(InputStream stream, Str buf, int count);
#endif
int ISread_n(InputStream stream, char *dst, int bufsize);
extern int ISfileno(InputStream stream);
extern int ISeos(InputStream stream);
extern void ssl_accept_this_site(char *hostname);
extern Str ssl_get_certificate(SSL *ssl, char *hostname);

enum IST_TYPE IStype(union input_stream *stream);
// #define is_eos(stream) ISeos(stream)
// #define iseos(stream) ((stream)->base.iseos)
// #define file_of(stream) ((stream)->file.handle->f)
// #define set_close(stream, closep) \
//   ((IStype(stream) == IST_FILE) ? ((stream)->file.handle->close = (closep)) :
//   0)
// #define str_of(stream) ((stream)->str.handle)

int ssl_socket_of(union input_stream *stream);
union input_stream *openIS(const char *path);
