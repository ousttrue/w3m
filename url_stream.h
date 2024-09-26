#pragma once
#include <time.h>

union input_stream;
struct URLFile {
  unsigned char scheme;
  char is_cgi;
  char encoding;
  union input_stream *stream;
  char *ext;
  int compression;
  int content_encoding;
  char *guess_type;
  char *ssl_certificate;
  char *url;
  time_t modtime;
};
