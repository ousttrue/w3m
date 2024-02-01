#pragma once
#include <time.h>
#include "url_schema.h"

#define SAVE_BUF_SIZE 1536

extern char *index_file;
extern bool use_lessopen;
extern bool PermitSaveToPipe;

struct URLOption {
  char *referer;
  int flag;
};

union input_stream;
struct UrlStream {
  UrlSchema schema;
  char is_cgi;
  char encoding;
  input_stream *stream;
  const char *ext;
  int compression;
  int content_encoding;
  const char *guess_type;
  const char *ssl_certificate;
  const char *url;
  time_t modtime;
};

struct ParsedURL;
struct FormList;
struct TextList;
struct HRequest;
UrlStream openURL(char *url, ParsedURL *pu, ParsedURL *current,
                  URLOption *option, FormList *request, TextList *extra_header,
                  UrlStream *ouf, HRequest *hr, unsigned char *status);

void init_stream(UrlStream *uf, UrlSchema schema, input_stream *stream);
void examineFile(const char *path, UrlStream *uf);
int save2tmp(UrlStream *uf, const char *tmpf);
int doFileSave(UrlStream *uf, const char *defstr);
void UFhalfclose(UrlStream *f);
