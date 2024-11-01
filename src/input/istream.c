#include "input/istream.h"
#include "alloc.h"
#include "buffer/buffer.h"
#include "buffer/document.h"
#include "file/shell.h"
#include "input/encoding_type.h"
#include "input/ftp.h"
#include "input/growbuf.h"
#include "input/https.h"
#include "input/loader.h"
#include "input/localcgi.h"
#include "input/proxy.h"
#include "input/stream_buffer.h"
#include "rc.h"
#include "siteconf.h"
#include "text/Str.h"
#include "text/regex.h"
#include "text/textlist.h"
#include "trap_jmp.h"
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

struct base_stream {
  struct stream_buffer stream;
  // file descriptor read/write
  int *handle;
  enum IST_TYPE type;
  // ftp ?
  bool unclose;
  bool iseos;
  ReadFunc read;
  CloseFunc close;
};

struct file_stream {
  struct stream_buffer stream;
  // fread/fwrite
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

struct encoded_stream {
  struct stream_buffer stream;
  struct ens_handle *handle;
  enum IST_TYPE type;
  bool unclose;
  bool iseos;
  ReadFunc read;
  CloseFunc close;
};

#ifdef _WIN32
struct winsock_stream {
  struct stream_buffer stream;
  uintptr_t *handle;
  enum IST_TYPE type;
  // ftp ?
  bool unclose;
  bool iseos;
  ReadFunc read;
  CloseFunc close;
};
#endif

union input_stream {
  struct base_stream base;
  struct file_stream file;
  struct str_stream str;
  struct ssl_stream ssl;
  struct encoded_stream ens;
#ifdef _WIN32
  struct winsock_stream ws;
#endif
};

const char *ssl_certificate(union input_stream *stream) {
  return stream ? stream->ssl.ssl_certificate : nullptr;
}
void ssl_set_certificate(union input_stream *stream,
                         const char *ssl_certificate) {
  if (ssl_certificate && IStype(stream) == IST_SSL) {
    stream->ssl.ssl_certificate = ssl_certificate;
  }
}

struct io_file_handle {
  FILE *f;
  CloseFunc close;
};

struct ens_handle {
  union input_stream *is;
  struct growbuf gb;
  int pos;
  char encoding;
};

Str StrISgets(union input_stream *stream) { return StrISgets2(stream, false); }
Str StrmyISgets(union input_stream *stream) { return StrISgets2(stream, true); }

#define uchar unsigned char

#define SSL_BUF_SIZE 1536

#define MUST_BE_UPDATED(bs) ((bs)->stream.cur == (bs)->stream.next)

#define POP_CHAR(bs) ((bs)->iseos ? '\0' : (bs)->stream.buf[(bs)->stream.cur++])

/* Raw level input stream functions */

static void basic_close(void *handle) {
  close(*(int *)handle);
  xfree(handle);
}

static int basic_read(void *handle, unsigned char *buf, int len) {
  return read(*(int *)handle, buf, len);
}

static void file_close(void *_handle) {
  auto handle = (struct io_file_handle *)_handle;
  handle->close(handle->f);
  xfree(handle);
}

static int file_read(void *handle, unsigned char *buf, int len) {
  return fread(buf, 1, len, ((struct io_file_handle *)handle)->f);
}

static int str_read(void *handle, unsigned char *buf, int len) { return 0; }

static void ens_close(void *_handle) {
  auto handle = (struct ens_handle *)_handle;
  ISclose(handle->is);
  growbuf_clear(&handle->gb);
  xfree(handle);
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

static void do_update(struct base_stream *base) {
  int len;
  base->stream.cur = base->stream.next = 0;
  len = (*base->read)(base->handle, base->stream.buf, base->stream.size);
  if (len <= 0)
    base->iseos = true;
  else
    base->stream.next += len;
}

static void ISgets_to_growbuf(union input_stream *stream, struct growbuf *gb,
                              char crnl) {
  struct base_stream *base = &stream->base;
  struct stream_buffer *sb = &base->stream;
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
static int ens_read(void *_handle, unsigned char *buf, int len) {
  auto handle = (struct ens_handle *)_handle;
  if (handle->pos == handle->gb.length) {
    ISgets_to_growbuf(handle->is, &handle->gb, true);
    if (handle->gb.length == 0)
      return 0;

    if (handle->encoding == ENC_BASE64)
      memchop(handle->gb.ptr, &handle->gb.length);
    else if (handle->encoding == ENC_UUENCODE) {
      if (handle->gb.length >= 5 && !strncmp(handle->gb.ptr, "begin", 5))
        ISgets_to_growbuf(handle->is, &handle->gb, true);
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

#ifdef _WIN32
static void ws_close(void *handle) {
  closesocket(*(SOCKET *)handle);
  xfree(handle);
}

static int ws_read(void *handle, unsigned char *buf, int len) {
  return recv(*(SOCKET *)handle, (char *)buf, len, 0);
}
#endif

static void init_buffer(struct base_stream *base, char *buf, int bufsize) {
  struct stream_buffer *sb = &base->stream;
  sb->size = bufsize;
  sb->cur = 0;
  sb->buf = NewWithoutGC_N(uchar, bufsize);
  if (buf) {
    memcpy(sb->buf, buf, bufsize);
    sb->next = bufsize;
  } else {
    sb->next = 0;
  }
  base->iseos = false;
  base->unclose = false;
}

static void init_base_stream(struct base_stream *base, int bufsize) {
  init_buffer(base, NULL, bufsize);
}

static void init_str_stream(struct base_stream *base, Str s) {
  init_buffer(base, s->ptr, s->length);
}

union input_stream *newInputStream(int des) {
  union input_stream *stream;
  if (des < 0)
    return NULL;
  stream = NewWithoutGC(union input_stream);
  init_base_stream(&stream->base, STREAM_BUF_SIZE);
  stream->base.type = IST_BASIC;
  stream->base.handle = NewWithoutGC(int);
  *(int *)stream->base.handle = des;
  stream->base.read = basic_read;
  stream->base.close = basic_close;
  return stream;
}

union input_stream *newFileStream(FILE *f, int (*closep)(FILE *)) {
  union input_stream *stream;
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
  stream->file.read = file_read;
  stream->file.close = file_close;
  return stream;
}

union input_stream *newStrStream(Str s) {
  union input_stream *stream;
  if (s == NULL)
    return NULL;
  stream = NewWithoutGC(union input_stream);
  init_str_stream(&stream->base, s);
  stream->str.type = IST_STR;
  stream->str.handle = NULL;
  stream->str.read = str_read;
  stream->str.close = NULL;
  return stream;
}

union input_stream *newSSLStream(SSL *ssl, int sock) {
  union input_stream *stream;
  if (sock < 0)
    return NULL;
  stream = NewWithoutGC(union input_stream);
  init_base_stream(&stream->base, SSL_BUF_SIZE);
  stream->ssl.type = IST_SSL;
  stream->ssl.handle = NewWithoutGC(struct ssl_handle);
  stream->ssl.handle->ssl = ssl;
  stream->ssl.handle->sock = sock;
  stream->ssl.read = ssl_read;
  stream->ssl.close = ssl_close;
  return stream;
}

union input_stream *newEncodedStream(union input_stream *is,
                                     enum ENCODING_TYPE encoding) {
  if (is == NULL || (encoding != ENC_QUOTE && encoding != ENC_BASE64 &&
                     encoding != ENC_UUENCODE))
    return is;
  union input_stream *stream = NewWithoutGC(union input_stream);
  init_base_stream(&stream->base, STREAM_BUF_SIZE);
  stream->ens.type = IST_ENCODED;
  stream->ens.handle = NewWithoutGC(struct ens_handle);
  stream->ens.handle->is = is;
  stream->ens.handle->pos = 0;
  stream->ens.handle->encoding = encoding;
  growbuf_init_without_GC(&stream->ens.handle->gb);
  stream->ens.read = ens_read;
  stream->ens.close = ens_close;
  return stream;
}

#ifdef _WIN32
union input_stream *newWinsockStream(SOCKET sock) {
  if (sock == INVALID_SOCKET)
    return NULL;
  union input_stream *stream = NewWithoutGC(union input_stream);
  init_base_stream(&stream->base, STREAM_BUF_SIZE);
  stream->ws.type = IST_WS;
  stream->ws.handle = NewWithoutGC(SOCKET);
  *(SOCKET *)stream->ws.handle = sock;
  stream->ws.read = ws_read;
  stream->ws.close = ws_close;
  return stream;
}
#endif

int ISclose(union input_stream *stream) {
  if (stream == NULL)
    return -1;
  if (stream->base.close != NULL) {
    if (stream->base.unclose) {
      return -1;
    }
    stream->base.close(stream->base.handle);
    // auto prevtrap = mySignal(SIGINT, SIG_IGN);
    // mySignal(SIGINT, prevtrap);
  }
  xfree(stream->base.stream.buf);
  xfree(stream);
  return 0;
}

int ISgetc(union input_stream *stream) {
  struct base_stream *base;
  if (stream == NULL)
    return '\0';
  base = &stream->base;
  if (!base->iseos && MUST_BE_UPDATED(base))
    do_update(base);
  return POP_CHAR(base);
}

int ISundogetc(union input_stream *stream) {
  struct stream_buffer *sb;
  if (stream == NULL)
    return -1;
  sb = &stream->base.stream;
  if (sb->cur > 0) {
    sb->cur--;
    return 0;
  }
  return -1;
}

Str StrISgets2(union input_stream *stream, char crnl) {
  struct growbuf gb;

  if (stream == NULL)
    return NULL;
  growbuf_init(&gb);
  ISgets_to_growbuf(stream, &gb, crnl);
  return growbuf_to_Str(&gb);
}

int ISread_n(union input_stream *stream, char *dst, int count) {
  if (stream == NULL || count <= 0)
    return -1;

  struct base_stream *base;
  if ((base = &stream->base)->iseos)
    return 0;

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

int ISfileno(union input_stream *stream) {
  if (stream == NULL)
    return -1;
  switch (IStype(stream)) {
  case IST_BASIC:
    return *(int *)stream->base.handle;
  case IST_FILE:
    return fileno(stream->file.handle->f);
  case IST_SSL:
    return stream->ssl.handle->sock;
  case IST_ENCODED:
    return ISfileno(stream->ens.handle->is);
  default:
    return -1;
  }
}

int ISeos(union input_stream *stream) {
  struct base_stream *base = &stream->base;
  if (!base->iseos && MUST_BE_UPDATED(base))
    do_update(base);
  return base->iseos;
}

enum IST_TYPE IStype(union input_stream *stream) { return stream->base.type; }

int ssl_socket_of(union input_stream *stream) {
  return stream->ssl.handle->sock;
}

union input_stream *openIS(const char *path) {
  return newInputStream(open(path, O_RDONLY));
}

struct TextList *NO_proxy_domains = nullptr;
void url_stream_init() { NO_proxy_domains = newTextList(); }

#ifdef SOCK_DEBUG
#include <stdarg.h>

static void sock_log(char *message, ...) {
  FILE *f = fopen("zzzsocklog", "a");
  va_list va;

  if (f == NULL)
    return;
  va_start(va, message);
  vfprintf(f, message, va);
  fclose(f);
}

#endif

#define ALLOC_STR(s) ((s) == NULL ? NULL : allocStr(s, -1))

void copyParsedURL(struct Url *p, const struct Url *q) {
  if (q == NULL) {
    memset(p, 0, sizeof(struct Url));
    p->scheme = SCM_UNKNOWN;
    return;
  }
  p->scheme = q->scheme;
  p->port = q->port;
  p->user = ALLOC_STR(q->user);
  p->pass = ALLOC_STR(q->pass);
  p->host = ALLOC_STR(q->host);
  p->file = ALLOC_STR(q->file);
  p->real_file = ALLOC_STR(q->real_file);
  p->label = ALLOC_STR(q->label);
  p->query = ALLOC_STR(q->query);
}

Str parsedURL2Str(struct Url *pu) {
  return _parsedURL2Str(pu, false, true, true);
}

Str parsedURL2RefererStr(struct Url *pu) {
  return _parsedURL2Str(pu, false, false, false);
}

/* add index_file if exists */
struct Url *schemeToProxy(enum URL_SCHEME_TYPE scheme) {
  struct Url *pu = NULL; /* for gcc */
  switch (scheme) {
  case SCM_HTTP:
    pu = &HTTP_proxy_parsed;
    break;
  case SCM_HTTPS:
    pu = &HTTPS_proxy_parsed;
    break;
  case SCM_FTP:
    pu = &FTP_proxy_parsed;
    break;
  default:
    abort();
  }
  return pu;
}

void UFhalfclose(struct URLFile *f) {
  // switch (f->scheme) {
  // case SCM_FTP:
  //   closeFTP();
  //   break;
  // default:
  //   ISclose(f->stream);
  //   break;
  // }
}

static FILE *lessopen_stream(const char *path) {

  auto lessopen = getenv("LESSOPEN");
  if (lessopen == NULL) {
    return NULL;
  }
  if (lessopen[0] == '\0') {
    return NULL;
  }

  if (lessopen[0] != '|') {
    /* filename mode */
    /* not supported m(__)m */
    return nullptr;
  }

  /* pipe mode */
  ++lessopen;
  auto tmpf = Sprintf(lessopen, shell_quote(path));
  auto fp = popen(tmpf->ptr, "r");
  if (fp == NULL) {
    return NULL;
  }
  auto c = getc(fp);
  if (c == EOF) {
    pclose(fp);
    return NULL;
  }
  ungetc(c, fp);
  return fp;
}

union input_stream *examineFile(const char *path) {
  // uf->guess_type = NULL;
  struct stat stbuf;
  if (path == NULL || *path == '\0' || stat(path, &stbuf) == -1 ||
      NOT_REGULAR(stbuf.st_mode)) {
    return nullptr;
  }

  auto stream = openIS(path);

  // if (use_lessopen && getenv("LESSOPEN") != NULL) {
  //   uf->guess_type = guessContentType(path);
  //   if (uf->guess_type == NULL)
  //     uf->guess_type = "text/plain";
  //   if (is_html_type(uf->guess_type))
  //     return;
  //
  //   FILE *fp;
  //   if ((fp = lessopen_stream(path))) {
  //     UFclose(uf);
  //     uf->stream = newFileStream(fp, (void (*)())pclose);
  //     uf->guess_type = "text/plain";
  //     return;
  //   }
  // }
  // uf->compression = check_compression(path, &uf->guess_type);
  // if (uf->compression != CMP_NOCOMPRESS) {
  //   const char *ext = uf->ext;
  //   auto t0 = uncompressed_file_type(path, &ext);
  //   uf->guess_type = t0;
  //   uf->ext = ext;
  //   uncompress_stream(uf, NULL);
  //   return;
  // }

  return stream;
}

void close_for_ftp(union input_stream *is) {
  is->base.unclose = false;
  ISclose(is);
}

union input_stream *newInputFtp(int sock) {
  auto rf = newInputStream(sock);
  rf->base.unclose = true;
  return rf;
}

#define SAVE_BUF_SIZE 1536

int save2tmp(union input_stream *stream, const char *tmpf) {
  auto ff = fopen(tmpf, "wb");
  if (ff == NULL) {
    /* fclose(f); */
    return -1;
  }

  int retval = 0;
  char *buf = nullptr;
  if (from_jmp()) {
    goto _end;
  }
  trap_on();

  {
    int count;

    buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
    while ((count = ISread_n(stream, buf, SAVE_BUF_SIZE)) > 0) {
      if (fwrite(buf, 1, count, ff) != count) {
        retval = -2;
        goto _end;
      }
      // linelen += count;
      // term_showProgress(&linelen, &trbyte, uf.current_content_length);
    }
  }

_end:
  trap_off();
  xfree(buf);
  fclose(ff);
  return retval;
}

Str StrISreadAll(union input_stream *stream) {
  Str content = Strnew();
  while (true) {
    auto line = StrmyISgets(stream);
    if (line->length == 0) {
      break;
    }
    Strcat(content, line);
  }
  return content;
}
