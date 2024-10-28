#pragma once
#include "input/compression.h"
#include "input/encoding_type.h"
#include "input/stream_buffer.h"
#include "input/url.h"
#include "text/Str.h"
#include <stdint.h>

enum IST_TYPE {
  IST_BASIC = 0,
  IST_FILE = 1,
  IST_STR = 2,
  IST_SSL = 3,
  IST_ENCODED = 4,
#ifdef _WIN32
  IST_WS = 5,
#endif
};
union input_stream;

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

union input_stream *newInputStream(int des);
union input_stream *newFileStream(FILE *f, void (*closep)());
union input_stream *newStrStream(Str s);
union input_stream *newEncodedStream(union input_stream *is,
                                     enum ENCODING_TYPE encoding);
int ISclose(union input_stream *stream);
int ISgetc(union input_stream *stream);
int ISundogetc(union input_stream *stream);
Str StrISgets2(union input_stream *stream, char crnl);
#define StrISgets(stream) StrISgets2(stream, false)
#define StrmyISgets(stream) StrISgets2(stream, true)
struct growbuf;
void ISgets_to_growbuf(union input_stream *stream, struct growbuf *gb,
                       char crnl);
int ISread_n(union input_stream *stream, char *dst, int bufsize);
int ISfileno(union input_stream *stream);
int ISeos(union input_stream *stream);
void ssl_accept_this_site(char *hostname);

enum IST_TYPE IStype(union input_stream *stream);
int ssl_socket_of(union input_stream *stream);
union input_stream *openIS(const char *path);

extern struct TextList *NO_proxy_domains;
#define set_no_proxy(domains) (NO_proxy_domains = make_domain_list(domains))
extern Str header_string;

void url_stream_init();

union input_stream;
struct URLFile {
  enum URL_SCHEME_TYPE scheme;
  char is_cgi;
  enum ENCODING_TYPE encoding;
  union input_stream *stream;
  const char *ext;
  enum COMPRESSION_TYPE compression;
  int content_encoding;
  int64_t current_content_length;
  const char *guess_type;
  char *ssl_certificate;
  char *url;
  time_t modtime;
};

#define StrUFgets(f) StrISgets((f)->stream)
#define StrmyUFgets(f) StrmyISgets((f)->stream)
#define UFgetc(f) ISgetc((f)->stream)
#define UFundogetc(f) ISundogetc((f)->stream)
#define UFclose(f)                                                             \
  if (ISclose((f)->stream) == 0) {                                             \
    (f)->stream = NULL;                                                        \
  }
#define UFfileno(f) ISfileno((f)->stream)

void UFhalfclose(struct URLFile *f);
const char *guessContentType(const char *filename);
void examineFile(const char *path, struct URLFile *uf);

enum HttpStatus {
  HTST_UNKNOWN = 255,
  HTST_MISSING = 254,
  HTST_NORMAL = 0,
  HTST_CONNECT = 1,
};

struct HttpRequest;

/* flags for loadGeneralFile */
enum RG_FLAGS {
  RG_NOCACHE = 1,
};

struct URLOption {
  const char *referer;
  enum RG_FLAGS flag;
};

struct FormList;
extern struct URLFile openURL(const char *url, struct Url *pu,
                              struct Url *current, struct URLOption *option,
                              struct FormList *request,
                              struct TextList *extra_header,
                              struct URLFile *ouf, struct HttpRequest *hr,
                              enum HttpStatus *status);

struct Url *schemeToProxy(enum URL_SCHEME_TYPE scheme);
extern int save2tmp(struct URLFile uf, char *tmpf);
extern void free_ssl_ctx();
extern void init_stream(struct URLFile *uf, int scheme,
                        union input_stream *stream);
extern int check_no_proxy(char *domain);

void close_for_ftp(union input_stream *is);
union input_stream *newInputFtp(int sock);
