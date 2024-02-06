#pragma once
#include "url_schema.h"
#include <string>
#include <optional>

struct Str;
struct Url {
  UrlSchema schema = SCM_UNKNOWN;
  std::string user;
  std::string pass;
  std::string host;
  int port = 0;
  std::string file;
  std::string real_file;
  const char *query = {};
  const char *label = {};
  int is_nocache = {};

  static Url parse(const char *url, std::optional<Url> current = {});
  static Url parse2(const char *url, std::optional<Url> current = {});

  // Url(const Url &src) { *this = src; }
  Url &operator=(const Url &src);
  // void copyUrl(Url *p, const Url *q);
  bool same_url_p(const Url *pu2) const;

  std::string to_Str(bool pass, bool user, bool label) const;
  std::string to_Str() const { return to_Str(false, true, true); }
  std::string to_RefererStr() const { return to_Str(false, false, false); }
  std::string RefererOriginStr() const;

  bool IS_EMPTY_PARSED_URL() const {
    return (this->schema == SCM_UNKNOWN && this->file.empty());
  }
};

bool checkRedirection(const Url *pu);
char url_unquote_char(const char **pstr);
const char *url_quote(const char *str);
const char *url_decode0(const char *url);
Str *Str_url_unquote(Str *x, int is_form, int safe);
inline Str *Str_form_unquote(Str *x) { return Str_url_unquote(x, true, false); }
const char *url_unquote_conv0(const char *url);
#define url_unquote_conv(url, charset) url_unquote_conv0(url)

const char *file_quote(const char *str);
const char *file_unquote(const char *str);
const char *cleanupName(const char *name);
