#pragma once
#include "url.h"
#include "compression.h"
#include <stdint.h>
#include <vector>
#include <memory>
#include <list>
#include <string>
#include <functional>

extern int FollowRedirection;

using GetsFunc = std::function<std::string()>;

struct HttpResponse : std::enable_shared_from_this<HttpResponse> {
  int http_response_code = 0;
  Url currentURL = {};

  std::string type = "text/plain";
  std::list<std::string> document_header;
  std::string filename;

  // cache / local filename / decompress
  std::string sourcefile;
  std::vector<uint8_t> raw;
  bool decoded = false;

  std::vector<Url> redirectins;
  const char *ssl_certificate = nullptr;
  const char *edit = nullptr;
  long long current_content_length;

  HttpResponse();
  ~HttpResponse();
  static std::shared_ptr<HttpResponse> fromHtml(const std::string &html) {
    auto res = std::make_shared<HttpResponse>();
    auto p = (const uint8_t *)html.data();
    res->raw.assign(p, p + html.size());
    res->type = "text/html";
    return res;
  }

  bool checkRedirection(const Url &pu);
  int readHeader(const GetsFunc &s, const Url &pu);
  void pushHeader(const Url &url, std::string_view lineBuf2);
  std::string getHeader(std::string_view field) const;
  std::string checkContentType() const;
  std::string guess_save_name(std::string) const;
  bool is_html_type() const {
    return type == "text/html" || type == "application/xhtml+xml";
  }
  FILE *createSourceFile();
  // void page_loaded(Url url, const std::string &body);
  std::string last_modified() const;
  std::string_view getBody();
  CompressionType contentEncoding() const;
};

std::string guess_filename(const char *file);
