#pragma once
#include "url_schema.h"
#include <string>

struct Str;
struct Url {
  UrlSchema schema = {};
  const char *user = {};
  const char *pass = {};
  const char *host = {};
  int port = {};
  const char *file = {};
  const char *real_file = {};
  const char *query = {};
  const char *label = {};
  int is_nocache = {};

  static Url parse(const char *url, const Url *current = {});
  static Url parse2(const char *url, const Url *current = {});

  // Url(const Url &src) { *this = src; }
  Url &operator=(const Url &src);
  // void copyUrl(Url *p, const Url *q);
  bool same_url_p(const Url *pu2) const;

  std::string to_Str(bool pass, bool user, bool label) const;
  std::string to_Str() const { return to_Str(false, true, true); }
  std::string to_RefererStr() const { return to_Str(false, false, false); }

  bool IS_EMPTY_PARSED_URL() const {
    return (this->schema == SCM_UNKNOWN && !this->file);
  }
};

const char *url_decode0(const char *url);

bool checkRedirection(const Url *pu);
