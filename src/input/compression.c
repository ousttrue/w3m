#include "input/compression.h"
#include "alloc.h"
#include "core.h"
#include "etc.h"
#include "file/tmpfile.h"
#include "input/istream.h"
#include "input/url_stream.h"
#include "text/Str.h"
#include "text/myctype.h"
#include "textlist.h"
#include "trap_jmp.h"
#include <string.h>
#include <sys/stat.h>

#define SAVE_BUF_SIZE 1536

#define GUNZIP_NAME "gunzip"
#define BUNZIP2_NAME "bunzip2"
#define INFLATE_NAME "inflate"
#define BROTLI_NAME "brotli"

#define GUNZIP_CMDNAME "gunzip"
#define BUNZIP2_CMDNAME "bunzip2"
#define INFLATE_CMDNAME "inflate"
#define BROTLI_CMDNAME "brotli"

static struct compression_decoder {
  int type;
  char *ext;
  const char *mime_type;
  int auxbin_p;
  char *cmd;
  char *name;
  char *encoding;
  char *encodings[4];
  int use_d_arg;
} compression_decoders[] = {
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
    {CMP_NOCOMPRESS,
     nullptr,
     nullptr,
     0,
     nullptr,
     nullptr,
     nullptr,
     {nullptr},
     0},
};

void check_compression(const char *path, struct URLFile *uf) {
  if (path == nullptr)
    return;

  int len = strlen(path);
  uf->compression = CMP_NOCOMPRESS;
  for (auto d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    int elen;
    if (d->ext == nullptr)
      continue;
    elen = strlen(d->ext);
    if (len > elen && strcasecmp(&path[len - elen], d->ext) == 0) {
      uf->compression = d->type;
      uf->guess_type = d->mime_type;
      break;
    }
  }
}

const char *compress_application_type(enum COMPRESSION_TYPE compression) {
  for (auto d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    if (d->type == compression)
      return d->mime_type;
  }
  return nullptr;
}

const char *filename_extension(const char *path, bool is_url) {
  const char *last_dot = "";
  const char *p = path;
  int i;

  if (path == NULL)
    return last_dot;
  if (*p == '.')
    p++;
  for (; *p; p++) {
    if (*p == '.') {
      last_dot = p;
    } else if (is_url && *p == '?')
      break;
  }
  if (*last_dot == '.') {
    for (i = 1; i < 8 && last_dot[i]; i++) {
      if (is_url && !IS_ALNUM(last_dot[i]))
        break;
    }
    return allocStr(last_dot, i);
  } else
    return last_dot;
}

const char *uncompressed_file_type(const char *path, const char **ext) {
  int len, slen;
  Str fn;
  struct compression_decoder *d;

  if (path == NULL)
    return NULL;

  slen = 0;
  len = strlen(path);
  for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    if (d->ext == NULL)
      continue;
    slen = strlen(d->ext);
    if (len > slen && strcasecmp(&path[len - slen], d->ext) == 0)
      break;
  }
  if (d->type == CMP_NOCOMPRESS)
    return NULL;

  fn = Strnew_charp(path);
  Strshrink(fn, slen);
  if (ext)
    *ext = filename_extension(fn->ptr, 0);
  auto t0 = guessContentType(fn->ptr);
  if (t0 == NULL)
    t0 = "text/plain";
  return t0;
}

#define S_IXANY (S_IXUSR | S_IXGRP | S_IXOTH)

#ifdef _WIN32
#define PATH_SEPARATOR ';'
#else
#define PATH_SEPARATOR ':'
#endif

static bool check_command(const char *cmd, int auxbin_p) {
  static char *path = NULL;
  Str dirs;
  char *p, *np;
  Str pathname;
  struct stat st;

  if (path == NULL)
    path = getenv("PATH");
  if (auxbin_p)
    dirs = Strnew_charp(w3m_auxbin_dir());
  else
    dirs = Strnew_charp(path);
  for (p = dirs->ptr; p != NULL; p = np) {
    np = strchr(p, PATH_SEPARATOR);
    if (np)
      *np++ = '\0';
    pathname = Strnew();
    Strcat_charp(pathname, p);
    Strcat_char(pathname, '/');
    Strcat_charp(pathname, cmd);
    if (stat(pathname->ptr, &st) == 0 && S_ISREG(st.st_mode) &&
        (st.st_mode & S_IXANY) != 0)
      return 1;
  }
  return 0;
}
char *acceptableEncoding() {
  static Str encodings = nullptr;
  if (encodings != nullptr)
    return encodings->ptr;

  struct TextList *l = newTextList();
  for (auto d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    if (check_command(d->cmd, d->auxbin_p)) {
      pushText(l, d->encoding);
    }
  }
  encodings = Strnew();
  char *p;
  while ((p = popText(l)) != nullptr) {
    if (encodings->length)
      Strcat_charp(encodings, ", ");
    Strcat_charp(encodings, p);
  }
  return encodings->ptr;
}

enum COMPRESSION_TYPE compressionFromEncoding(const char *p) {
  auto compression = CMP_NOCOMPRESS;
  for (auto d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    char **e;
    for (e = d->encodings; *e != NULL; e++) {
      if (strncasecmp(p, *e, strlen(*e)) == 0) {
        compression = d->type;
        break;
      }
    }
    if (compression != CMP_NOCOMPRESS) {
      break;
    }
  }
  return compression;
}

static const char *auxbinFile(const char *base) {
  return expandPath(Strnew_m_charp(w3m_auxbin_dir(), "/", base, NULL)->ptr);
}

void uncompress_stream(struct URLFile *uf, const char **src) {

  if (IStype(uf->stream) != IST_ENCODED) {
    uf->stream = newEncodedStream(uf->stream, uf->encoding);
    uf->encoding = ENC_7BIT;
  }

  const char *expand_cmd = GUNZIP_CMDNAME;
  const char *expand_name = GUNZIP_NAME;
  const char *ext = NULL;
  int use_d_arg = 0;
  for (auto d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    if (uf->compression == d->type) {
      if (d->auxbin_p)
        expand_cmd = auxbinFile(d->cmd);
      else
        expand_cmd = d->cmd;
      expand_name = d->name;
      ext = d->ext;
      use_d_arg = d->use_d_arg;
      break;
    }
  }
  uf->compression = CMP_NOCOMPRESS;

  const char *tmpf = NULL;
  if (uf->scheme != SCM_LOCAL) {
    tmpf = tmpfname(TMPF_DFL, ext)->ptr;
  }

  /* child1 -- stdout|f1=uf -> parent */
  FILE *f1;
  auto pid1 = open_pipe_rw(&f1, NULL);
  if (pid1 < 0) {
    UFclose(uf);
    return;
  }
  if (pid1 == 0) {
    /* child */

    /* uf -> child2 -- stdout|stdin -> child1 */
    FILE *f2 = stdin;
    auto pid2 = open_pipe_rw(&f2, NULL);
    if (pid2 < 0) {
      UFclose(uf);
      exit(1);
    }
    if (pid2 == 0) {
      /* child2 */
      char *buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
      int count;
      FILE *f = NULL;

      setup_child(true, 2, UFfileno(uf));
      if (tmpf)
        f = fopen(tmpf, "wb");
      while ((count = ISread_n(uf->stream, buf, SAVE_BUF_SIZE)) > 0) {
        if (fwrite(buf, 1, count, stdout) != count)
          break;
        if (f && fwrite(buf, 1, count, f) != count)
          break;
      }
      UFclose(uf);
      if (f)
        fclose(f);
      xfree(buf);
      exit(0);
    }
    /* child1 */
    dup2(1, 2); /* stderr>&stdout */
    setup_child(true, -1, -1);
    if (use_d_arg)
      execlp(expand_cmd, expand_name, "-d", NULL);
    else
      execlp(expand_cmd, expand_name, NULL);
    exit(1);
  }
  if (tmpf) {
    if (src)
      *src = tmpf;
    else
      uf->scheme = SCM_LOCAL;
  }
  UFhalfclose(uf);
  uf->stream = newFileStream(f1, (void (*)())fclose);
}
