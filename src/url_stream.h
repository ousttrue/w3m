#pragma once
#include <time.h>
#include "encoding.h"
#include "url_schema.h"
#include "istream.h"
#include "compression.h"
#include "url.h"

#define SAVE_BUF_SIZE 1536

extern char *index_file;
extern bool PermitSaveToPipe;
extern bool AutoUncompress;
extern bool PreserveTimestamp;
extern bool ArgvIsURL;
extern bool LocalhostOnly;
extern bool retryAsHttp;

class input_stream;
struct Url;
struct FormList;
struct TextList;
struct HttpRequest;
struct HttpOption;

struct UrlStream {
  const char *url = {};
  UrlSchema schema = {};
  bool is_cgi = false;
  EncodingType encoding = ENC_7BIT;
  std::shared_ptr<input_stream> stream;
  std::string ext;
  CompressionType compression = CMP_NOCOMPRESS;
  int content_encoding = CMP_NOCOMPRESS;
  std::string guess_type;
  const char *ssl_certificate = {};
  time_t modtime = -1;

  UrlStream(UrlSchema schema, const std::shared_ptr<input_stream> &stream = {})
      : schema(schema), stream(stream) {}

  void openFile(const char *path);

  std::shared_ptr<HttpRequest> openURL(std::string_view url,
                                       std::optional<Url> current,
                                       const HttpOption &option,
                                       FormList *request);

  int doFileSave(const char *defstr);
  std::string uncompress_stream();

private:
  void openHttp(const std::shared_ptr<HttpRequest> &hr,
                const HttpOption &option, FormList *request);
  void openLocalCgi(const std::shared_ptr<HttpRequest> &hr,
                    const HttpOption &option, FormList *request);
  void openData(const Url &pu);
  void add_index_file(Url *pu);
};

int save2tmp(const std::shared_ptr<input_stream> &stream, const char *tmpf);

const char *file_quote(const char *str);
const char *file_unquote(const char *str);
const char *cleanupName(const char *name);
