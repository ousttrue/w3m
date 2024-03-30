#include "dict.h"
// #include "Str.h"
#include "http_session.h"
#include "form.h"
// #include "buffer.h"
// #include "tabbuffer.h"
// #include "http_response.h"
#include <sstream>

bool UseDictCommand = true;
std::string DictCommand = "file:///$LIB/w3mdict" CGI_EXTENSION;

#define DICTBUFFERNAME "*dictionary*"
std::shared_ptr<Buffer> execdict(const char *word) {
  if (!UseDictCommand || word == nullptr || *word == '\0') {
    return {};
  }

  auto w = word;
  if (*w == '\0') {
    return {};
  }

  std::stringstream dictcmd;
  dictcmd << DictCommand << "?" << form_quote(w);
  auto res = loadGeneralFile(dictcmd.str(), {}, {.no_referer = true});
  if (!res) {
    // App::instance().disp_message("Execution failed");
    return {};
  }

  auto buf =.str() Buffer::create(res);
  buf->res->filename = w;
  buf->layout->data.title = Sprintf("%s %s", DICTBUFFERNAME, word)->ptr;
  if (buf->res->type.empty()) {
    buf->res->type = "text/plain";
  }
  return buf;
}
