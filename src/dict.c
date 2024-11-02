#include "dict.h"
#include "buffer/buffer.h"
#include "buffer/document.h"
#include "buffer/message.h"
#include "html/form.h"
#include "input/http.h"
#include "input/loader.h"
#include "input/localcgi.h"
#include "term/termsize.h"
#include "text/Str.h"

bool UseDictCommand = true;
const char *DictCommand = "file:///$LIB/w3mdict" CGI_EXTENSION;
#define DICTBUFFERNAME "*dictionary*"

struct Buffer *execdict(const char *word) {
  if (!UseDictCommand || !word || *word == '\0') {
    return nullptr;
  }

  const char *w = word;
  if (*w == '\0') {
    return nullptr;
  }

  auto dictcmd =
      Sprintf("%s?%s", DictCommand, Str_form_quote(Strnew_charp(w))->ptr)->ptr;
  auto buf =
      loadGeneralFile(INIT_BUFFER_WIDTH, dictcmd, NULL, NO_REFERER, 0, NULL);
  if (buf == NULL) {
    message_push("Execution failed");
    return nullptr;
  } else if (buf == NO_BUFFER) {
    return nullptr;
  }

  buf->document->filename = w;
  buf->buffername = Sprintf("%s %s", DICTBUFFERNAME, word)->ptr;
  if (buf->document->type == NULL)
    buf->document->type = "text/plain";
  return buf;
}
