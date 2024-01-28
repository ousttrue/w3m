/* $Id: istream.h,v 1.12 2003/10/20 16:41:56 ukai Exp $ */
#ifndef IO_STREAM_H
#define IO_STREAM_H

#include "indep.h"
#include <stdio.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

struct stream_buffer {
  unsigned char *buf;
  int size, cur, next;
};

typedef int (*ReadFunc)(void *userpointer, unsigned char *buffer, int size);
typedef void (*CloseFunc)(void *userpointer);

struct io_file_handle {
  FILE *f;
  CloseFunc close;
};

struct ssl_handle {
  SSL *ssl;
  int sock;
};

union input_stream;

struct ens_handle {
  union input_stream *is;
  struct growbuf gb;
  int pos;
  char encoding;
};

struct base_stream {
  struct stream_buffer stream;
  void *handle;
  char type;
  char iseos;
  ReadFunc read;
  CloseFunc close;
};

struct file_stream {
  struct stream_buffer stream;
  struct io_file_handle *handle;
  char type;
  char iseos;
  ReadFunc read;
  CloseFunc close;
};

struct str_stream {
  struct stream_buffer stream;
  Str *handle;
  char type;
  char iseos;
  ReadFunc read;
  CloseFunc close;
};

struct ssl_stream {
  struct stream_buffer stream;
  struct ssl_handle *handle;
  char type;
  char iseos;
  ReadFunc read;
  CloseFunc close;
};

struct encoded_stream {
  struct stream_buffer stream;
  struct ens_handle *handle;
  char type;
  char iseos;
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

extern input_stream* newInputStream(int des);
extern input_stream* newFileStream(FILE *f, void (*closep)());
extern input_stream* newStrStream(Str *s);
extern input_stream* newSSLStream(SSL *ssl, int sock);
extern input_stream* newEncodedStream(input_stream* is, char encoding);
extern int ISclose(input_stream* stream);
extern int ISgetc(input_stream* stream);
extern int ISundogetc(input_stream* stream);
extern Str *StrISgets2(input_stream* stream, char crnl);
#define StrISgets(stream) StrISgets2(stream, FALSE)
#define StrmyISgets(stream) StrISgets2(stream, TRUE)
void ISgets_to_growbuf(input_stream* stream, struct growbuf *gb, char crnl);
#ifdef unused
extern int ISread(input_stream* stream, Str *buf, int count);
#endif
int ISread_n(input_stream* stream, char *dst, int bufsize);
extern int ISfileno(input_stream* stream);
extern int ISeos(input_stream* stream);
extern void ssl_accept_this_site(char *hostname);
extern Str *ssl_get_certificate(SSL *ssl, char *hostname);

#define IST_BASIC 0
#define IST_FILE 1
#define IST_STR 2
#define IST_SSL 3
#define IST_ENCODED 4
#define IST_UNCLOSE 0x10

#define IStype(stream) ((stream)->base.type)
#define is_eos(stream) ISeos(stream)
#define iseos(stream) ((stream)->base.iseos)
#define file_of(stream) ((stream)->file.handle->f)
#define set_close(stream, closep)                                              \
  ((IStype(stream) == IST_FILE) ? ((stream)->file.handle->close = (closep)) : 0)
#define str_of(stream) ((stream)->str.handle)
#define ssl_socket_of(stream) ((stream)->ssl.handle->sock)
#define ssl_of(stream) ((stream)->ssl.handle->ssl)

#define openIS(path) newInputStream(open((path), O_RDONLY))
#endif
