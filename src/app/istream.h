#pragma once
#include <stdio.h>
#include <memory>
#include <openssl/types.h>
#include <string>
#include <string_view>

enum StreamType {
  IST_BASIC = 0,
  IST_FILE = 1,
  IST_STR = 2,
  IST_SSL = 3,
  IST_ENCODED = 4,
};

enum EncodingType : char {
  ENC_7BIT = 0,
  ENC_BASE64 = 1,
  ENC_QUOTE = 2,
  ENC_UUENCODE = 3,
};

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

private:
  virtual int read(unsigned char *buf, int size) = 0;

public:
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
std::shared_ptr<input_stream> openIS(const std::string &path);
using FileClose = int (*)(FILE *);
std::shared_ptr<input_stream> newFileStream(FILE *f, FileClose closep);
std::shared_ptr<input_stream> newStrStream(std::string_view str);
std::shared_ptr<input_stream>
newEncodedStream(const std::shared_ptr<input_stream> &is,
                 EncodingType encoding);
