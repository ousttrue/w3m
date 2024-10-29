#include "http_request.h"
#include "input/url.h"
#include <string.h>

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


Str HTTPrequestURI(struct Url *pu, struct HttpRequest *hr) {
  Str tmp = Strnew();
  if (hr->command == HR_COMMAND_CONNECT) {
    Strcat_charp(tmp, pu->host);
    Strcat(tmp, Sprintf(":%d", pu->port));
  } else if (hr->flag & HR_FLAG_LOCAL) {
    Strcat_charp(tmp, pu->file);
    if (pu->query) {
      Strcat_char(tmp, '?');
      Strcat_charp(tmp, pu->query);
    }
  } else
    Strcat(tmp, _parsedURL2Str(pu, true, true, false));
  return tmp;
}
