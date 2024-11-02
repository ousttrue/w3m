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

struct Document *execdict(const char *word) {
  if (!UseDictCommand || !word || *word == '\0') {
    return nullptr;
  }

  const char *w = word;
  if (*w == '\0') {
    return nullptr;
  }

  auto dictcmd =
      Sprintf("%s?%s", DictCommand, Str_form_quote(Strnew_charp(w))->ptr)->ptr;
  auto doc =
      loadGeneralFile(INIT_BUFFER_WIDTH, dictcmd, NULL, NO_REFERER, 0, NULL);
  if (doc == NULL) {
    message_push("Execution failed");
    return nullptr;
  } 
  // else if (buf == NO_BUFFER) {
  //   return nullptr;
  // }

  doc->filename = w;
  // buf->buffername = Sprintf("%s %s", DICTBUFFERNAME, word)->ptr;
  if (doc->type == NULL)
    doc->type = "text/plain";
  return doc;
}
