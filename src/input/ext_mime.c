#include "ext_mime.h"
#include "alloc.h"
#include "core.h"
#include "text/Str.h"
#include "text/text.h"
#include "text/textlist.h"
#include <stdio.h>

#define USER_MIMETYPES "~/.mime.types"
#define SYS_MIMETYPES ETC_DIR "/mime.types"

static struct ExtMime DefaultGuess[] = {
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

const char *mimetypes_files = USER_MIMETYPES ", " SYS_MIMETYPES;

static struct TextList *mimetypes_list = nullptr;
static struct ExtMime **UserMimeTypes = nullptr;

static struct ExtMime *loadMimeTypes(char *filename) {
  auto f = fopen(expandPath(filename), "r");
  if (f == NULL)
    return NULL;

  int n = 0;
  Str tmp;
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
  auto mtypes = New_N(struct ExtMime, n + 1);
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
      mtypes[i].ext = Strnew_charp(d)->ptr;
      mtypes[i].mime = Strnew_charp(type)->ptr;
      i++;
    }
  }

  mtypes[i].ext = NULL;
  mtypes[i].mime = NULL;
  fclose(f);
  return mtypes;
}

void initMimeTypes() {
  if (non_null(mimetypes_files))
    mimetypes_list = make_domain_list(mimetypes_files);
  else
    mimetypes_list = NULL;
  if (mimetypes_list == NULL)
    return;

  UserMimeTypes = New_N(struct ExtMime *, mimetypes_list->nitem);
  struct TextListItem *tl = mimetypes_list->first;
  for (int i = 0; tl; i++, tl = tl->next)
    UserMimeTypes[i] = loadMimeTypes(tl->ptr);
}

static const char *guessContentTypeFromTable(struct ExtMime *table,
                                             const char *filename) {
  if (!table)
    return NULL;
  auto p = &filename[strlen(filename) - 1];
  while (filename < p && *p != '.')
    p--;
  if (p == filename)
    return NULL;
  p++;
  for (auto t = table; t->ext; t++) {
    if (!strcmp(p, t->ext))
      return t->mime;
  }
  for (auto t = table; t->ext; t++) {
    if (!strcasecmp(p, t->ext))
      return t->mime;
  }
  return NULL;
}

const char *guessContentType(const char *filename) {

  if (filename == NULL)
    return NULL;
  if (mimetypes_list == NULL)
    goto no_user_mimetypes;

  for (int i = 0; i < mimetypes_list->nitem; i++) {
    auto ret = guessContentTypeFromTable(UserMimeTypes[i], filename);
    if (ret) {
      return ret;
    }
  }

no_user_mimetypes:
  return guessContentTypeFromTable(DefaultGuess, filename);
}
