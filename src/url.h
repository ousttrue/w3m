#pragma once
#include "url_schema.h"
#include <string>
#include <optional>

struct Url {
  UrlSchema schema = SCM_UNKNOWN;
  std::string user;
  std::string pass;
  std::string host;
  int port = 0;
  std::string file;
  std::string real_file;
  std::string query;
  std::string label;
  bool is_nocache = false;

  static Url parse(const char *url, std::optional<Url> current = {});
  static Url parse2(const char *url, std::optional<Url> current = {});

  Url &operator=(const Url &src) {
    this->schema = src.schema;
    this->port = src.port;
    this->is_nocache = src.is_nocache;
    this->user = src.user;
    this->pass = src.pass;
    this->host = src.host;
    this->file = src.file;
    this->real_file = src.real_file;
    this->label = src.label;
    this->query = src.query;
    return *this;
  }

  std::string to_Str(bool pass, bool user, bool label) const;
  std::string to_Str() const { return to_Str(false, true, true); }
  std::string to_RefererStr() const { return to_Str(false, false, false); }
  std::string RefererOriginStr() const {
    Url u = *this;
    u.file = {};
    u.query = {};
    return u.to_Str(false, false, false);
  }

  bool IS_EMPTY_PARSED_URL() const {
    return (this->schema == SCM_UNKNOWN && this->file.empty());
  }
};
