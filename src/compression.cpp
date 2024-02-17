#include "compression.h"
#include "etc.h"
#include "rc.h"
#include "mimetypes.h"
#include "myctype.h"
#include "url_stream.h"
#include "istream.h"
#include "textlist.h"
#include "alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#define PATH_SEPARATOR ':'

std::vector<compression_decoder> compression_decoders = {
    {CMP_COMPRESS,
     ".gz",
     "application/x-gzip",
     0,
     GUNZIP_CMDNAME,
     GUNZIP_NAME,
     "gzip",
     {"gzip", "x-gzip", nullptr},
     0},
    {CMP_COMPRESS,
     ".Z",
     "application/x-compress",
     0,
     GUNZIP_CMDNAME,
     GUNZIP_NAME,
     "compress",
     {"compress", "x-compress", nullptr},
     0},
    {CMP_BZIP2,
     ".bz2",
     "application/x-bzip",
     0,
     BUNZIP2_CMDNAME,
     BUNZIP2_NAME,
     "bzip, bzip2",
     {"x-bzip", "bzip", "bzip2", nullptr},
     0},
    {CMP_DEFLATE,
     ".deflate",
     "application/x-deflate",
     1,
     INFLATE_CMDNAME,
     INFLATE_NAME,
     "deflate",
     {"deflate", "x-deflate", nullptr},
     0},
    {CMP_BROTLI,
     ".br",
     "application/x-br",
     0,
     BROTLI_CMDNAME,
     BROTLI_NAME,
     "br",
     {"br", "x-br", nullptr},
     1},
};

const compression_decoder *
compress_application_type(CompressionType compression) {
  for (auto &d : compression_decoders) {
    if (d.type == compression)
      return &d;
  }
  return {};
}

const compression_decoder *check_compression(const char *path) {
  if (!path) {
    return {};
  }

  auto len = strlen(path);
  for (auto &d : compression_decoders) {
    if (d.ext.empty()) {
      continue;
    }
    auto elen = d.ext.size();
    if (len > elen && strcasecmp(&path[len - elen], d.ext.c_str()) == 0) {
      return &d;
    }
  }
  return {};
}

std::tuple<std::string, std::string> uncompressed_file_type(const char *path) {
  if (path == nullptr) {
    return {};
  }

  size_t slen = 0;
  auto len = strlen(path);
  compression_decoder *d = nullptr;
  for (auto &_d : compression_decoders) {
    auto d = &_d;
    if (d->ext.empty())
      continue;
    slen = d->ext.size();
    if (len > slen && strcasecmp(&path[len - slen], d->ext.c_str()) == 0)
      break;
  }
  if (!d) {
    return {};
  }

  auto fn = Strnew_charp(path);
  Strshrink(fn, slen);
  auto t0 = guessContentType(fn->ptr);
  if (t0 == nullptr) {
    t0 = "text/plain";
  }
  auto ext = filename_extension(fn->ptr, 0);
  return {t0, ext};
}

#define S_IXANY (S_IXUSR | S_IXGRP | S_IXOTH)

static int check_command(const char *cmd, int auxbin_p) {
  static char *path = nullptr;
  if (path == nullptr) {
    path = getenv("PATH");
  }

  Str *dirs;
  if (auxbin_p)
    dirs = Strnew_charp(w3m_auxbin_dir());
  else
    dirs = Strnew_charp(path);

  char *np;
  for (auto p = dirs->ptr; p != nullptr; p = np) {
    np = strchr(p, PATH_SEPARATOR);
    if (np)
      *np++ = '\0';
    auto pathname = Strnew();
    Strcat_charp(pathname, p);
    Strcat_char(pathname, '/');
    Strcat_charp(pathname, cmd);
    struct stat st;
    if (stat(pathname->ptr, &st) == 0 && S_ISREG(st.st_mode) &&
        (st.st_mode & S_IXANY) != 0)
      return 1;
  }
  return 0;
}

char *acceptableEncoding(void) {
  static Str *encodings = nullptr;
  if (encodings != nullptr) {
    return encodings->ptr;
  }

  auto l = newTextList();
  for (auto &d : compression_decoders) {
    if (check_command(d.cmd, d.auxbin_p)) {
      pushText(l, d.encoding);
    }
  }

  encodings = Strnew();
  while (auto p = popText(l)) {
    if (encodings->length)
      Strcat_charp(encodings, ", ");
    Strcat_charp(encodings, p);
  }
  return encodings->ptr;
}

void process_compression(Str *lineBuf2, UrlStream *uf) {
  auto p = lineBuf2->ptr + 17;
  while (IS_SPACE(*p))
    p++;
  uf->compression = CMP_NOCOMPRESS;
  for (auto &d : compression_decoders) {
    for (auto e = d.encodings; *e != nullptr; e++) {
      if (strncasecmp(p, *e, strlen(*e)) == 0) {
        uf->compression = d.type;
        break;
      }
    }
    if (uf->compression != CMP_NOCOMPRESS)
      break;
  }
  uf->content_encoding = uf->compression;
}

std::string filename_extension(const std::string_view path, bool is_url) {
  if (path.empty()) {
    return "";
  }

  auto p = path.begin();
  if (*p == '.') {
    p++;
  }

  const char *last_dot = "";
  for (; p != path.end(); p++) {
    if (*p == '.') {
      last_dot = p;
    } else if (is_url && *p == '?')
      break;
  }
  if (*last_dot != '.') {
    return "";
  }

  int i;
  for (i = 1; i < 8 && last_dot[i]; i++) {
    if (is_url && !IS_ALNUM(last_dot[i]))
      break;
  }
  return std::string(last_dot, last_dot + i);
}
