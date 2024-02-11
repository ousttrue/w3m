#pragma once
#include "url.h"
#include "url_stream.h"
#include "optional"
#include <vector>
#include <memory>

extern int FollowRedirection;
extern bool DecodeCTE;

struct TextList;
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
  TextList *document_header = nullptr;
  std::string filename;

  // cache / local filename / decompress
  std::string sourcefile;

  std::vector<Url> redirectins;
  const char *ssl_certificate = nullptr;
  const char *baseTarget = nullptr;
  size_t trbyte = 0;
  const char *edit = nullptr;
  long long current_content_length;
  UrlStream f;

  HttpResponse();
  ~HttpResponse();
  bool checkRedirection(const Url &pu);
  int readHeader(struct UrlStream *uf, const Url &pu);
  const char *getHeader(const char *field) const;
  const char *checkContentType() const;
  const char *guess_save_name(const char *file) const;
  bool is_html_type() const {
    return type == "text/html" || type == "application/xhtml+xml";
  }
  FILE *createSourceFile();

  void page_loaded(Url url);
};

const char *mybasename(const char *s);
const char *guess_filename(const char *file);
