#pragma once
#include "encoding.h"
#include "input_stream.h"
#include <stdio.h>
#include <memory>
#include <openssl/types.h>
#include <string>
#include <string_view>

struct stream_buffer;
class input_stream {
  stream_buffer *_buffer;
  bool _iseos = false;

protected:
  input_stream(int bufsize, const unsigned char *data = nullptr);

public:
  virtual ~input_stream();
  input_stream(const input_stream &) = delete;
  input_stream &operator=(const input_stream &) = delete;
  virtual int read(unsigned char *buf, int size) = 0;
  virtual void close() = 0;
  virtual StreamType IStype() const = 0;
  virtual int ISfileno() const = 0;

  int ISread_n(char *dst, int bufsize);
  void ISgets_to_growbuf(struct growbuf *gb, char crnl);
  std::string StrISgets2(bool crnl);
  std::string StrISgets() { return StrISgets2(false); }
  std::string StrmyISgets() { return StrISgets2(true); }
  int ISundogetc();
  int ISgetc();
};

std::shared_ptr<input_stream> newInputStream(int des);
std::shared_ptr<input_stream> newSSLStream(SSL *ssl, int sock);
std::shared_ptr<input_stream> openIS(const char *path);
using FileClose = int (*)(FILE *);
std::shared_ptr<input_stream> newFileStream(FILE *f, FileClose closep);
std::shared_ptr<input_stream> newStrStream(std::string_view str);
std::shared_ptr<input_stream>
newEncodedStream(const std::shared_ptr<input_stream> &is,
                 EncodingType encoding);
