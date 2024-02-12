#include "compression.h"
#include "signal_util.h"
#include "etc.h"
#include "tmpfile.h"
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

#define GUNZIP_CMDNAME "gunzip"
#define BUNZIP2_CMDNAME "bunzip2"
#define INFLATE_CMDNAME "inflate"
#define BROTLI_CMDNAME "brotli"

#define GUNZIP_NAME "gunzip"
#define BUNZIP2_NAME "bunzip2"
#define INFLATE_NAME "inflate"
#define BROTLI_NAME "brotli"

/* *INDENT-OFF* */
static compression_decoder compression_decoders[] = {
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
    {CMP_NOCOMPRESS, {}, nullptr, 0, nullptr, nullptr, nullptr, {nullptr}, 0},
};

const char *compress_application_type(CompressionType compression) {
  for (auto d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    if (d->type == compression)
      return d->mime_type;
  }
  return {};
}

std::tuple<const char *, std::string> uncompressed_file_type(const char *path) {
  if (path == nullptr) {
    return {};
  }

  auto slen = 0;
  auto len = strlen(path);
  struct compression_decoder *d;
  for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    if (d->ext.empty())
      continue;
    slen = d->ext.size();
    if (len > slen && strcasecmp(&path[len - slen], d->ext.c_str()) == 0)
      break;
  }
  if (d->type == CMP_NOCOMPRESS) {
    return {};
  }

  auto fn = Strnew_charp(path);
  Strshrink(fn, slen);
  auto t0 = guessContentType(fn->ptr);
  if (t0 == nullptr) {
    t0 = "text/plain";
  }
  auto ext = filename_extension(fn->ptr, 0);
  if (!ext) {
    ext = "";
  }
  return {t0, ext};
}

std::string UrlStream::uncompress_stream() {
  if (this->stream->IStype() != IST_ENCODED) {
    this->stream = newEncodedStream(this->stream, this->encoding);
    this->encoding = ENC_7BIT;
  }

  const char *expand_cmd = GUNZIP_CMDNAME;
  const char *expand_name = GUNZIP_NAME;
  int use_d_arg = 0;
  for (auto d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    if (this->compression == d->type) {
      if (d->auxbin_p)
        expand_cmd = auxbinFile((char *)d->cmd);
      else
        expand_cmd = d->cmd;
      expand_name = d->name;
      ext = d->ext;
      use_d_arg = d->use_d_arg;
      break;
    }
  }
  this->compression = CMP_NOCOMPRESS;

  std::string tmpf;
  if (this->schema != SCM_LOCAL) {
    tmpf = tmpfname(TMPF_DFL, ext);
  }

  /* child1 -- stdout|f1=uf -> parent */
  FILE *f1;
  auto pid1 = open_pipe_rw(&f1, nullptr);
  if (pid1 < 0) {
    return {};
  }
  if (pid1 == 0) {
    /* child */
    pid_t pid2;
    FILE *f2 = stdin;

    /* uf -> child2 -- stdout|stdin -> child1 */
    pid2 = open_pipe_rw(&f2, nullptr);
    if (pid2 < 0) {
      exit(1);
    }
    if (pid2 == 0) {
      /* child2 */
      char *buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
      int count;
      FILE *f = nullptr;

      // int UrlStream::fileno() const { return this->stream->ISfileno(); }

      setup_child(true, 2, this->stream->ISfileno());
      if (tmpf.size()) {
        f = fopen(tmpf.c_str(), "wb");
      }
      while ((count = this->stream->ISread_n(buf, SAVE_BUF_SIZE)) > 0) {
        if (static_cast<int>(fwrite(buf, 1, count, stdout)) != count)
          break;
        if (f && static_cast<int>(fwrite(buf, 1, count, f)) != count)
          break;
      }
      if (f)
        fclose(f);
      xfree(buf);
      exit(0);
    }
    /* child1 */
    dup2(1, 2); /* stderr>&stdout */
    setup_child(true, -1, -1);
    if (use_d_arg)
      execlp(expand_cmd, expand_name, "-d", nullptr);
    else
      execlp(expand_cmd, expand_name, nullptr);
    exit(1);
  }
  this->stream = newFileStream(f1, fclose);
  if (tmpf.size()) {
    this->schema = SCM_LOCAL;
  }
  return tmpf;
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
  for (auto d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    if (check_command(d->cmd, d->auxbin_p)) {
      pushText(l, d->encoding);
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
  for (auto d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    for (auto e = d->encodings; *e != nullptr; e++) {
      if (strncasecmp(p, *e, strlen(*e)) == 0) {
        uf->compression = d->type;
        break;
      }
    }
    if (uf->compression != CMP_NOCOMPRESS)
      break;
  }
  uf->content_encoding = uf->compression;
}

void check_compression(const char *path, UrlStream *uf) {
  if (!path) {
    return;
  }
  auto len = strlen(path);
  uf->compression = CMP_NOCOMPRESS;
  for (auto d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    if (d->ext.empty()) {
      continue;
    }
    auto elen = d->ext.size();
    if (len > elen && strcasecmp(&path[len - elen], d->ext.c_str()) == 0) {
      uf->compression = d->type;
      uf->guess_type = d->mime_type;
      break;
    }
  }
}

const char *filename_extension(const char *path, int is_url) {
  const char *last_dot = "";
  if (path == nullptr) {
    return last_dot;
  }

  auto p = path;
  if (*p == '.') {
    p++;
  }

  for (; *p; p++) {
    if (*p == '.') {
      last_dot = p;
    } else if (is_url && *p == '?')
      break;
  }
  if (*last_dot == '.') {
    int i;
    for (i = 1; i < 8 && last_dot[i]; i++) {
      if (is_url && !IS_ALNUM(last_dot[i]))
        break;
    }
    return allocStr(last_dot, i);
  }
  return last_dot;
}
