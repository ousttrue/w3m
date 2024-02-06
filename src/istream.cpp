#include "istream.h"
#include "Str.h"
#include "linein.h"
#include "ssl_util.h"
#include "signal_util.h"
#include "proto.h"
#include "alloc.h"
#include <fcntl.h>
#include <unistd.h>

#define STREAM_BUF_SIZE 8192

#define MUST_BE_UPDATED(bs) ((bs)->stream.cur == (bs)->stream.next)

#define POP_CHAR(bs) ((bs)->iseos ? '\0' : (bs)->stream.buf[(bs)->stream.cur++])

static void basic_close(int *handle);
static int basic_read(int *handle, char *buf, int len);

static void file_close(struct io_file_handle *handle);
static int file_read(struct io_file_handle *handle, char *buf, int len);

static int str_read(Str *handle, char *buf, int len);

static int ens_read(struct ens_handle *handle, char *buf, int len);
static void ens_close(struct ens_handle *handle);

static void memchop(char *p, int *len);

static void do_update(base_stream *base) {
  int len;
  base->stream.cur = base->stream.next = 0;
  len = (*base->read)(base->handle, base->stream.buf, base->stream.size);
  if (len <= 0)
    base->iseos = true;
  else
    base->stream.next += len;
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

static void init_buffer(base_stream *base, char *buf, int bufsize) {
  stream_buffer *sb = &base->stream;
  sb->size = bufsize;
  sb->cur = 0;
  sb->buf = (unsigned char *)NewWithoutGC_N(unsigned char, bufsize);
  if (buf) {
    memcpy(sb->buf, buf, bufsize);
    sb->next = bufsize;
  } else {
    sb->next = 0;
  }
  base->iseos = false;
}

void init_base_stream(base_stream *base, int bufsize) {
  init_buffer(base, NULL, bufsize);
}

static void init_str_stream(base_stream *base, Str *s) {
  init_buffer(base, s->ptr, s->length);
}

input_stream *newInputStream(int des) {
  input_stream *stream;
  if (des < 0)
    return NULL;
  stream = NewWithoutGC(union input_stream);
  init_base_stream(&stream->base, STREAM_BUF_SIZE);
  stream->base.type = IST_BASIC;
  stream->base.handle = NewWithoutGC(int);
  *(int *)stream->base.handle = des;
  stream->base.read = (ReadFunc)basic_read;
  stream->base.close = (CloseFunc)basic_close;
  return stream;
}

input_stream *newFileStream(FILE *f, void (*closep)()) {
  input_stream *stream;
  if (f == NULL)
    return NULL;
  stream = NewWithoutGC(union input_stream);
  init_base_stream(&stream->base, STREAM_BUF_SIZE);
  stream->file.type = IST_FILE;
  stream->file.handle = NewWithoutGC(struct io_file_handle);
  stream->file.handle->f = f;
  if (closep)
    stream->file.handle->close = (CloseFunc)closep;
  else
    stream->file.handle->close = (CloseFunc)fclose;
  stream->file.read = (ReadFunc)file_read;
  stream->file.close = (CloseFunc)file_close;
  return stream;
}

input_stream *newStrStream(Str *s) {
  input_stream *stream;
  if (s == NULL)
    return NULL;
  stream = NewWithoutGC(union input_stream);
  init_str_stream(&stream->base, s);
  stream->str.type = IST_STR;
  stream->str.handle = NULL;
  stream->str.read = (ReadFunc)str_read;
  stream->str.close = NULL;
  return stream;
}

input_stream *input_stream::newEncodedStream(EncodingType encoding) {
  if ((encoding != ENC_QUOTE && encoding != ENC_BASE64 &&
       encoding != ENC_UUENCODE)) {
    return this;
  }

  auto stream = NewWithoutGC(union input_stream);
  init_base_stream(&stream->base, STREAM_BUF_SIZE);
  stream->ens.type = IST_ENCODED;
  stream->ens.handle = NewWithoutGC(struct ens_handle);
  stream->ens.handle->is = this;
  stream->ens.handle->pos = 0;
  stream->ens.handle->encoding = encoding;
  growbuf_init_without_GC(&stream->ens.handle->gb);
  stream->ens.read = (ReadFunc)ens_read;
  stream->ens.close = (CloseFunc)ens_close;
  return stream;
}

int input_stream::ISclose() {
  MySignalHandler prevtrap = {};
  if (this->base.close != NULL) {
    prevtrap = mySignalInt(mySignalGetIgn());
    this->base.close(this->base.handle);
    mySignalInt(prevtrap);
  }
  xfree(this->base.stream.buf);
  xfree(this);
  return 0;
}

int input_stream::ISgetc() {
  auto base = &this->base;
  if (!base->iseos && MUST_BE_UPDATED(base))
    do_update(base);
  return POP_CHAR(base);
}

int input_stream::ISundogetc() {
  auto sb = &base.stream;
  if (sb->cur > 0) {
    sb->cur--;
    return 0;
  }
  return -1;
}

Str *input_stream::StrISgets2(bool crnl) {
  struct growbuf gb;
  growbuf_init(&gb);
  this->ISgets_to_growbuf(&gb, crnl);
  return growbuf_to_Str(&gb);
}

void input_stream::ISgets_to_growbuf(struct growbuf *gb, char crnl) {
  base_stream *base = &this->base;
  stream_buffer *sb = &base->stream;
  int i;

  gb->length = 0;

  while (!base->iseos) {
    if (MUST_BE_UPDATED(base)) {
      do_update(base);
      continue;
    }
    if (crnl && gb->length > 0 && gb->ptr[gb->length - 1] == '\r') {
      if (sb->buf[sb->cur] == '\n') {
        GROWBUF_ADD_CHAR(gb, '\n');
        ++sb->cur;
      }
      break;
    }
    for (i = sb->cur; i < sb->next; ++i) {
      if (sb->buf[i] == '\n' || (crnl && sb->buf[i] == '\r')) {
        ++i;
        break;
      }
    }
    growbuf_append(gb, &sb->buf[sb->cur], i - sb->cur);
    sb->cur = i;
    if (gb->length > 0 && gb->ptr[gb->length - 1] == '\n')
      break;
  }

  growbuf_reserve(gb, gb->length + 1);
  gb->ptr[gb->length] = '\0';
  return;
}

#ifdef unused
int ISread(input_stream *stream, Str *buf, int count) {
  int len;

  if (count + 1 > buf->area_size) {
    char *newptr = GC_MALLOC_ATOMIC(count + 1);
    memcpy(newptr, buf->ptr, buf->length);
    newptr[buf->length] = '\0';
    buf->ptr = newptr;
    buf->area_size = count + 1;
  }
  len = ISread_n(stream, buf->ptr, count);
  buf->length = (len > 0) ? len : 0;
  buf->ptr[buf->length] = '\0';
  return (len > 0) ? 1 : 0;
}
#endif

int input_stream::ISread_n(char *dst, int count) {
  if (count <= 0) {
    return -1;
  }

  base_stream *base;
  if ((base = &this->base)->iseos) {
    return 0;
  }

  auto len = buffer_read(&base->stream, dst, count);
  if (MUST_BE_UPDATED(base)) {
    auto l =
        (*base->read)(base->handle, (unsigned char *)&dst[len], count - len);
    if (l <= 0) {
      base->iseos = true;
    } else {
      len += l;
    }
  }
  return len;
}

int input_stream::ISfileno() const {
  switch (this->IStype()) {
  case IST_BASIC:
    return *(int *)this->base.handle;
  case IST_FILE:
    return fileno(this->file.handle->f);
  case IST_SSL:
    return this->ssl.handle->sock;
  case IST_ENCODED:
    return this->ens.handle->is->ISfileno();
  default:
    return -1;
  }
}

int ISeos(input_stream *stream) {
  base_stream *base = &stream->base;
  if (!base->iseos && MUST_BE_UPDATED(base))
    do_update(base);
  return base->iseos;
}

/* Raw level input stream functions */

static void basic_close(int *handle) {
  close(*(int *)handle);
  xfree(handle);
}

static int basic_read(int *handle, char *buf, int len) {
  return read(*(int *)handle, buf, len);
}

static void file_close(struct io_file_handle *handle) {
  handle->close((void *)handle->f);
  xfree(handle);
}

static int file_read(struct io_file_handle *handle, char *buf, int len) {
  return fread(buf, 1, len, handle->f);
}

static int str_read(Str *handle, char *buf, int len) { return 0; }

static void ens_close(struct ens_handle *handle) {
  handle->is->ISclose();
  growbuf_clear(&handle->gb);
  xfree(handle);
}

static int ens_read(struct ens_handle *handle, char *buf, int len) {
  if (handle->pos == handle->gb.length) {
    handle->is->ISgets_to_growbuf(&handle->gb, true);
    if (handle->gb.length == 0) {
      return 0;
    }
    if (handle->encoding == ENC_BASE64) {
      memchop(handle->gb.ptr, &handle->gb.length);
    } else if (handle->encoding == ENC_UUENCODE) {
      if (handle->gb.length >= 5 && !strncmp(handle->gb.ptr, "begin", 5)) {
        handle->is->ISgets_to_growbuf(&handle->gb, true);
      }
      memchop(handle->gb.ptr, &handle->gb.length);
    }

    struct growbuf gbtmp;
    growbuf_init_without_GC(&gbtmp);
    auto p = handle->gb.ptr;
    if (handle->encoding == ENC_QUOTE)
      decodeQP_to_growbuf(&gbtmp, &p);
    else if (handle->encoding == ENC_BASE64)
      decodeB_to_growbuf(&gbtmp, &p);
    else if (handle->encoding == ENC_UUENCODE)
      decodeU_to_growbuf(&gbtmp, &p);
    growbuf_clear(&handle->gb);
    handle->gb = gbtmp;
    handle->pos = 0;
  }

  if (len > handle->gb.length - handle->pos)
    len = handle->gb.length - handle->pos;

  memcpy(buf, &handle->gb.ptr[handle->pos], len);
  handle->pos += len;
  return len;
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

input_stream *openIS(const char *path) {
  return newInputStream(open(path, O_RDONLY));
};
