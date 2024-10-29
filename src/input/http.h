#pragma once
#include "input/url.h"
#include <stdint.h>

extern int FollowRedirection;
extern bool retryAsHttp;
extern bool override_user_agent;
extern const char *UserAgent;
extern const char *AcceptLang;
extern const char *AcceptEncoding;
extern const char *AcceptMedia;
extern bool NoCache;
extern bool NoSendReferer;
extern bool CrossOriginReferer;
extern bool override_content_type;

#define NO_REFERER ((char *)-1)

enum HttpRequestMethod {
  HR_COMMAND_GET = 0,
  HR_COMMAND_POST = 1,
  HR_COMMAND_CONNECT = 2,
  HR_COMMAND_HEAD = 3,
};

enum HttpRequestFlags {
  HR_FLAG_LOCAL = 1,
  HR_FLAG_PROXY = 2,
};

struct HttpRequest {
  struct Url url;
  enum HttpRequestFlags flag;
  enum HttpRequestMethod command;
  struct FormList *form;
  struct TextList *extra_header;
  const char *referer;
  struct Url *current;
  bool no_cache;
};

enum ContentType {
  CONTENTTYPE_TextPlane,
  CONTENTTYPE_TextHTml,
};

enum CharSet {
  CHARSET_UNKONWN,
  CHARSET_UTF8,
  // CHARSET_ISO_8859_1, 1byte subset of UTF8
  CHARSET_SJIS,
};

enum StreamStatus {
  STREAM_UNKNOWN = 255,
  STREAM_MISSING = 254,
  STREAM_NORMAL = 0,
  STREAM_CONNECT = 1,
};

struct HttpResponse {
  struct HttpRequest *request;

  enum StreamStatus stream_status;
  union input_stream *stream;

  int http_status_code;
  struct TextList *document_header;
  enum ContentType content_type;
  enum CharSet content_charset;
  int64_t content_length;
};

// HttpRequest
struct HttpRequest *newHttpRequest(struct Url url, struct FormList *form,
                                   const char *referer, bool no_cache,
                                   struct TextList *extra_header);
Str HTTPrequestMethod(struct HttpRequest *hr);
Str HTTPrequestURI(struct HttpRequest *hr);
Str HTTPrequestToStr(struct HttpRequest *hr);
// HttpResponse
struct HttpResponse *newHttpResponse(struct HttpRequest *req);
void httpReadResponse(struct HttpResponse *res);
const char *httpGetHeader(struct HttpResponse *res, const char *field);
const char *httpGetContentType(struct HttpResponse *res);
const char *guess_save_name(struct HttpResponse *buf, const char *file);

void clearRedirection();
struct HttpResponse *sendHttpRequest(struct HttpRequest *req,
                                     union input_stream *of,
                                     bool add_auth_cookie_flag, Str realm,
                                     Str uname, Str pwd);
