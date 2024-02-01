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
// #include <unistd.h>
// #include <sys/types.h>
#include <sys/stat.h>
// #include <fcntl.h>
// #include <stdlib.h>
#include "indep.h"

#define GUNZIP_CMDNAME "gunzip"
#define BUNZIP2_CMDNAME "bunzip2"
#define INFLATE_CMDNAME "inflate"
#define BROTLI_CMDNAME "brotli"

#define GUNZIP_NAME "gunzip"
#define BUNZIP2_NAME "bunzip2"
#define INFLATE_NAME "inflate"
#define BROTLI_NAME "brotli"

/* *INDENT-OFF* */
static struct compression_decoder {
  int type;
  const char *ext;
  const char *mime_type;
  int auxbin_p;
  const char *cmd;
  const char *name;
  const char *encoding;
  const char *encodings[4];
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

const char *compress_application_type(int compression) {
  struct compression_decoder *d;

  for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    if (d->type == compression)
      return d->mime_type;
  }
  return {};
}

const char *uncompressed_file_type(const char *path, const char **ext) {
  int len, slen;
  Str *fn;
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

void uncompress_stream(UrlStream *uf, const char **src) {
  pid_t pid1;
  FILE *f1;
  const char *expand_cmd = GUNZIP_CMDNAME;
  const char *expand_name = GUNZIP_NAME;
  char *tmpf = NULL;
  char *ext = NULL;
  struct compression_decoder *d;
  int use_d_arg = 0;

  if (IStype(uf->stream) != IST_ENCODED) {
    uf->stream = newEncodedStream(uf->stream, uf->encoding);
    uf->encoding = ENC_7BIT;
  }
  for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    if (uf->compression == d->type) {
      if (d->auxbin_p)
        expand_cmd = auxbinFile((char *)d->cmd);
      else
        expand_cmd = d->cmd;
      expand_name = d->name;
      ext = (char *)d->ext;
      use_d_arg = d->use_d_arg;
      break;
    }
  }
  uf->compression = CMP_NOCOMPRESS;

  if (uf->schema != SCM_LOCAL) {
    tmpf = tmpfname(TMPF_DFL, ext)->ptr;
  }

  /* child1 -- stdout|f1=uf -> parent */
  pid1 = open_pipe_rw(&f1, NULL);
  if (pid1 < 0) {
    UFclose(uf);
    return;
  }
  if (pid1 == 0) {
    /* child */
    pid_t pid2;
    FILE *f2 = stdin;

    /* uf -> child2 -- stdout|stdin -> child1 */
    pid2 = open_pipe_rw(&f2, NULL);
    if (pid2 < 0) {
      UFclose(uf);
      exit(1);
    }
    if (pid2 == 0) {
      /* child2 */
      char *buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
      int count;
      FILE *f = NULL;

      setup_child(TRUE, 2, UFfileno(uf));
      if (tmpf)
        f = fopen(tmpf, "wb");
      while ((count = ISread_n(uf->stream, buf, SAVE_BUF_SIZE)) > 0) {
        if (static_cast<int>(fwrite(buf, 1, count, stdout)) != count)
          break;
        if (f && static_cast<int>(fwrite(buf, 1, count, f)) != count)
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
    setup_child(TRUE, -1, -1);
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
      uf->schema = SCM_LOCAL;
  }
  UFhalfclose(uf);
  uf->stream = newFileStream(f1, (void (*)())fclose);
}

#define S_IXANY (S_IXUSR | S_IXGRP | S_IXOTH)

static int check_command(const char *cmd, int auxbin_p) {
  static char *path = NULL;
  Str *dirs;
  char *p, *np;
  Str *pathname;
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

char *acceptableEncoding(void) {
  static Str *encodings = NULL;
  struct compression_decoder *d;
  TextList *l;
  char *p;

  if (encodings != NULL)
    return encodings->ptr;
  l = newTextList();
  for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    if (check_command(d->cmd, d->auxbin_p)) {
      pushText(l, d->encoding);
    }
  }
  encodings = Strnew();
  while ((p = popText(l)) != NULL) {
    if (encodings->length)
      Strcat_charp(encodings, ", ");
    Strcat_charp(encodings, p);
  }
  return encodings->ptr;
}

void process_compression(Str *lineBuf2, UrlStream *uf) {
  struct compression_decoder *d;
  auto p = lineBuf2->ptr + 17;
  while (IS_SPACE(*p))
    p++;
  uf->compression = CMP_NOCOMPRESS;
  for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    for (auto e = d->encodings; *e != NULL; e++) {
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
  int len;
  struct compression_decoder *d;

  if (path == NULL)
    return;

  len = strlen(path);
  uf->compression = CMP_NOCOMPRESS;
  for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    int elen;
    if (d->ext == NULL)
      continue;
    elen = strlen(d->ext);
    if (len > elen && strcasecmp(&path[len - elen], d->ext) == 0) {
      uf->compression = d->type;
      uf->guess_type = d->mime_type;
      break;
    }
  }
}

const char *filename_extension(const char *path, int is_url) {
  const char *last_dot = "", *p = path;
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
