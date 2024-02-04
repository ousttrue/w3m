#pragma once
#include <time.h>
#include "url_schema.h"
#include "istream.h"
#include "compression.h"
#include "url.h"

#define SAVE_BUF_SIZE 1536

extern char *index_file;
extern bool use_lessopen;
extern bool PermitSaveToPipe;
extern bool AutoUncompress;
extern bool PreserveTimestamp;
extern bool ArgvIsURL;
extern bool LocalhostOnly;
extern bool retryAsHttp;

union input_stream;
struct Url;
struct FormList;
struct TextList;
struct HttpRequest;
struct HttpOption;

enum StreamStatus {
  HTST_UNKNOWN = 255,
  HTST_MISSING = 254,
  HTST_NORMAL = 0,
  HTST_CONNECT = 1,
};

struct UrlStream {
  const char *url = {};
  UrlSchema schema = {};
  bool is_cgi = false;
  char encoding = ENC_7BIT;
  input_stream *stream = {};
  const char *ext = {};
  int compression = CMP_NOCOMPRESS;
  int content_encoding = CMP_NOCOMPRESS;
  const char *guess_type = {};
  const char *ssl_certificate = {};
  time_t modtime = -1;

  UrlStream(UrlSchema schema, input_stream *stream = {})
      : schema(schema), stream(stream) {}

  void openFile(const char *path);

  StreamStatus openURL(const char *url, Url *pu, Url *current,
                       const HttpOption &option, FormList *request,
                       TextList *extra_header, HttpRequest *hr);

  int save2tmp(const char *tmpf) const;
  int doFileSave(const char *defstr);
  void close();
  uint8_t getc() const;
  void undogetc();
  int fileno() const;

private:
  StreamStatus openHttp(const char *url, Url *pu, Url *current,
                        const HttpOption &option, FormList *request,
                        TextList *extra_header, HttpRequest *hr);
  void openLocalCgi(Url *pu, Url *current, const HttpOption &option,
                    FormList *request, TextList *extra_header, HttpRequest *hr);
  void openData(Url *pu);
  void add_index_file(Url *pu);
};
