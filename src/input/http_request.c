#include "http_request.h"
#include "alloc.h"
#include "input/url.h"

struct HttpRequest *newHttpRequest(struct Url url, struct FormList *form,
                                   const char *referer, bool no_cache,
                                   struct TextList *extra_header) {
  auto hr = New(struct HttpRequest);
  hr->url = url;
  hr->form = form;
  hr->command = HR_COMMAND_GET;
  hr->flag = 0;
  hr->referer = referer;
  hr->no_cache = no_cache;
  hr->form = form;
  hr->extra_header = extra_header;
  return hr;
}

Str HTTPrequestMethod(struct HttpRequest *hr) {
  switch (hr->command) {
  case HR_COMMAND_CONNECT:
    return Strnew_charp("CONNECT");
  case HR_COMMAND_POST:
    return Strnew_charp("POST");
    break;
  case HR_COMMAND_HEAD:
    return Strnew_charp("HEAD");
    break;
  case HR_COMMAND_GET:
  default:
    return Strnew_charp("GET");
  }
  return nullptr;
}

Str HTTPrequestURI(struct HttpRequest *hr) {
  Str tmp = Strnew();
  if (hr->command == HR_COMMAND_CONNECT) {
    Strcat_charp(tmp, hr->url.host);
    Strcat(tmp, Sprintf(":%d", hr->url.port));
  } else if (hr->flag & HR_FLAG_LOCAL) {
    Strcat_charp(tmp, hr->url.file);
    if (hr->url.query) {
      Strcat_char(tmp, '?');
      Strcat_charp(tmp, hr->url.query);
    }
  } else
    Strcat(tmp, _parsedURL2Str(&hr->url, true, true, false));
  return tmp;
}
