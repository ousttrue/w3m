#pragma once
#include <optional>
#include "url_schema.h"

extern bool ArgvIsURL;
extern bool LocalhostOnly;
extern bool retryAsHttp;
extern char *HTTP_proxy;
extern char *HTTPS_proxy;
extern char *FTP_proxy;
extern char *NO_proxy;
extern int NOproxy_netaddr;
extern bool use_proxy;
// #define Do_not_use_proxy (!use_proxy)
extern const char *w3m_reqlog;
struct TextList;
extern TextList *NO_proxy_domains;

extern const char *mimetypes_files;

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
};

extern Url HTTP_proxy_parsed;
extern Url HTTPS_proxy_parsed;
extern Url FTP_proxy_parsed;

#define IS_EMPTY_PARSED_URL(pu) ((pu)->schema == SCM_UNKNOWN && !(pu)->file)

Str *Url2Str(const Url *pu);

inline Str *Url2Str(const std::optional<Url> &pu) {
  if (pu) {
    return Url2Str(&*pu);
  } else {
    return {};
  }
}

Str *Url2RefererStr(const Url *pu);
Str *_Url2Str(const Url *pu, int pass, int user, int label);
Url *schemaToProxy(UrlSchema schema);
const char *url_decode0(const char *url);

bool checkRedirection(const Url *pu);
