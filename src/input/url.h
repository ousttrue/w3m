#pragma once
#include "url_scheme.h"
#include <string>
#include <optional>

/// https://datatracker.ietf.org/doc/html/rfc3986#section-3
struct Url {
  UrlScheme scheme = SCM_UNKNOWN;
  std::string user;
  std::string pass;
  std::string host;
  int port = 0;
  std::string file;
  std::string real_file;
  std::string query;
  std::string label;

  static Url parse(const char *url, std::optional<Url> current = {});

  Url &operator=(const Url &src) {
    this->scheme = src.scheme;
    this->port = src.port;
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
    return (this->scheme == SCM_UNKNOWN && this->file.empty());
  }

  void do_label(std::string_view p);
};

Url urlParse(const char *url, std::optional<Url> current = {});
