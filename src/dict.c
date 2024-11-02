#include "dict.h"
#include "buffer/message.h"
#include "html/form.h"
#include "input/content.h"
#include "input/http.h"
#include "input/loader.h"
#include "input/localcgi.h"
#include "text/Str.h"

bool UseDictCommand = true;
const char *DictCommand = "file:///$LIB/w3mdict" CGI_EXTENSION;
#define DICTBUFFERNAME "*dictionary*"

struct Content *execdict(const char *word) {
  if (!UseDictCommand || !word || *word == '\0') {
    return nullptr;
  }

  const char *w = word;
  if (*w == '\0') {
    return nullptr;
  }

  auto dictcmd =
      Sprintf("%s?%s", DictCommand, Str_form_quote(Strnew_charp(w))->ptr)->ptr;
  auto content = loadGeneralFile(dictcmd, NULL, NO_REFERER, 0, NULL);
  if (!content) {
    message_push("Execution failed");
    return nullptr;
  }

  content->filename = w;
  // buf->buffername = Sprintf("%s %s", DICTBUFFERNAME, word)->ptr;
  // if (content->type == NULL)
  //   doc->type = "text/plain";
  return content;
}
