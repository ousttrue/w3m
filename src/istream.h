#pragma once
#include "mimehead.h"
#include "encoding.h"
#include <stdio.h>
#include <openssl/types.h>

struct stream_buffer {
  unsigned char *buf;
  int size;
  int cur;
  int next;
};

using ReadFunc = int (*)(void *userpointer, unsigned char *buffer, int size);
using CloseFunc = void (*)(void *userpointer);

struct io_file_handle {
  FILE *f;
  CloseFunc close;
};

union input_stream;

struct ens_handle {
  union input_stream *is;
  struct growbuf gb;
  int pos;
  EncodingType encoding;
};

enum StreamType {
  IST_BASIC = 0,
  IST_FILE = 1,
  IST_STR = 2,
  IST_SSL = 3,
  IST_ENCODED = 4,
};

struct base_stream {
  struct stream_buffer stream;
  void *handle;
  StreamType type;
  char iseos;
  ReadFunc read;
  CloseFunc close;
};

struct file_stream {
  struct stream_buffer stream;
  struct io_file_handle *handle;
  StreamType type;
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
  StreamType type;
  char iseos;
  ReadFunc read;
  CloseFunc close;
};

struct encoded_stream {
  struct stream_buffer stream;
  struct ens_handle *handle;
  StreamType type;
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

  StreamType IStype() const { return base.type; }
  int ISfileno() const;
  int ISread_n(char *dst, int bufsize);
  void ISgets_to_growbuf(struct growbuf *gb, char crnl);

  Str *StrISgets2(bool crnl);
  Str *StrISgets() { return StrISgets2(false); }
  Str *StrmyISgets() { return StrISgets2(true); }
  int ISundogetc();
  int ISgetc();
  int ISclose();

  input_stream *newEncodedStream(EncodingType encoding);
};

struct ssl_handle {
  SSL *ssl;
  int sock;
};
union input_stream;
input_stream *newSSLStream(SSL *ssl, int sock);

void init_base_stream(base_stream *base, int bufsize);

input_stream *newInputStream(int des);
input_stream *openIS(const char *path);
input_stream *newFileStream(FILE *f, void (*closep)());
input_stream *newStrStream(Str *s);
