#include "istream.h"
#include "Str.h"
#include "linein.h"
#include "mimehead.h"
#include "ssl_util.h"
#include "signal_util.h"
// #include "proto.h"
#include "alloc.h"
#include <stdio.h>
#include <fcntl.h>
// #include <unistd.h>

#define STREAM_BUF_SIZE 8192

stream_buffer::stream_buffer(int bufsize, const unsigned char *data)
    : size(bufsize) {
  this->buf = (unsigned char *)NewWithoutGC_N(unsigned char, bufsize);
  if (data) {
    memcpy(this->buf, data, bufsize);
    this->next = bufsize;
  } else {
    this->next = 0;
  }
}

//
// constructor
//
struct base_stream {
  int _des;
  base_stream(int des) : _des(des) {}
  void close() {
    if (_des >= 0) {
      ::close(_des);
      _des = -1;
    }
  }
  int read(unsigned char *buf, int len) { return ::read(_des, buf, len); }
};

std::shared_ptr<input_stream> newInputStream(int des) {
  if (des < 0) {
    return NULL;
  }
  auto p = new base_stream(des);
  return std::make_shared<input_stream>(IST_BASIC, p, STREAM_BUF_SIZE);
}

struct file_stream {
  FILE *_file;
  FileClose _close;
  file_stream(FILE *file, FileClose close) : _file(file), _close(close) {}
  void close() {
    if (_file) {
      this->_close(_file);
      _file = {};
    }
  }
  int read(unsigned char *buf, int len) { return fread(buf, 1, len, _file); }
};

std::shared_ptr<input_stream> newFileStream(FILE *f, FileClose closep) {
  if (f == NULL) {
    return NULL;
  }
  auto p = new file_stream(f, closep ? closep : fclose);
  return std::make_shared<input_stream>(IST_FILE, p, STREAM_BUF_SIZE);
}

std::shared_ptr<input_stream> openIS(const char *path) {
  return newInputStream(open(path, O_RDONLY));
};

struct str_stream {
  void close() {}
  int read(unsigned char *buf, int len) { return 0; }
};

std::shared_ptr<input_stream> newStrStream(std::string_view s) {
  if (s.empty()) {
    return {};
  }
  auto p = new str_stream();
  return std::make_shared<input_stream>(IST_STR, p, s.size(),
                                        (const unsigned char *)s.data());
}

struct ssl_stream {
  SSL *_ssl;
  int _sock;
  ssl_stream(SSL *ssl, int sock) : _ssl(ssl), _sock(sock) {}
  void close() {
    if (this->_sock) {
      ::close(this->_sock);
      this->_sock = 0;
    }
    if (this->_ssl) {
      SSL_free(this->_ssl);
      this->_ssl = {};
    }
  }
  int read(unsigned char *buf, int len) {
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
    return NULL;
  }
  auto p = new ssl_stream(ssl, sock);
  return std::make_shared<input_stream>(IST_SSL, p, SSL_BUF_SIZE);
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

struct encoded_stream {
  std::shared_ptr<input_stream> is;
  struct growbuf gb;
  int pos = 0;
  EncodingType encoding;

  encoded_stream(const std::shared_ptr<input_stream> &is, EncodingType encoding)
      : is(is), encoding(encoding) {
    growbuf_init_without_GC(&this->gb);
  }
  void close() {
    this->is->ISclose();
    growbuf_clear(&this->gb);
  }
  int read(unsigned char *buf, int len) {
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
  auto p = new encoded_stream(is, encoding);
  return std::make_shared<input_stream>(IST_ENCODED, p, STREAM_BUF_SIZE);
}

void input_stream::do_update() {
  this->_buffer.cur = this->_buffer.next = 0;
  int len = this->read(this->_buffer.buf, this->_buffer.size);
  if (len <= 0) {
    this->_iseos = true;
  } else {
    this->_buffer.next += len;
  }
}

int input_stream::read(unsigned char *buf, int size) {
  switch (this->IStype()) {
  case IST_BASIC:
    return ((base_stream *)this->_stream)->read(buf, size);
  case IST_FILE:
    return ((file_stream *)this->_stream)->read(buf, size);
  case IST_SSL:
    return ((ssl_stream *)this->_stream)->read(buf, size);
  case IST_ENCODED:
    return ((encoded_stream *)this->_stream)->read(buf, size);
  default:
    return 0;
  }
}

int input_stream::ISclose() {
  auto prevtrap = mySignalInt(mySignalGetIgn());
  switch (this->IStype()) {
  case IST_BASIC:
    ((base_stream *)this->_stream)->close();
    break;
  case IST_FILE:
    ((file_stream *)this->_stream)->close();
    break;
  case IST_SSL:
    ((ssl_stream *)this->_stream)->close();
    break;
  case IST_ENCODED:
    ((encoded_stream *)this->_stream)->close();
    break;
  default:
    break;
  }
  mySignalInt(prevtrap);
  return 0;
}

bool input_stream::MUST_BE_UPDATED() const {
  return this->_buffer.cur == this->_buffer.next;
}

char input_stream::POP_CHAR() {
  if (this->_iseos) {
    return '\0';
  }
  return this->_buffer.buf[this->_buffer.cur++];
}

int input_stream::ISgetc() {
  if (!this->_iseos && this->MUST_BE_UPDATED()) {
    this->do_update();
  }
  return this->POP_CHAR();
}

int input_stream::ISundogetc() {
  if (this->_buffer.cur > 0) {
    this->_buffer.cur--;
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
    if (this->MUST_BE_UPDATED()) {
      this->do_update();
      continue;
    }
    if (crnl && gb->length > 0 && gb->ptr[gb->length - 1] == '\r') {
      if (this->_buffer.buf[this->_buffer.cur] == '\n') {
        GROWBUF_ADD_CHAR(gb, '\n');
        ++this->_buffer.cur;
      }
      break;
    }
    int i;
    for (i = this->_buffer.cur; i < this->_buffer.next; ++i) {
      if (this->_buffer.buf[i] == '\n' ||
          (crnl && this->_buffer.buf[i] == '\r')) {
        ++i;
        break;
      }
    }
    growbuf_append(gb, &this->_buffer.buf[this->_buffer.cur],
                   i - this->_buffer.cur);
    this->_buffer.cur = i;
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

  auto len = buffer_read(&this->_buffer, dst, count);
  if (this->MUST_BE_UPDATED()) {
    auto l = this->read((unsigned char *)&dst[len], count - len);
    if (l <= 0) {
      this->_iseos = true;
    } else {
      len += l;
    }
  }
  return len;
}

int input_stream::ISfileno() const {
  switch (this->IStype()) {
  case IST_BASIC:
    return ((base_stream *)this->_stream)->_des;
  case IST_FILE:
    return fileno(((file_stream *)this->_stream)->_file);
  case IST_SSL:
    return ((ssl_stream *)this->_stream)->_sock;
  case IST_ENCODED:
    return ((encoded_stream *)this->_stream)->is->ISfileno();
  default:
    return -1;
  }
}
