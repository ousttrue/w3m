#include "compression.h"
#include "etc.h"
#include "quote.h"
#include "rc.h"
#include "mimetypes.h"
#include "myctype.h"
#include "url_stream.h"
#include "Str.h"
#include "alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <sstream>
#include <zlib.h>

#ifdef _MSC_VER
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#define PATH_SEPARATOR ';'
#else
#define PATH_SEPARATOR ':'
#endif

// https://stackoverflow.com/questions/11238918/s-isreg-macro-undefined
#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

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

static int check_command(const char *cmd, bool auxbin_p) {
  static char *path = nullptr;
  if (!path) {
    path = getenv("PATH");
  }

  Str *dirs;
  if (auxbin_p) {
    dirs = Strnew_charp(w3m_auxbin_dir());
  } else {
    dirs = Strnew_charp(path);
  }

  char *np;
  for (auto p = dirs->ptr; p != nullptr; p = np) {
    np = strchr(p, PATH_SEPARATOR);
    if (np) {
      *np++ = '\0';
    }
    auto pathname = Strnew();
    Strcat_charp(pathname, p);
    Strcat_char(pathname, '/');
    Strcat_charp(pathname, cmd);
    struct stat st;
    if (stat(pathname->ptr, &st) == 0 && S_ISREG(st.st_mode)
#ifndef _MSC_VER
        && (st.st_mode & S_IXANY) != 0
#endif
    ) {
      return true;
    }
  }
  return false;
}

const char *acceptableEncoding() {
  static std::string encodings;
  if (encodings.empty()) {
    std::list<std::string> l;
    for (auto &d : compression_decoders) {
      if (check_command(d.cmd, d.auxbin_p)) {
        l.push_back(d.encoding);
      }
    }
    while (l.size()) {
      auto p = l.back();
      l.pop_back();
      if (encodings.size()) {
        encodings += ", ";
      }
      encodings += p;
    }
  }
  return encodings.c_str();
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
      last_dot = &*p;
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

std::vector<uint8_t> decode_gzip(std::span<uint8_t> src) {
  assert(src[0] == 0x1f);
  assert(src[1] == 0x8b);
  assert(src[2] == 0x08);

  z_stream strm = {0};
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = 0;
  strm.next_in = Z_NULL;
  int ret = inflateInit2(&strm, 47);
  assert(ret == Z_OK);

  std::vector<uint8_t> buffer;

  strm.avail_in = src.size();
  strm.next_in = src.data();
  do {
    const int CHUNK = 16384;
    unsigned char out[CHUNK];
    strm.avail_out = CHUNK;
    strm.next_out = out;
    ret = inflate(&strm, Z_NO_FLUSH);
    assert(ret != Z_STREAM_ERROR); /* state not clobbered */
    switch (ret) {
    case Z_NEED_DICT:
      ret = Z_DATA_ERROR; /* and fall through */
    case Z_DATA_ERROR:
    case Z_MEM_ERROR:
      (void)inflateEnd(&strm);
      assert(false);
      return {};
    }
    auto have = CHUNK - strm.avail_out;
    auto before = buffer.size();
    buffer.resize(before + have);
    memcpy(buffer.data() + before, out, have);

  } while (strm.avail_out == 0);

  inflateEnd(&strm);

  return buffer;
}
