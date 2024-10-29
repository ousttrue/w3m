#include "input/istream.h"
#include "alloc.h"
#include "buffer/buffer.h"
#include "buffer/document.h"
#include "buffer/message.h"
#include "file/file.h"
#include "file/shell.h"
#include "fm.h"
#include "input/encoding_type.h"
#include "input/ftp.h"
#include "input/growbuf.h"
#include "input/http_cookie.h"
#include "input/http_request.h"
#include "input/http_response.h"
#include "input/isocket.h"
#include "input/loader.h"
#include "input/localcgi.h"
#include "input/proxy.h"
#include "input/stream_buffer.h"
#include "rand48.h"
#include "rc.h"
#include "siteconf.h"
#include "term/terms.h"
#include "term/termsize.h"
#include "text/Str.h"
#include "text/myctype.h"
#include "text/regex.h"
#include "text/text.h"
#include "text/textlist.h"
#include "trap_jmp.h"
#include <assert.h>
#include <fcntl.h>
#include <openssl/bio.h>
#include <openssl/crypto.h> /* SSLEAY_VERSION_NUMBER may be here */
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
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

typedef int (*ReadFunc)(void *handle, unsigned char *buf, int size);
typedef void (*CloseFunc)(void *handle);

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

struct ssl_stream {
  struct stream_buffer stream;
  struct ssl_handle *handle;
  enum IST_TYPE type;
  bool unclose;
  bool iseos;
  ReadFunc read;
  CloseFunc close;
  const char *ssl_certificate;
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

struct ssl_handle {
  SSL *ssl;
  int sock;
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

static void ssl_close(void *_handle) {
  auto handle = (struct ssl_handle *)_handle;
  close(handle->sock);
  if (handle->ssl)
    SSL_free(handle->ssl);
  xfree(handle);
}

static int ssl_read(void *_handle, unsigned char *buf, int len) {
  int status;
  auto handle = (struct ssl_handle *)_handle;
  if (handle->ssl) {
    for (;;) {
      status = SSL_read(handle->ssl, buf, len);
      if (status > 0)
        break;
      switch (SSL_get_error(handle->ssl, status)) {
      case SSL_ERROR_WANT_READ:
      case SSL_ERROR_WANT_WRITE: /* reads can trigger write errors; see
                                    SSL_get_error(3) */
        continue;
      default:
        break;
      }
      break;
    }
  } else
    status = read(handle->sock, buf, len);
  return status;
}

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
    char *p;
    struct growbuf gbtmp;

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
    growbuf_init_without_GC(&gbtmp);
    p = handle->gb.ptr;
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

static Str accept_this_site;

void ssl_accept_this_site(const char *hostname) {
  if (hostname)
    accept_this_site = Strnew_charp(hostname);
  else
    accept_this_site = NULL;
}

static int ssl_match_cert_ident(char *ident, int ilen, char *hostname) {
  /* RFC2818 3.1.  Server Identity
   * Names may contain the wildcard
   * character * which is considered to match any single domain name
   * component or component fragment. E.g., *.a.com matches foo.a.com but
   * not bar.foo.a.com. f*.com matches foo.com but not bar.com.
   */
  int hlen = strlen(hostname);
  int i, c;

  /* Is this an exact match? */
  if ((ilen == hlen) && strncasecmp(ident, hostname, hlen) == 0)
    return true;

  for (i = 0; i < ilen; i++) {
    if (ident[i] == '*' && ident[i + 1] == '.') {
      while ((c = *hostname++) != '\0')
        if (c == '.')
          break;
      i++;
    } else {
      if (ident[i] != *hostname++)
        return false;
    }
  }
  return *hostname == '\0';
}

static Str ssl_check_cert_ident(X509 *x, char *hostname) {
  int i;
  Str ret = NULL;
  int match_ident = false;
  /*
   * All we need to do here is check that the CN matches.
   *
   * From RFC2818 3.1 Server Identity:
   * If a subjectAltName extension of type dNSName is present, that MUST
   * be used as the identity. Otherwise, the (most specific) Common Name
   * field in the Subject field of the certificate MUST be used. Although
   * the use of the Common Name is existing practice, it is deprecated and
   * Certification Authorities are encouraged to use the dNSName instead.
   */
  i = X509_get_ext_by_NID(x, NID_subject_alt_name, -1);
  if (i >= 0) {
    X509_EXTENSION *ex;
    STACK_OF(GENERAL_NAME) * alt;

    ex = X509_get_ext(x, i);
    alt = X509V3_EXT_d2i(ex);
    if (alt) {
      int n;
      GENERAL_NAME *gn;
      Str seen_dnsname = NULL;

      n = sk_GENERAL_NAME_num(alt);
      for (i = 0; i < n; i++) {
        gn = sk_GENERAL_NAME_value(alt, i);
        if (gn->type == GEN_DNS) {
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
          unsigned char *sn = ASN1_STRING_data(gn->d.ia5);
#else
          const unsigned char *sn = ASN1_STRING_get0_data(gn->d.ia5);
#endif
          int sl = ASN1_STRING_length(gn->d.ia5);

          /*
           * sn is a pointer to internal data and not guaranteed to
           * be null terminated. Ensure we have a null terminated
           * string that we can modify.
           */
          char *asn = _GC_MALLOC(sl + 1);
          if (!asn)
            exit(1);
          memcpy(asn, sn, sl);
          asn[sl] = '\0';

          if (!seen_dnsname)
            seen_dnsname = Strnew();
          /* replace \0 to make full string visible to user */
          if (sl != strlen(asn)) {
            int i;
            for (i = 0; i < sl; ++i) {
              if (!asn[i])
                asn[i] = '!';
            }
          }
          Strcat_m_charp(seen_dnsname, asn, " ", NULL);
          if (sl == strlen(asn) /* catch \0 in SAN */
              && ssl_match_cert_ident(asn, sl, hostname))
            break;
        }
      }

      auto method = X509V3_EXT_get(ex);
      sk_GENERAL_NAME_free(alt);
      if (i < n) /* Found a match */
        match_ident = true;
      else if (seen_dnsname)
        /* FIXME: gettextize? */
        ret = Sprintf("Bad cert ident from %s: dNSName=%s", hostname,
                      seen_dnsname->ptr);
    }
  }

  if (match_ident == false && ret == NULL) {
    X509_NAME *xn;
    char buf[2048];
    int slen;

    xn = X509_get_subject_name(x);

    slen = X509_NAME_get_text_by_NID(xn, NID_commonName, buf, sizeof(buf));
    if (slen == -1)
      /* FIXME: gettextize? */
      ret = Strnew_charp("Unable to get common name from peer cert");
    else if (slen != strlen(buf) ||
             !ssl_match_cert_ident(buf, strlen(buf), hostname)) {
      /* replace \0 to make full string visible to user */
      if (slen != strlen(buf)) {
        int i;
        for (i = 0; i < slen; ++i) {
          if (!buf[i])
            buf[i] = '!';
        }
      }
      /* FIXME: gettextize? */
      ret = Sprintf("Bad cert ident %s from %s", buf, hostname);
    }
  }
  return ret;
}

Str ssl_get_certificate(SSL *ssl, char *hostname) {
  BIO *bp;
  X509 *x;
  X509_NAME *xn;
  char *p;
  int len;
  Str s;
  char buf[2048];
  Str amsg = NULL;
  Str emsg;

  if (ssl == NULL)
    return NULL;
  x = SSL_get_peer_certificate(ssl);
  if (x == NULL) {
    const char *ans;
    if (accept_this_site && strcasecmp(accept_this_site->ptr, hostname) == 0)
      ans = "y";
    else {
      /* FIXME: gettextize? */
      emsg = Strnew_charp("No SSL peer certificate: accept? (y/n)");
      ans = term_inputAnswer(emsg->ptr);
    }
    if (ans && TOLOWER(*ans) == 'y')
      /* FIXME: gettextize? */
      amsg = Strnew_charp("Accept SSL session without any peer certificate");
    else {
      /* FIXME: gettextize? */
      char *e = "This SSL session was rejected "
                "to prevent security violation: no peer certificate";
      message_push(e);
      free_ssl_ctx();
      return NULL;
    }
    if (amsg)
      message_push(amsg->ptr);
    ssl_accept_this_site(hostname);
    /* FIXME: gettextize? */
    s = amsg ? amsg : Strnew_charp("valid certificate");
    return s;
  }
  /* check the cert chain.
   * The chain length is automatically checked by OpenSSL when we
   * set the verify depth in the ctx.
   */
  if (ssl_verify_server) {
    long verr;
    if ((verr = SSL_get_verify_result(ssl)) != X509_V_OK) {
      const char *em = X509_verify_cert_error_string(verr);
      const char *ans;
      if (accept_this_site && strcasecmp(accept_this_site->ptr, hostname) == 0)
        ans = "y";
      else {
        /* FIXME: gettextize? */
        emsg = Sprintf("%s: accept? (y/n)", em);
        ans = term_inputAnswer(emsg->ptr);
      }
      if (ans && TOLOWER(*ans) == 'y') {
        /* FIXME: gettextize? */
        amsg = Sprintf("Accept unsecure SSL session: "
                       "unverified: %s",
                       em);
      } else {
        /* FIXME: gettextize? */
        char *e = Sprintf("This SSL session was rejected: %s", em)->ptr;
        message_push(e);
        free_ssl_ctx();
        return NULL;
      }
    }
  }
  emsg = ssl_check_cert_ident(x, hostname);
  if (emsg != NULL) {
    const char *ans;
    if (accept_this_site && strcasecmp(accept_this_site->ptr, hostname) == 0)
      ans = "y";
    else {
      Str ep = Strdup(emsg);
      if (ep->length > COLS - 16)
        Strshrink(ep, ep->length - (COLS - 16));
      Strcat_charp(ep, ": accept? (y/n)");
      ans = term_inputAnswer(ep->ptr);
    }
    if (ans && TOLOWER(*ans) == 'y') {
      /* FIXME: gettextize? */
      amsg = Strnew_charp("Accept unsecure SSL session:");
      Strcat(amsg, emsg);
    } else {
      /* FIXME: gettextize? */
      char *e = "This SSL session was rejected "
                "to prevent security violation";
      message_push(e);
      free_ssl_ctx();
      return NULL;
    }
  }
  if (amsg) {
    message_push(amsg->ptr);
  }
  ssl_accept_this_site(hostname);
  /* FIXME: gettextize? */
  s = amsg ? amsg : Strnew_charp("valid certificate");
  Strcat_charp(s, "\n");
  xn = X509_get_subject_name(x);
  if (X509_NAME_get_text_by_NID(xn, NID_commonName, buf, sizeof(buf)) == -1)
    Strcat_charp(s, " subject=<unknown>");
  else
    Strcat_m_charp(s, " subject=", buf, NULL);
  xn = X509_get_issuer_name(x);
  if (X509_NAME_get_text_by_NID(xn, NID_commonName, buf, sizeof(buf)) == -1)
    Strcat_charp(s, ": issuer=<unknown>");
  else
    Strcat_m_charp(s, ": issuer=", buf, NULL);
  Strcat_charp(s, "\n\n");

  bp = BIO_new(BIO_s_mem());
  X509_print(bp, x);
  len = (int)BIO_ctrl(bp, BIO_CTRL_INFO, 0, (char *)&p);
  Strcat_charp_n(s, p, len);
  BIO_free_all(bp);
  X509_free(x);
  return s;
}

enum IST_TYPE IStype(union input_stream *stream) { return stream->base.type; }

int ssl_socket_of(union input_stream *stream) {
  return stream->ssl.handle->sock;
}

union input_stream *openIS(const char *path) {
  return newInputStream(open(path, O_RDONLY));
}

Str header_string = NULL;

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

SSL_CTX *ssl_ctx = NULL;

void free_ssl_ctx() {
  if (ssl_ctx != NULL)
    SSL_CTX_free(ssl_ctx);
  ssl_ctx = NULL;
  ssl_accept_this_site(NULL);
}

#if SSLEAY_VERSION_NUMBER >= 0x00905100
#include <openssl/rand.h>
static void init_PRNG() {
  if (RAND_status())
    return;

  char buffer[256];
  const char *file = nullptr;
  if ((file = RAND_file_name(buffer, sizeof(buffer)))) {
#ifdef USE_EGD
    if (RAND_egd(file) > 0)
      return;
#endif
    RAND_load_file(file, -1);
  }
  if (RAND_status())
    goto seeded;
  srand48((long)time(NULL));
  while (!RAND_status()) {
    long l = lrand48();
    RAND_seed((unsigned char *)&l, sizeof(long));
  }
seeded:
  if (file)
    RAND_write_file(file);
}
#endif /* SSLEAY_VERSION_NUMBER >= 0x00905100 */

#ifdef SSL_CTX_set_min_proto_version
static int str_to_ssl_version(const char *name) {
  if (!strcasecmp(name, "all"))
    return 0;
  if (!strcasecmp(name, "none"))
    return 0;
#ifdef TLS1_3_VERSION
  if (!strcasecmp(name, "TLSv1.3"))
    return TLS1_3_VERSION;
#endif
#ifdef TLS1_2_VERSION
  if (!strcasecmp(name, "TLSv1.2"))
    return TLS1_2_VERSION;
#endif
#ifdef TLS1_1_VERSION
  if (!strcasecmp(name, "TLSv1.1"))
    return TLS1_1_VERSION;
#endif
  if (!strcasecmp(name, "TLSv1.0"))
    return TLS1_VERSION;
  if (!strcasecmp(name, "TLSv1"))
    return TLS1_VERSION;
  if (!strcasecmp(name, "SSLv3.0"))
    return SSL3_VERSION;
  if (!strcasecmp(name, "SSLv3"))
    return SSL3_VERSION;
  return -1;
}
#endif /* SSL_CTX_set_min_proto_version */

static SSL *openSSLHandle(int sock, char *hostname, char **p_cert) {
  SSL *handle = NULL;
  static char *old_ssl_forbid_method = NULL;
  static int old_ssl_verify_server = -1;

  if (old_ssl_forbid_method != ssl_forbid_method &&
      (!old_ssl_forbid_method || !ssl_forbid_method ||
       strcmp(old_ssl_forbid_method, ssl_forbid_method))) {
    old_ssl_forbid_method = ssl_forbid_method;
    ssl_path_modified = 1;
  }
  if (old_ssl_verify_server != ssl_verify_server) {
    old_ssl_verify_server = ssl_verify_server;
    ssl_path_modified = 1;
  }
  if (ssl_path_modified) {
    free_ssl_ctx();
    ssl_path_modified = 0;
  }
  if (ssl_ctx == NULL) {
    int option;
#if OPENSSL_VERSION_NUMBER < 0x0800
    ssl_ctx = SSL_CTX_new();
    X509_set_default_verify_paths(ssl_ctx->cert);
#else /* SSLEAY_VERSION_NUMBER >= 0x0800 */
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
    SSLeay_add_ssl_algorithms();
    SSL_load_error_strings();
#else
    OPENSSL_init_ssl(0, NULL);
#endif
    if (!(ssl_ctx = SSL_CTX_new(SSLv23_client_method())))
      goto eend;
#ifdef SSL_CTX_set_min_proto_version
    if (ssl_min_version && *ssl_min_version != '\0') {
      int sslver;
      sslver = str_to_ssl_version(ssl_min_version);
      if (sslver < 0 || !SSL_CTX_set_min_proto_version(ssl_ctx, sslver)) {
        free_ssl_ctx();
        goto eend;
      }
    }
#endif
    if (ssl_cipher && *ssl_cipher != '\0')
      if (!SSL_CTX_set_cipher_list(ssl_ctx, ssl_cipher)) {
        free_ssl_ctx();
        goto eend;
      }
    option = SSL_OP_ALL;
    if (ssl_forbid_method) {
      if (strchr(ssl_forbid_method, '2'))
        option |= SSL_OP_NO_SSLv2;
      if (strchr(ssl_forbid_method, '3'))
        option |= SSL_OP_NO_SSLv3;
      if (strchr(ssl_forbid_method, 't'))
        option |= SSL_OP_NO_TLSv1;
      if (strchr(ssl_forbid_method, 'T'))
        option |= SSL_OP_NO_TLSv1;
      if (strchr(ssl_forbid_method, '4'))
        option |= SSL_OP_NO_TLSv1;
#ifdef SSL_OP_NO_TLSv1_1
      if (strchr(ssl_forbid_method, '5'))
        option |= SSL_OP_NO_TLSv1_1;
#endif
#ifdef SSL_OP_NO_TLSv1_2
      if (strchr(ssl_forbid_method, '6'))
        option |= SSL_OP_NO_TLSv1_2;
#endif
#ifdef SSL_OP_NO_TLSv1_3
      if (strchr(ssl_forbid_method, '7'))
        option |= SSL_OP_NO_TLSv1_3;
#endif
    }
#ifdef SSL_OP_NO_COMPRESSION
    option |= SSL_OP_NO_COMPRESSION;
#endif
    SSL_CTX_set_options(ssl_ctx, option);

#ifdef SSL_MODE_RELEASE_BUFFERS
    SSL_CTX_set_mode(ssl_ctx, SSL_MODE_RELEASE_BUFFERS);
#endif

    /* derived from openssl-0.9.5/apps/s_{client,cb}.c */
#if 1 /* use SSL_get_verify_result() to verify cert */
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
#else
    SSL_CTX_set_verify(
        ssl_ctx, ssl_verify_server ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, NULL);
#endif
    if (ssl_cert_file != NULL && *ssl_cert_file != '\0') {
      int ng = 1;
      if (SSL_CTX_use_certificate_file(ssl_ctx, ssl_cert_file,
                                       SSL_FILETYPE_PEM) > 0) {
        const char *key_file = (ssl_key_file == NULL || *ssl_key_file == '\0')
                                   ? ssl_cert_file
                                   : ssl_key_file;
        if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file, SSL_FILETYPE_PEM) >
            0)
          if (SSL_CTX_check_private_key(ssl_ctx))
            ng = 0;
      }
      if (ng) {
        free_ssl_ctx();
        goto eend;
      }
    }
    if (ssl_verify_server) {
      const char *file = NULL, *path = NULL;
      if (ssl_ca_file && *ssl_ca_file != '\0')
        file = ssl_ca_file;
      if (ssl_ca_path && *ssl_ca_path != '\0')
        path = ssl_ca_path;
      if ((file || path) &&
          !SSL_CTX_load_verify_locations(ssl_ctx, file, path)) {
        free_ssl_ctx();
        goto eend;
      }
      if (ssl_ca_default)
        SSL_CTX_set_default_verify_paths(ssl_ctx);
    }
#endif /* SSLEAY_VERSION_NUMBER >= 0x0800 */
  }
  handle = SSL_new(ssl_ctx);
  SSL_set_fd(handle, sock);
#if SSLEAY_VERSION_NUMBER >= 0x00905100
  init_PRNG();
#endif /* SSLEAY_VERSION_NUMBER >= 0x00905100 */
#if (SSLEAY_VERSION_NUMBER >= 0x00908070) && !defined(OPENSSL_NO_TLSEXT)
  SSL_set_tlsext_host_name(handle, hostname);
#endif /* (SSLEAY_VERSION_NUMBER >= 0x00908070) && !defined(OPENSSL_NO_TLSEXT) \
        */
  if (SSL_connect(handle) > 0) {
    Str serv_cert = ssl_get_certificate(handle, hostname);
    if (serv_cert) {
      *p_cert = serv_cert->ptr;
      return handle;
    }
    close(sock);
    SSL_free(handle);
    return NULL;
  }
eend:
  close(sock);
  if (handle)
    SSL_free(handle);
  message_push(Sprintf("SSL error: %s, a workaround might be: w3m -insecure",
                       ERR_error_string(ERR_get_error(), NULL))
                   ->ptr);
  return NULL;
}

static void SSL_write_from_file(SSL *ssl, const char *file) {
  auto fd = fopen(file, "r");
  if (fd != NULL) {
    int c;
    while ((c = fgetc(fd)) != EOF) {
      char buf[1];
      buf[0] = c;
      SSL_write(ssl, buf, 1);
    }
    fclose(fd);
  }
}

static void write_from_file(int sock, const char *file) {
  auto fd = fopen(file, "r");
  if (fd != NULL) {
    int c;
    while ((c = fgetc(fd)) != EOF) {
      char buf[1];
      buf[0] = c;
      write(sock, buf, 1);
    }
    fclose(fd);
  }
}

struct Url *baseURL(struct Buffer *buf) {
  if (buf->bufferprop & BP_NO_URL) {
    /* no URL is defined for the buffer */
    return NULL;
  }

  if (buf->document->baseURL != NULL) {
    /* <BASE> tag is defined in the document */
    return buf->document->baseURL;
  } else if (IS_EMPTY_PARSED_URL(&buf->currentURL))
    return NULL;
  else
    return &buf->currentURL;
}

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

static Str parsedURL2RefererOriginStr(struct Url *pu) {
  Str s;
  const char *f = pu->file, *q = pu->query;

  pu->file = NULL;
  pu->query = NULL;
  s = _parsedURL2Str(pu, false, false, false);
  pu->file = f;
  pu->query = q;

  return s;
}

Str parsedURL2RefererStr(struct Url *pu) {
  return _parsedURL2Str(pu, false, false, false);
}

// if (hr->referer == NO_REFERER)
// else
// Strcat_charp(tmp, otherinfo(pu, NULL, NULL, no_cache));
// Strcat_charp(tmp, otherinfo(pu, current, hr->referer, no_cache));
static char *
otherinfo(struct HttpRequest *hr
          // struct Url *target, struct Url *current,
          //                      const char *referer, bool is_nocache
) {
  Str s = Strnew();
  const int *no_referer_ptr;
  int no_referer;
  const char *url_user_agent = query_SCONF_USER_AGENT(&hr->url);

  if (!override_user_agent) {
    Strcat_charp(s, "User-Agent: ");
    if (url_user_agent)
      Strcat_charp(s, url_user_agent);
    else if (UserAgent == NULL || *UserAgent == '\0')
      Strcat_charp(s, w3m_version);
    else
      Strcat_charp(s, UserAgent);
    Strcat_charp(s, "\r\n");
  }

  Strcat_m_charp(s, "Accept: ", AcceptMedia, "\r\n", NULL);
  Strcat_m_charp(s, "Accept-Encoding: ", AcceptEncoding, "\r\n", NULL);
  Strcat_m_charp(s, "Accept-Language: ", AcceptLang, "\r\n", NULL);

  if (hr->url.host) {
    Strcat_charp(s, "Host: ");
    Strcat_charp(s, hr->url.host);
    if (hr->url.port != DefaultPort[hr->url.scheme])
      Strcat(s, Sprintf(":%d", hr->url.port));
    Strcat_charp(s, "\r\n");
  }
  if (hr->no_cache || NoCache) {
    Strcat_charp(s, "Pragma: no-cache\r\n");
    Strcat_charp(s, "Cache-control: no-cache\r\n");
  }

  auto current = hr->referer == NO_REFERER ? nullptr : hr->current;
  auto referer = hr->referer == NO_REFERER ? nullptr : hr->referer;
  no_referer = NoSendReferer;
  no_referer_ptr = query_SCONF_NO_REFERER_FROM(current);
  no_referer = no_referer || (no_referer_ptr && *no_referer_ptr);
  no_referer_ptr = query_SCONF_NO_REFERER_TO(&hr->url);
  no_referer = no_referer || (no_referer_ptr && *no_referer_ptr);
  if (!no_referer) {
    int cross_origin = false;
    if (CrossOriginReferer && current && current->host &&
        (!hr->url.host || strcasecmp(current->host, hr->url.host) != 0 ||
         current->port != hr->url.port || current->scheme != hr->url.scheme))
      cross_origin = true;
    if (current && current->scheme == SCM_HTTPS &&
        hr->url.scheme != SCM_HTTPS) {
      /* Don't send Referer: if https:// -> http:// */
    } else if (referer == NULL && current && current->scheme != SCM_LOCAL &&
               current->scheme != SCM_LOCAL_CGI &&
               current->scheme != SCM_DATA &&
               (current->scheme != SCM_FTP ||
                (current->user == NULL && current->pass == NULL))) {
      Strcat_charp(s, "Referer: ");
      if (cross_origin)
        Strcat(s, parsedURL2RefererOriginStr(current));
      else
        Strcat(s, parsedURL2RefererStr(current));
      Strcat_charp(s, "\r\n");
    } else if (referer != NULL && referer != NO_REFERER) {
      Strcat_charp(s, "Referer: ");
      if (cross_origin)
        Strcat(s, parsedURL2RefererOriginStr(current));
      else
        Strcat_charp(s, referer);
      Strcat_charp(s, "\r\n");
    }
  }
  return s->ptr;
}

static Str HTTPrequestToStr(struct HttpRequest *hr) {
  Str tmp;
  struct TextListItem *i;
  Str cookie;
  tmp = HTTPrequestMethod(hr);
  Strcat_charp(tmp, " ");
  Strcat_charp(tmp, HTTPrequestURI(hr)->ptr);
  Strcat_charp(tmp, " HTTP/1.0\r\n");
  Strcat_charp(tmp, otherinfo(hr));
  if (hr->extra_header)
    for (i = hr->extra_header->first; i != NULL; i = i->next) {
      if (strncasecmp(i->ptr, "Authorization:", sizeof("Authorization:") - 1) ==
          0) {
        if (hr->command == HR_COMMAND_CONNECT)
          continue;
      }
      if (strncasecmp(i->ptr, "Proxy-Authorization:",
                      sizeof("Proxy-Authorization:") - 1) == 0) {
        if (hr->url.scheme == SCM_HTTPS && hr->command != HR_COMMAND_CONNECT)
          continue;
      }
      Strcat_charp(tmp, i->ptr);
    }

  if (hr->command != HR_COMMAND_CONNECT && use_cookie &&
      (cookie = find_cookie(&hr->url))) {
    Strcat_charp(tmp, "Cookie: ");
    Strcat(tmp, cookie);
    Strcat_charp(tmp, "\r\n");
    /* [DRAFT 12] s. 10.1 */
    if (cookie->ptr[0] != '$')
      Strcat_charp(tmp, "Cookie2: $Version=\"1\"\r\n");
  }
  if (hr->command == HR_COMMAND_POST) {
    if (hr->form->enctype == FORM_ENCTYPE_MULTIPART) {
      Strcat_charp(tmp, "Content-Type: multipart/form-data; boundary=");
      Strcat_charp(tmp, hr->form->boundary);
      Strcat_charp(tmp, "\r\n");
      Strcat(tmp, Sprintf("Content-Length: %ld\r\n", hr->form->length));
      Strcat_charp(tmp, "\r\n");
    } else {
      if (!override_content_type) {
        Strcat_charp(tmp,
                     "Content-Type: application/x-www-form-urlencoded\r\n");
      }
      Strcat(tmp, Sprintf("Content-Length: %ld\r\n", hr->form->length));
      if (header_string)
        Strcat(tmp, header_string);
      Strcat_charp(tmp, "\r\n");
      Strcat_charp_n(tmp, hr->form->body, hr->form->length);
      Strcat_charp(tmp, "\r\n");
    }
  } else {
    if (header_string)
      Strcat(tmp, header_string);
    Strcat_charp(tmp, "\r\n");
  }
#ifdef DEBUG
  fprintf(stderr, "HTTPrequest: [ %s ]\n\n", tmp->ptr);
#endif /* DEBUG */
  return tmp;
}

/* add index_file if exists */
union input_stream *add_index_file(struct Url *pu, union input_stream *stream) {
  struct TextList *index_file_list = NULL;
  if (non_null(index_file))
    index_file_list = make_domain_list(index_file);
  if (index_file_list == NULL) {
    return NULL;
  }

  for (auto ti = index_file_list->first; ti; ti = ti->next) {
    const char *p =
        Strnew_m_charp(pu->file, "/", file_quote(ti->ptr), NULL)->ptr;
    p = cleanupName(p);
    auto q = cleanupName(file_unquote(p));
    auto index = examineFile(q);
    if (index != NULL) {
      pu->file = p;
      pu->real_file = q;
      return index;
    }
  }

  return stream;
}

struct HttpResponse *
openURL(struct HttpRequest *hr,
        // const char *url, struct Url *pu,
        // struct Url *current, const char *referer, bool no_cache,
        // struct FormList *form, struct TextList *extra_header,
        union input_stream *ouf) {
  assert(hr);
  // struct HttpRequest hr0;
  // if (!hr) {
  //   hr = &hr0;
  // }

  // auto u = url;
  // auto scheme = getURLScheme(&u);
  // if (current == NULL && scheme == SCM_MISSING && !ArgvIsURL) {
  //   u = file_to_url(url); /* force to local file */
  // } else {
  //   u = url;
  // }
  // parseURL2(u, pu, current);

  auto res = newHttpResponse(hr);
  res->stream = ouf;

  if (hr->url.scheme == SCM_LOCAL && hr->url.file == NULL) {
    if (hr->url.label != NULL) {
      /* #hogege is not a label but a filename */
      Str tmp2 = Strnew_charp("#");
      Strcat_charp(tmp2, hr->url.label);
      hr->url.file = tmp2->ptr;
      hr->url.real_file = cleanupName(file_unquote(hr->url.file));
      hr->url.label = NULL;
    } else {
      /* given URL must be null string */
#ifdef SOCK_DEBUG
      sock_log("given URL must be null string\n");
#endif
      return res;
    }
  }

  if (LocalhostOnly && hr->url.host && !is_localhost(hr->url.host)) {
    hr->url.host = NULL;
  }

  switch (hr->url.scheme) {
  case SCM_LOCAL:
  case SCM_LOCAL_CGI:
    // if (hr->form && hr->form->body)
    //   /* local CGI: POST */
    res->stream = newFileStream(localcgi_request(hr), fclose);
    // else
    //   /* lodal CGI: GET */
    //   res->stream = newFileStream(
    //       localcgi_get(pu->real_file, pu->query, referer), fclose);
    if (res->stream) {
      // uf.is_cgi = true;
      hr->url.scheme = SCM_LOCAL_CGI;
      return res;
    }

    res->stream = examineFile(hr->url.real_file);
    if (!res->stream) {
      if (dir_exist(hr->url.real_file)) {
        add_index_file(&hr->url, res->stream);
        if (!res->stream)
          return nullptr;
      } else if (document_root != NULL) {
        auto tmp = Strnew_charp(document_root);
        if (Strlastchar(tmp) != '/' && hr->url.file[0] != '/')
          Strcat_char(tmp, '/');
        Strcat_charp(tmp, hr->url.file);
        auto p = cleanupName(tmp->ptr);
        auto q = cleanupName(file_unquote(p));
        if (dir_exist(q)) {
          hr->url.file = p;
          hr->url.real_file = q;
          res->stream = add_index_file(&hr->url, res->stream);
          if (!res->stream) {
            return nullptr;
          }
        } else {
          res->stream = examineFile(q);
          if (res->stream) {
            hr->url.file = p;
            hr->url.real_file = q;
          }
        }
      }
    }
    return res;

  case SCM_FTP:
  case SCM_FTPDIR:
    if (hr->url.file == NULL)
      hr->url.file = allocStr("/", -1);
    if (non_null(FTP_proxy) && use_proxy && hr->url.host != NULL &&
        !check_no_proxy(hr->url.host)) {
      hr->flag |= HR_FLAG_PROXY;
      SocketType sock;
      if (!socketOpen(FTP_proxy_parsed.host,
                      schemeNumToName(FTP_proxy_parsed.scheme),
                      FTP_proxy_parsed.port, &sock)) {
        return res;
      }
#ifdef _WIN32
      assert(false);
#else
      stream = newInputStream(sock);
#endif
      hr->url.scheme = SCM_HTTP;
      auto tmp = HTTPrequestToStr(hr);
      socketWrite(sock, tmp->ptr, tmp->length);
    } else {
      res->stream = openFTPStream(&hr->url);
      return res;
    }
    break;

  case SCM_HTTP:
  case SCM_HTTPS: {
    if (hr->url.file == NULL)
      hr->url.file = allocStr("/", -1);
    if (hr->form && hr->form->method == FORM_METHOD_POST && hr->form->body)
      hr->command = HR_COMMAND_POST;
    if (hr->form && hr->form->method == FORM_METHOD_HEAD)
      hr->command = HR_COMMAND_HEAD;

    Str tmp = nullptr;
    SocketType sock = socketInvalid();
    SSL *sslh = NULL;
    char *ssl_certificate = nullptr;
    if (((hr->url.scheme == SCM_HTTPS) ? non_null(HTTPS_proxy)
                                       : non_null(HTTP_proxy)) &&
        use_proxy && hr->url.host != NULL && !check_no_proxy(hr->url.host)) {
      hr->flag |= HR_FLAG_PROXY;
      if (hr->url.scheme == SCM_HTTPS && res->stream_status == STREAM_CONNECT) {
        sock = ssl_socket_of(ouf);
        char *ssl_certificate;
        if (!(sslh = openSSLHandle(sock, hr->url.host, &ssl_certificate))) {
          res->stream_status = STREAM_MISSING;
          return nullptr;
        }
      } else if (hr->url.scheme == SCM_HTTPS) {
        if (!socketOpen(HTTPS_proxy_parsed.host,
                        schemeNumToName(HTTPS_proxy_parsed.scheme),
                        HTTPS_proxy_parsed.port, &sock)) {
          return nullptr;
        }
        sslh = NULL;
      } else {
        if (!socketOpen(HTTP_proxy_parsed.host,
                        schemeNumToName(HTTP_proxy_parsed.scheme),
                        HTTP_proxy_parsed.port, &sock)) {
          return nullptr;
        }
        sslh = NULL;
      }
      if (hr->url.scheme == SCM_HTTPS) {
        if (res->stream_status == STREAM_NORMAL) {
          hr->command = HR_COMMAND_CONNECT;
          tmp = HTTPrequestToStr(hr);
          res->stream_status = STREAM_CONNECT;
        } else {
          hr->flag |= HR_FLAG_LOCAL;
          tmp = HTTPrequestToStr(hr);
          res->stream_status = STREAM_NORMAL;
        }
      } else {
        tmp = HTTPrequestToStr(hr);
        res->stream_status = STREAM_NORMAL;
      }
    } else {
      if (!socketOpen(hr->url.host, schemeNumToName(hr->url.scheme),
                      hr->url.port, &sock)) {
        res->stream_status = STREAM_MISSING;
        return nullptr;
      }
      if (hr->url.scheme == SCM_HTTPS) {
        if (!(sslh = openSSLHandle(sock, hr->url.host, &ssl_certificate))) {
          res->stream_status = STREAM_MISSING;
          return nullptr;
        }
      }
      hr->flag |= HR_FLAG_LOCAL;
      tmp = HTTPrequestToStr(hr);
      res->stream_status = STREAM_NORMAL;
    }
#ifdef _WIN32
    res->stream = newWinsockStream(sock);
#else
    res->stream = newInputStream(sock);
#endif

    if (hr->url.scheme == SCM_HTTPS) {
      res->stream = newSSLStream(sslh, sock);
      ssl_set_certificate(res->stream, ssl_certificate);
      if (sslh)
        SSL_write(sslh, tmp->ptr, tmp->length);
      else
        socketWrite(sock, tmp->ptr, tmp->length);
      if (hr->command == HR_COMMAND_POST &&
          hr->form->enctype == FORM_ENCTYPE_MULTIPART) {
        if (sslh)
          SSL_write_from_file(sslh, hr->form->body);
        else
          write_from_file(sock, hr->form->body);
      }
      return res;
    } else {
      socketWrite(sock, tmp->ptr, tmp->length);
      if (hr->command == HR_COMMAND_POST &&
          hr->form->enctype == FORM_ENCTYPE_MULTIPART)
        write_from_file(sock, hr->form->body);
    }
    break;
  }

  case SCM_DATA: {
    if (hr->url.file == NULL)
      return nullptr;
    auto p = Strnew_charp(hr->url.file)->ptr;
    auto q = strchr(p, ',');
    if (q == NULL)
      return nullptr;
    *q++ = '\0';
    auto tmp = Strnew_charp(q);
    q = strrchr(p, ';');
    if (q != NULL && !strcmp(q, ";base64")) {
      *q = '\0';
      // uf.encoding = ENC_BASE64;
    } else
      tmp = Str_url_unquote(tmp, false, false);
    res->stream = newStrStream(tmp);
    // uf.guess_type = (*p != '\0') ? p : "text/plain";
    return res;
  }

  default:
    break;
  }
  return nullptr;
}

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

// Str StrmyUFgets(struct URLFile *f) { return StrmyISgets(f->stream); }
// int UFgetc(struct URLFile *f) { return ISgetc(f->stream); }
// void UFundogetc(struct URLFile *f) { ISundogetc((f)->stream); }
// void UFclose(struct URLFile *f) {
//   if (ISclose((f)->stream) == 0) {
//     (f)->stream = NULL;
//   }
// }
// int UFfileno(struct URLFile *f) { return ISfileno((f)->stream); }

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
