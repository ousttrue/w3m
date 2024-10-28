#pragma once
#include "input/compression.h"
#include "input/encoding_type.h"
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
union input_stream *newInputStream(int des);
union input_stream *newFileStream(FILE *f, void (*closep)());
union input_stream *newStrStream(Str s);
union input_stream *newEncodedStream(union input_stream *is,
                                     enum ENCODING_TYPE encoding);
int ISclose(union input_stream *stream);
int ISgetc(union input_stream *stream);
int ISundogetc(union input_stream *stream);
Str StrISgets2(union input_stream *stream, char crnl);
Str StrISgets(union input_stream *stream);
Str StrmyISgets(union input_stream *stream);
int ISread_n(union input_stream *stream, char *dst, int bufsize);
int ISfileno(union input_stream *stream);
int ISeos(union input_stream *stream);
enum IST_TYPE IStype(union input_stream *stream);
union input_stream *openIS(const char *path);

void ssl_accept_this_site(const char *hostname);
int ssl_socket_of(union input_stream *stream);

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
enum RG_FLAGS {
  RG_NOCACHE = 1,
};
struct URLOption {
  const char *referer;
  enum RG_FLAGS flag;
};
struct FormList;
struct TextList;
struct HttpRequest;
extern struct URLFile openURL(const char *url, struct Url *pu,
                              struct Url *current, struct URLOption *option,
                              struct FormList *request,
                              struct TextList *extra_header,
                              struct URLFile *ouf, struct HttpRequest *hr,
                              enum HttpStatus *status);

extern int save2tmp(struct URLFile uf, char *tmpf);
extern void free_ssl_ctx();
extern void init_stream(struct URLFile *uf, int scheme,
                        union input_stream *stream);
void close_for_ftp(union input_stream *is);
union input_stream *newInputFtp(int sock);
