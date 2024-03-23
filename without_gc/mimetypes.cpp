#include "mimetypes.h"
#include "quote.h"
#include "myctype.h"
#include <string.h>
#include <list>
#include <string>
#include <vector>
#include <fstream>

#define USER_MIMETYPES "~/.mime.types"
#ifndef ETC_DIR
#define ETC_DIR "/etc"
#endif
#define SYS_MIMETYPES ETC_DIR "/mime.types"

std::string mimetypes_files = USER_MIMETYPES ", " SYS_MIMETYPES;

static std::list<std::string> mimetypes_list;

struct table2 {
  std::string item1;
  std::string item2;
};
static std::list<std::vector<table2>> UserMimeTypes;

static std::vector<table2> DefaultGuess = {
    {"html", "text/html"},         {"htm", "text/html"},
    {"shtml", "text/html"},        {"xhtml", "application/xhtml+xml"},
    {"gif", "image/gif"},          {"jpeg", "image/jpeg"},
    {"jpg", "image/jpeg"},         {"png", "image/png"},
    {"xbm", "image/xbm"},          {"au", "audio/basic"},
    {"gz", "application/x-gzip"},  {"Z", "application/x-compress"},
    {"bz2", "application/x-bzip"}, {"tar", "application/x-tar"},
    {"zip", "application/x-zip"},  {"lha", "application/x-lha"},
    {"lzh", "application/x-lha"},  {"ps", "application/postscript"},
    {"pdf", "application/pdf"},
};

static std::vector<table2> loadMimeTypes(const char *filename) {
  std::ifstream f(expandPath(filename), std::ios::binary);
  if (!f) {
    return {};
  }

  std::string tmp;
  while (std::getline(f, tmp)) {
    if (tmp.empty() || tmp[0] == '#')
      continue;

    auto d = tmp.data();
    if (d[0] != '#') {
      d = strtok(d, " \t\n\r");
      if (d != NULL) {
        d = strtok(NULL, " \t\n\r");
        for (; d != NULL;)
          d = strtok(NULL, " \t\n\r");
      }
    }
  }
  f.seekg(0, std::ios_base::beg);

  std::vector<table2> mtypes;
  while (std::getline(f, tmp)) {
    if (tmp.empty() || tmp[0] == '#')
      continue;

    auto d = tmp.data();
    auto type = strtok(d, " \t\n\r");
    if (type == NULL)
      continue;
    while (1) {
      d = strtok(NULL, " \t\n\r");
      if (d == NULL)
        break;
      mtypes.push_back({
          .item1 = d,
          .item2 = type,
      });
    }
  }
  return mtypes;
}

void initMimeTypes() {
  if (non_null(mimetypes_files)) {
    make_domain_list(mimetypes_list, mimetypes_files);
  }
  if (mimetypes_list.empty()) {
    return;
  }
  UserMimeTypes.clear();
  // int i = 0;
  for (auto &tl : mimetypes_list) {
    UserMimeTypes.push_back(loadMimeTypes(tl.c_str()));
  }
}

static std::string guessContentTypeFromTable(const std::vector<table2> &table,
                                             std::string_view filename) {
  if (table.empty()) {
    return "";
  }

  auto p = &filename.back();
  while (filename < p && *p != '.')
    p--;
  if (p == filename)
    return "";
  p++;

  for (auto &t : table) {
    if (p == t.item1)
      return t.item2;
  }

  for (auto &t : table) {
    if (!strcasecmp(p, t.item1.c_str()))
      return t.item2;
  }

  return "";
}

std::string guessContentType(std::string_view filename) {
  if (filename.empty()) {
    return {};
  }

  for (auto &table : UserMimeTypes) {
    auto ret = guessContentTypeFromTable(table, filename);
    if (ret.size()) {
      return ret;
    }
  }

  return guessContentTypeFromTable(DefaultGuess, filename);
}
