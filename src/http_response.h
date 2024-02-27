#pragma once
#include "url.h"
#include "optional"
#include "compression.h"
#include <stdint.h>
#include <vector>
#include <memory>
#include <list>
#include <string>

extern int FollowRedirection;
extern bool DecodeCTE;

struct HttpResponse : std::enable_shared_from_this<HttpResponse> {
  int http_response_code = 0;
  Url currentURL = {.schema = SCM_UNKNOWN};
  std::optional<Url> baseURL;
  std::optional<Url> getBaseURL() const {
    if (this->baseURL) {
      /* <BASE> tag is defined in the document */
      return *this->baseURL;
    } else if (this->currentURL.IS_EMPTY_PARSED_URL()) {
      return {};
    } else {
      return this->currentURL;
    }
  }

  std::string type = "text/plain";
  std::list<std::string> document_header;
  std::string filename;

  // cache / local filename / decompress
  std::string sourcefile;
  std::vector<uint8_t> raw;
  bool decoded = false;

  std::vector<Url> redirectins;
  const char *ssl_certificate = nullptr;
  const char *baseTarget = nullptr;
  size_t trbyte = 0;
  const char *edit = nullptr;
  long long current_content_length;

  HttpResponse();
  ~HttpResponse();
  bool checkRedirection(const Url &pu);
  int readHeader(const std::shared_ptr<class input_stream> &s, const Url &pu);
  const char *getHeader(const char *field) const;
  const char *checkContentType() const;
  const char *guess_save_name(const char *file) const;
  bool is_html_type() const {
    return type == "text/html" || type == "application/xhtml+xml";
  }
  FILE *createSourceFile();
  void page_loaded(Url url, const std::shared_ptr<input_stream> &stream);
  std::string last_modified() const;
  std::string_view getBody();
  CompressionType contentEncoding() const;
};

const char *mybasename(const char *s);
const char *guess_filename(const char *file);
