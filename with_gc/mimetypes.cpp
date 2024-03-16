#include "mimetypes.h"
#include "quote.h"
#include "myctype.h"
#include "etc.h"
#include "alloc.h"
#include "Str.h"
#include <string.h>
#include <list>
#include <string>

#define USER_MIMETYPES "~/.mime.types"
#ifndef ETC_DIR
#define ETC_DIR "/etc"
#endif
#define SYS_MIMETYPES ETC_DIR "/mime.types"

const char *mimetypes_files = USER_MIMETYPES ", " SYS_MIMETYPES;

static std::list<std::string> mimetypes_list;

struct table2 {
  const char *item1;
  const char *item2;
};
static struct table2 **UserMimeTypes;

static struct table2 DefaultGuess[] = {
    {"html", "text/html"},         {"htm", "text/html"},
    {"shtml", "text/html"},        {"xhtml", "application/xhtml+xml"},
    {"gif", "image/gif"},          {"jpeg", "image/jpeg"},
    {"jpg", "image/jpeg"},         {"png", "image/png"},
    {"xbm", "image/xbm"},          {"au", "audio/basic"},
    {"gz", "application/x-gzip"},  {"Z", "application/x-compress"},
    {"bz2", "application/x-bzip"}, {"tar", "application/x-tar"},
    {"zip", "application/x-zip"},  {"lha", "application/x-lha"},
    {"lzh", "application/x-lha"},  {"ps", "application/postscript"},
    {"pdf", "application/pdf"},    {NULL, NULL}};

static struct table2 *loadMimeTypes(const char *filename) {
  auto f = fopen(expandPath(filename).c_str(), "r");
  if (f == NULL) {
    return NULL;
  }
  int n = 0;
  Str *tmp;
  while (tmp = Strfgets(f), tmp->length > 0) {
    auto d = tmp->ptr;
    if (d[0] != '#') {
      d = strtok(d, " \t\n\r");
      if (d != NULL) {
        d = strtok(NULL, " \t\n\r");
        int i = 0;
        for (; d != NULL; i++)
          d = strtok(NULL, " \t\n\r");
        n += i;
      }
    }
  }
  fseek(f, 0, 0);

  auto mtypes = (struct table2 *)New_N(struct table2, n + 1);
  int i = 0;
  while (tmp = Strfgets(f), tmp->length > 0) {
    auto d = tmp->ptr;
    if (d[0] == '#')
      continue;
    auto type = strtok(d, " \t\n\r");
    if (type == NULL)
      continue;
    while (1) {
      d = strtok(NULL, " \t\n\r");
      if (d == NULL)
        break;
      mtypes[i].item1 = Strnew_charp(d)->ptr;
      mtypes[i].item2 = Strnew_charp(type)->ptr;
      i++;
    }
  }
  mtypes[i].item1 = NULL;
  mtypes[i].item2 = NULL;
  fclose(f);
  return mtypes;
}

void initMimeTypes() {
  if (non_null(mimetypes_files)) {
    make_domain_list(mimetypes_list, mimetypes_files);
  }
  if (mimetypes_list.empty()) {
    return;
  }
  UserMimeTypes =
      (struct table2 **)New_N(struct table2 *, mimetypes_list.size());
  int i = 0;
  for (auto &tl : mimetypes_list) {
    UserMimeTypes[i++] = loadMimeTypes(tl.c_str());
  }
}

static const char *guessContentTypeFromTable(struct table2 *table,
                                             const char *filename) {
  if (table == NULL)
    return NULL;
  auto p = &filename[strlen(filename) - 1];
  while (filename < p && *p != '.')
    p--;
  if (p == filename)
    return NULL;
  p++;

  for (auto t = table; t->item1; t++) {
    if (!strcmp(p, t->item1))
      return t->item2;
  }
  for (auto t = table; t->item1; t++) {
    if (!strcasecmp(p, t->item1))
      return t->item2;
  }
  return NULL;
}

const char *guessContentType(const char *filename) {
  if (!filename) {
    return {};
  }

  for (size_t i = 0; i < mimetypes_list.size(); i++) {
    if (auto ret = guessContentTypeFromTable(UserMimeTypes[i], filename)) {
      return ret;
    }
  }

  return guessContentTypeFromTable(DefaultGuess, filename);
}
