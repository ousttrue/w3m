#pragma once
#include <time.h>

extern struct TextList *NO_proxy_domains;
#define set_no_proxy(domains) (NO_proxy_domains = make_domain_list(domains))

void url_stream_init();

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

#define StrUFgets(f) StrISgets((f)->stream)
#define StrmyUFgets(f) StrmyISgets((f)->stream)
#define UFgetc(f) ISgetc((f)->stream)
#define UFundogetc(f) ISundogetc((f)->stream)
#define UFclose(f)                                                             \
  if (ISclose((f)->stream) == 0) {                                             \
    (f)->stream = NULL;                                                        \
  }
#define UFfileno(f) ISfileno((f)->stream)
