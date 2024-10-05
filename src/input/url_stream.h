#pragma once
#include <time.h>
#include <stdint.h>
#include "Str.h"
#include "url.h"
#include "compression.h"
#include "encoding_type.h"

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
bool is_html_type(const char *type);
bool is_text_type(const char *type);
bool is_plain_text_type(const char *type);

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
extern struct URLFile openURL(const char *url, struct Url *pu, struct Url *current,
                              struct URLOption *option,
                              struct FormList *request,
                              struct TextList *extra_header,
                              struct URLFile *ouf, struct HttpRequest *hr,
                              enum HttpStatus *status);

struct Url *schemeToProxy(enum URL_SCHEME_TYPE scheme);
extern int save2tmp(struct URLFile uf, char *tmpf);
extern void free_ssl_ctx();
extern void init_stream(struct URLFile *uf, int scheme,
                        union input_stream *stream);
extern struct TextList *make_domain_list(char *domain_list);
extern int check_no_proxy(char *domain);
