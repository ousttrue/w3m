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
  StreamType _type = {};
  void *_stream;
  std::shared_ptr<stream_buffer> _buffer;
  bool _iseos = false;

public:
  input_stream(StreamType type, void *stream, int bufsize,
               const unsigned char *data = nullptr);

  StreamType IStype() const { return _type; }
  int ISfileno() const;
  int ISread_n(char *dst, int bufsize);
  void ISgets_to_growbuf(struct growbuf *gb, char crnl);
  std::string StrISgets2(bool crnl);
  std::string StrISgets() { return StrISgets2(false); }
  std::string StrmyISgets() { return StrISgets2(true); }
  int ISundogetc();
  int ISgetc();
  int ISclose();

private:
  int read(unsigned char *buf, int size);
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
