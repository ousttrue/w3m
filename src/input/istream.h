#pragma once
// #include "input/encoding_type.h"
#include "input/url.h"
#include "text/Str.h"
#include <stdint.h>

extern Str header_string;

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
// union input_stream *newEncodedStream(union input_stream *is,
//                                      enum ENCODING_TYPE encoding);
int ISclose(union input_stream *stream);
int ISgetc(union input_stream *stream);
int ISundogetc(union input_stream *stream);
Str StrISgets2(union input_stream *stream, char crnl);
Str StrISgets(union input_stream *stream);
Str StrmyISgets(union input_stream *stream);
Str StrISreadAll(union input_stream *stream);
int ISread_n(union input_stream *stream, char *dst, int bufsize);
int ISfileno(union input_stream *stream);
int ISeos(union input_stream *stream);
enum IST_TYPE IStype(union input_stream *stream);
union input_stream *openIS(const char *path);
void ssl_accept_this_site(const char *hostname);
int ssl_socket_of(union input_stream *stream);
const char *ssl_certificate(union input_stream *stream);
void ssl_set_certificate(union input_stream *stream,
                         const char *ssl_certificate);
union input_stream;
const char *guessContentType(const char *filename);
union input_stream *examineFile(const char *path);

enum StreamStatus {
  STREAM_UNKNOWN = 255,
  STREAM_MISSING = 254,
  STREAM_NORMAL = 0,
  STREAM_CONNECT = 1,
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
struct HttpResponse *openURL(const char *url, struct Url *pu,
                            struct Url *current, struct URLOption *option,
                            struct FormList *form,
                            struct TextList *extra_header,
                            union input_stream *ouf, struct HttpRequest *hr,
                            enum StreamStatus *status);

int save2tmp(union input_stream *stream, const char *tmpf);
void free_ssl_ctx();
void close_for_ftp(union input_stream *is);
union input_stream *newInputFtp(int sock);
