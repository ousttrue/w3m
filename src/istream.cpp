#include "istream.h"
#include "Str.h"
#include "mimehead.h"
#include <functional>
#include <openssl/ssl.h>
#include <stdio.h>
#include <fcntl.h>
#include <vector>

#define STREAM_BUF_SIZE 8192

struct stream_buffer {
  std::vector<unsigned char> buf;
  int cur = 0;
  int next = 0;
  stream_buffer(int bufsize, const unsigned char *data) : buf(bufsize) {
    this->buf.resize(bufsize);
    if (data) {
      this->buf.assign(data, data + bufsize);
      this->next = bufsize;
    } else {
      this->next = 0;
    }
  }
  bool MUST_BE_UPDATED() const { return this->cur == this->next; }
  int do_update(const std::function<int(unsigned char *buf, int)> &reader) {
    this->cur = this->next = 0;
    int len = reader(this->buf.data(), this->buf.size());
    if (len <= 0) {
    } else {
      this->next += len;
    }
    return len;
  }
  char POP_CHAR() { return this->buf[this->cur++]; }
};

input_stream::input_stream(int bufsize, const unsigned char *data)
    : _buffer(new stream_buffer(bufsize, data)) {}

input_stream::~input_stream() { delete _buffer; }

//
// constructor
//
struct base_stream : public input_stream {
  int _des;
  base_stream(int des) : input_stream(STREAM_BUF_SIZE), _des(des) {}
  StreamType IStype() const override { return IST_BASIC; }
  int ISfileno() const override { return _des; }
  void close() override {
    if (_des >= 0) {
      ::close(_des);
      _des = -1;
    }
  }
  int read(unsigned char *buf, int len) override {
    return ::read(_des, buf, len);
  }
};

std::shared_ptr<input_stream> newInputStream(int des) {
  if (des < 0) {
    return {};
  }
  return std::make_shared<base_stream>(des);
}

struct file_stream : public input_stream {
  FILE *_file;
  FileClose _close;
  file_stream(FILE *file, FileClose close)
      : input_stream(STREAM_BUF_SIZE), _file(file), _close(close) {}
  StreamType IStype() const override { return IST_FILE; }
  int ISfileno() const override { return fileno(_file); }
  void close() override {
    if (_file) {
      this->_close(_file);
      _file = {};
    }
  }
  int read(unsigned char *buf, int len) override {
    return fread(buf, 1, len, _file);
  }
};

std::shared_ptr<input_stream> newFileStream(FILE *f, FileClose closep) {
  if (!f) {
    return {};
  }
  return std::make_shared<file_stream>(f, closep ? closep : fclose);
}

std::shared_ptr<input_stream> openIS(const char *path) {
  return newInputStream(open(path, O_RDONLY));
};

struct str_stream : public input_stream {
  str_stream(std::string_view s)
      : input_stream(s.size(), (const unsigned char *)s.data()) {}
  StreamType IStype() const override { return IST_STR; }
  int ISfileno() const override { return -1; }
  void close() override {}
  int read(unsigned char *buf, int len) override { return 0; }
};

std::shared_ptr<input_stream> newStrStream(std::string_view s) {
  if (s.empty()) {
    return {};
  }
  return std::make_shared<str_stream>(s);
}

struct ssl_stream : public input_stream {
  SSL *_ssl;
  int _sock;
  ssl_stream(SSL *ssl, int sock)
      : input_stream(STREAM_BUF_SIZE), _ssl(ssl), _sock(sock) {}
  StreamType IStype() const override { return IST_SSL; }
  int ISfileno() const override { return _sock; }
  void close() override {
    if (this->_sock) {
      ::close(this->_sock);
      this->_sock = 0;
    }
    if (this->_ssl) {
      SSL_free(this->_ssl);
      this->_ssl = {};
    }
  }
  int read(unsigned char *buf, int len) override {
    int status;
    if (this->_ssl) {
      for (;;) {
        status = SSL_read(this->_ssl, buf, len);
        if (status > 0)
          break;
        switch (SSL_get_error(this->_ssl, status)) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE: /* reads can trigger write errors; see
                                      SSL_get_error(3) */
          continue;
        default:
          break;
        }
        break;
      }
    } else {
      status = ::read(this->_sock, buf, len);
    }
    return status;
  }
};

#define SSL_BUF_SIZE 1536
std::shared_ptr<input_stream> newSSLStream(SSL *ssl, int sock) {
  if (sock < 0) {
    return {};
  }
  return std::make_shared<ssl_stream>(ssl, sock);
}

static void memchop(char *p, int *len) {
  char *q;
  for (q = p + *len; q > p; --q) {
    if (q[-1] != '\n' && q[-1] != '\r')
      break;
  }
  if (q != p + *len)
    *q = '\0';
  *len = q - p;
  return;
}

struct encoded_stream : public input_stream {
  std::shared_ptr<input_stream> is;
  struct growbuf gb;
  int pos = 0;
  EncodingType encoding;

  encoded_stream(const std::shared_ptr<input_stream> &is, EncodingType encoding)
      : input_stream(STREAM_BUF_SIZE), is(is), encoding(encoding) {
    growbuf_init_without_GC(&this->gb);
  }
  StreamType IStype() const override { return IST_ENCODED; }
  int ISfileno() const override { return is->ISfileno(); }
  void close() override {
    this->is->close();
    growbuf_clear(&this->gb);
  }
  int read(unsigned char *buf, int len) override {
    if (this->pos == this->gb.length) {
      this->is->ISgets_to_growbuf(&this->gb, true);
      if (this->gb.length == 0) {
        return 0;
      }
      if (this->encoding == ENC_BASE64) {
        memchop(this->gb.ptr, &this->gb.length);
      } else if (this->encoding == ENC_UUENCODE) {
        if (this->gb.length >= 5 && !strncmp(this->gb.ptr, "begin", 5)) {
          this->is->ISgets_to_growbuf(&this->gb, true);
        }
        memchop(this->gb.ptr, &this->gb.length);
      }

      struct growbuf gbtmp;
      growbuf_init_without_GC(&gbtmp);
      auto p = this->gb.ptr;
      if (this->encoding == ENC_QUOTE)
        decodeQP_to_growbuf(&gbtmp, &p);
      else if (this->encoding == ENC_BASE64)
        decodeB_to_growbuf(&gbtmp, &p);
      else if (this->encoding == ENC_UUENCODE)
        decodeU_to_growbuf(&gbtmp, &p);
      growbuf_clear(&this->gb);
      this->gb = gbtmp;
      this->pos = 0;
    }

    if (len > this->gb.length - this->pos)
      len = this->gb.length - this->pos;

    memcpy(buf, &this->gb.ptr[this->pos], len);
    this->pos += len;
    return len;
  }
};

std::shared_ptr<input_stream>
newEncodedStream(const std::shared_ptr<input_stream> &is,
                 EncodingType encoding) {
  if ((encoding != ENC_QUOTE && encoding != ENC_BASE64 &&
       encoding != ENC_UUENCODE)) {
    return is;
  }
  return std::make_shared<encoded_stream>(is, encoding);
}

int input_stream::ISgetc() {
  if (!this->_iseos && this->_buffer->MUST_BE_UPDATED()) {
    if (this->_buffer->do_update(std::bind(&input_stream::read, this,
                                           std::placeholders::_1,
                                           std::placeholders::_2)) <= 0) {
      this->_iseos = true;
    }
  }
  if (this->_iseos) {
    return '\0';
  } else {
    return this->_buffer->POP_CHAR();
  }
}

int input_stream::ISundogetc() {
  if (this->_buffer->cur > 0) {
    this->_buffer->cur--;
    return 0;
  }
  return -1;
}

std::string input_stream::StrISgets2(bool crnl) {
  struct growbuf gb;
  growbuf_init(&gb);
  this->ISgets_to_growbuf(&gb, crnl);
  return growbuf_to_Str(&gb)->ptr;
}

void input_stream::ISgets_to_growbuf(struct growbuf *gb, char crnl) {
  gb->length = 0;
  while (!this->_iseos) {
    if (this->_buffer->MUST_BE_UPDATED()) {
      if (this->_buffer->do_update(std::bind(&input_stream::read, this,
                                             std::placeholders::_1,
                                             std::placeholders::_2)) <= 0) {
        this->_iseos = true;
      }
      continue;
    }
    if (crnl && gb->length > 0 && gb->ptr[gb->length - 1] == '\r') {
      if (this->_buffer->buf[this->_buffer->cur] == '\n') {
        GROWBUF_ADD_CHAR(gb, '\n');
        ++this->_buffer->cur;
      }
      break;
    }
    int i;
    for (i = this->_buffer->cur; i < this->_buffer->next; ++i) {
      if (this->_buffer->buf[i] == '\n' ||
          (crnl && this->_buffer->buf[i] == '\r')) {
        ++i;
        break;
      }
    }
    growbuf_append(gb, &this->_buffer->buf[this->_buffer->cur],
                   i - this->_buffer->cur);
    this->_buffer->cur = i;
    if (gb->length > 0 && gb->ptr[gb->length - 1] == '\n')
      break;
  }
  growbuf_reserve(gb, gb->length + 1);
  gb->ptr[gb->length] = '\0';
  return;
}

static int buffer_read(stream_buffer *sb, char *obuf, int count) {
  int len = sb->next - sb->cur;
  if (len > 0) {
    if (len > count)
      len = count;
    bcopy((const void *)&sb->buf[sb->cur], obuf, len);
    sb->cur += len;
  }
  return len;
}

int input_stream::ISread_n(char *dst, int count) {
  if (count <= 0) {
    return -1;
  }

  if (this->_iseos) {
    return 0;
  }

  auto len = buffer_read(this->_buffer, dst, count);
  if (this->_buffer->MUST_BE_UPDATED()) {
    auto l = this->read((unsigned char *)&dst[len], count - len);
    if (l <= 0) {
      this->_iseos = true;
    } else {
      len += l;
    }
  }
  return len;
}
