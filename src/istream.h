#pragma once
#include "encoding.h"
#include <stdio.h>
#include <memory>
#include <openssl/types.h>

enum StreamType {
  IST_BASIC = 0,
  IST_FILE = 1,
  IST_STR = 2,
  IST_SSL = 3,
  IST_ENCODED = 4,
};

struct stream_buffer {
  unsigned char *buf;
  int size;
  int cur = 0;
  int next;
  stream_buffer(int bufsize, const unsigned char *buf = nullptr);
};

struct Str;
class input_stream {
  StreamType _type = {};
  void *_stream;
  stream_buffer _buffer;
  bool _iseos = false;

public:
  input_stream(StreamType type, void *stream, int bufsize,
               const unsigned char *data = nullptr)
      : _type(type), _stream(stream), _buffer(bufsize, data) {}
  bool MUST_BE_UPDATED() const;
  void do_update();
  char POP_CHAR();

  StreamType IStype() const { return _type; }
  int read(unsigned char *buf, int size);
  int ISfileno() const;
  int ISread_n(char *dst, int bufsize);
  void ISgets_to_growbuf(struct growbuf *gb, char crnl);
  Str *StrISgets2(bool crnl);
  Str *StrISgets() { return StrISgets2(false); }
  Str *StrmyISgets() { return StrISgets2(true); }
  int ISundogetc();
  int ISgetc();
  int ISclose();
};

std::shared_ptr<input_stream> newInputStream(int des);
std::shared_ptr<input_stream> newSSLStream(SSL *ssl, int sock);
std::shared_ptr<input_stream> openIS(const char *path);
using FileClose = int (*)(FILE *);
std::shared_ptr<input_stream> newFileStream(FILE *f, FileClose closep);
std::shared_ptr<input_stream> newStrStream(Str *s);
std::shared_ptr<input_stream>
newEncodedStream(const std::shared_ptr<input_stream> &is,
                 EncodingType encoding);
