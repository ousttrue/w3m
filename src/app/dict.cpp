#include "dict.h"
#include "http_session.h"
#include "http_response.h"
#include "form.h"
#include <sstream>

bool UseDictCommand = true;
std::string DictCommand = "file:///$LIB/w3mdict" CGI_EXTENSION;

#define DICTBUFFERNAME "*dictionary*"
std::shared_ptr<HttpResponse> execdict(std::string_view word) {
  if (!UseDictCommand || word.empty()) {
    return {};
  }

  std::stringstream dictcmd;
  dictcmd << DictCommand << "?" << form_quote(word);
  auto res = loadGeneralFile(dictcmd.str(), {}, {.no_referer = true});
  if (!res) {
    // App::instance().disp_message("Execution failed");
    return {};
  }

  // std::stringstream title;
  // title << DICTBUFFERNAME << " " << word;
  // res->title = title.str();
  res->filename = word;
  if (res->type.empty()) {
    res->type = "text/plain";
  }
  return res;
}
