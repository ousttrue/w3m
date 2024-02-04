#pragma once
#include <time.h>
#include "url_schema.h"
#include "url.h"

#define SAVE_BUF_SIZE 1536

extern char *index_file;
extern bool use_lessopen;
extern bool PermitSaveToPipe;
extern bool AutoUncompress;
extern bool PreserveTimestamp;

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
struct TextList;
extern TextList *NO_proxy_domains;
extern Url HTTP_proxy_parsed;
extern Url HTTPS_proxy_parsed;
extern Url FTP_proxy_parsed;

union input_stream;
struct Url;
struct FormList;
struct TextList;
struct HttpRequest;
struct HttpOption;

struct UrlStream {
  UrlSchema schema;
  char is_cgi;
  char encoding;
  input_stream *stream;
  const char *ext;
  int compression;
  int content_encoding;
  const char *guess_type;
  const char *ssl_certificate;
  const char *url;
  time_t modtime;

  void openFile(const char *path);

  void openLocalCgi(Url *pu, Url *current, const HttpOption &option,
                    FormList *request, TextList *extra_header, UrlStream *ouf,
                    HttpRequest *hr, unsigned char *status);

private:
  void add_index_file(Url *pu);
};

UrlStream openURL(const char *url, Url *pu, Url *current,
                  const HttpOption &option, FormList *request,
                  TextList *extra_header, UrlStream *ouf, HttpRequest *hr,
                  unsigned char *status);

void init_stream(UrlStream *uf, UrlSchema schema, input_stream *stream);
int save2tmp(UrlStream *uf, const char *tmpf);
int doFileSave(UrlStream *uf, const char *defstr);
void UFhalfclose(UrlStream *f);
int check_no_proxy(const char *domain);
int openSocket(const char *hostname, const char *remoteport_name,
               unsigned short remoteport_num);

Url *schemaToProxy(UrlSchema schema);
