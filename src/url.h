#pragma once
#include <optional>
#include "url_schema.h"

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

  Str *to_Str() const;
};

#define IS_EMPTY_PARSED_URL(pu) ((pu)->schema == SCM_UNKNOWN && !(pu)->file)

inline Str *Url2Str(const std::optional<Url> &pu) {
  if (pu) {
    return pu->to_Str();
  } else {
    return {};
  }
}
Str *Url2RefererStr(const Url *pu);
Str *_Url2Str(const Url *pu, int pass, int user, int label);

const char *url_decode0(const char *url);

bool checkRedirection(const Url *pu);
