#include "file.h"
#include "symbol.h"
#include "document.h"
#include "loader.h"
#include "html_renderer.h"
#include "trap_jmp.h"
#include "file.h"
#include "line.h"
#include "os.h"
#include "http_response.h"
#include "istream.h"
#include "alloc.h"
#include "ctrlcode.h"
#include "url_stream.h"
#include "buffer.h"
#include "html_readbuffer.h"
#include "termsize.h"
#include "terms.h"
#include "myctype.h"
#include "localcgi.h"
#include "fm.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#define SHELLBUFFERNAME "*Shellout*"

#define SAVE_BUF_SIZE 1536

#define TAG_IS(s, tag, len)                                                    \
  (strncasecmp(s, tag, len) == 0 && (s[len] == '>' || IS_SPACE((int)s[len])))

static char *w3m_dir(const char *name, char *dft) {
#ifdef USE_PATH_ENVVAR
  char *value = getenv(name);
  return value ? value : dft;
#else
  return dft;
#endif
}

const char *w3m_auxbin_dir() { return w3m_dir("W3M_AUXBIN_DIR", AUXBIN_DIR); }

const char *w3m_lib_dir() {
  /* FIXME: use W3M_CGIBIN_DIR? */
  return w3m_dir("W3M_LIB_DIR", CGIBIN_DIR);
}

const char *w3m_etc_dir() { return w3m_dir("W3M_ETC_DIR", ETC_DIR); }

const char *w3m_conf_dir() { return w3m_dir("W3M_CONF_DIR", CONF_DIR); }

const char *w3m_help_dir() { return w3m_dir("W3M_HELP_DIR", HELP_DIR); }

const char *expandPath(const char *name) {
  if (!name) {
    return nullptr;
  }

  if (name[0] != '~') {
    return name;
  }

  auto p = name + 1;
  if (*p != '/' && *p != '\0') {
    return name;
  }

  /* ~/dir... or ~ */
  auto extpath = Strnew_charp(getenv("HOME"));
  if (Strcmp_charp(extpath, "/") == 0 && *p == '/')
    p++;
  Strcat_charp(extpath, p);
  return extpath->ptr;
}
int getMetaRefreshParam(const char *q, Str *refresh_uri) {
  int refresh_interval;
  // char *r;
  Str s_tmp = NULL;

  if (q == NULL || refresh_uri == NULL)
    return 0;

  refresh_interval = atoi(q);
  if (refresh_interval < 0)
    return 0;

  while (*q) {
    if (!strncasecmp(q, "url=", 4)) {
      q += 4;
      if (*q == '\"' || *q == '\'') /* " or ' */
        q++;
      auto r = q;
      while (*r && !IS_SPACE(*r) && *r != ';')
        r++;
      s_tmp = Strnew_charp_n(q, r - q);

      if (s_tmp->length > 0 &&
          (s_tmp->ptr[s_tmp->length - 1] == '\"' ||  /* " */
           s_tmp->ptr[s_tmp->length - 1] == '\'')) { /* ' */
        s_tmp->length--;
        s_tmp->ptr[s_tmp->length] = '\0';
      }
      q = r;
    }
    while (*q && *q != ';')
      q++;
    if (*q == ';')
      q++;
    while (*q && *q == ' ')
      q++;
  }
  *refresh_uri = s_tmp;
  return refresh_interval;
}

extern char *NullLine;
extern Lineprop NullProp[];

static Str conv_symbol(struct Line *l) {
  Str tmp = NULL;
  char *p = l->lineBuf, *ep = p + l->len;
  Lineprop *pr = l->propBuf;
  char **symbol = get_symbol();

  for (; p < ep; p++, pr++) {
    if (*pr & PC_SYMBOL) {
      char c = *p - SYMBOL_BASE;
      if (tmp == NULL) {
        tmp = Strnew_size(l->len);
        Strcopy_charp_n(tmp, l->lineBuf, p - l->lineBuf);
      }
      Strcat_charp(tmp, symbol[(unsigned char)c % N_SYMBOL]);
    } else if (tmp != NULL)
      Strcat_char(tmp, *p);
  }
  if (tmp)
    return tmp;
  else
    return Strnew_charp_n(l->lineBuf, l->len);
}

/*
 * saveBuffer: write buffer to file
 */
static void _saveBuffer(struct Buffer *buf, struct Line *l, FILE *f, int cont) {
  Str tmp;
  int is_html = false;

  is_html = is_html_type(buf->type);

  for (; l != NULL; l = l->next) {
    if (is_html)
      tmp = conv_symbol(l);
    else
      tmp = Strnew_charp_n(l->lineBuf, l->len);
    Strfputs(tmp, f);
    if (Strlastchar(tmp) != '\n' && !(cont && l->next && l->next->bpos))
      putc('\n', f);
  }
}

void saveBuffer(struct Buffer *buf, FILE *f, int cont) {
  _saveBuffer(buf, buf->document->firstLine, f, cont);
}

void saveBufferBody(struct Buffer *buf, FILE *f, int cont) {
  struct Line *l = buf->document->firstLine;

  while (l != NULL && l->real_linenumber == 0)
    l = l->next;
  _saveBuffer(buf, l, f, cont);
}

/*
 * getshell: execute shell command and get the result into a buffer
 */
struct Buffer *getshell(const char *cmd) {
  auto buf = loadcmdout(cmd, loadBuffer, NULL);
  if (buf == NULL)
    return NULL;
  buf->filename = cmd;
  buf->buffername = Sprintf("%s %s", SHELLBUFFERNAME, cmd)->ptr;
  return buf;
}

int save2tmp(struct URLFile uf, char *tmpf) {
  int check;
  int64_t linelen = 0, trbyte = 0;
  int retval = 0;
  char *buf = NULL;

  auto ff = fopen(tmpf, "wb");
  if (ff == NULL) {
    /* fclose(f); */
    return -1;
  }

  if (from_jmp()) {
    goto _end;
  }
  trap_on();

  check = 0;
  {
    int count;

    buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
    while ((count = ISread_n(uf.stream, buf, SAVE_BUF_SIZE)) > 0) {
      if (fwrite(buf, 1, count, ff) != count) {
        retval = -2;
        goto _end;
      }
      linelen += count;
      term_showProgress(&linelen, &trbyte, uf.current_content_length);
    }
  }

_end:
  trap_off();
  xfree(buf);
  fclose(ff);
  return retval;
}

int checkOverWrite(const char *path) {
  struct stat st;
  if (stat(path, &st) < 0)
    return 0;

  auto ans = term_inputAnswer("File exists. Overwrite? (y/n)");
  if (ans && TOLOWER(*ans) == 'y')
    return 0;
  else
    return -1;
}

int _MoveFile(char *path1, char *path2) {
  auto f1 = openIS(path1);
  if (!f1)
    return -1;

  FILE *f2;
  bool is_pipe;
  if (*path2 == '|' && PermitSaveToPipe) {
    is_pipe = true;
    f2 = popen(path2 + 1, "w");
  } else {
    is_pipe = false;
    f2 = fopen(path2, "wb");
  }
  if (!f2) {
    ISclose(f1);
    return -1;
  }

  int64_t linelen = 0;
  int64_t trbyte = 0;
  int count;
  uint64_t current_content_length = 0;
  auto buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
  while ((count = ISread_n(f1, buf, SAVE_BUF_SIZE)) > 0) {
    fwrite(buf, 1, count, f2);
    linelen += count;
    term_showProgress(&linelen, &trbyte, current_content_length);
  }

  xfree(buf);
  ISclose(f1);
  if (is_pipe)
    pclose(f2);
  else
    fclose(f2);
  return 0;
}

const char *etcFile(const char *base) {
  return expandPath(Strnew_m_charp(w3m_etc_dir(), "/", base, NULL)->ptr);
}

const char *confFile(const char *base) {
  return expandPath(Strnew_m_charp(w3m_conf_dir(), "/", base, NULL)->ptr);
}

#define IS_DIRECTORY(m) (((m) & S_IFMT) == S_IFDIR)

const char *lastFileName(const char *path) {
  const char *p, *q;

  p = q = path;
  while (*p != '\0') {
    if (*p == '/')
      q = p + 1;
    p++;
  }

  return allocStr(q, -1);
}

const char *mybasename(const char *s) {
  const char *p = s;
  while (*p)
    p++;
  while (s <= p && *p != '/')
    p--;
  if (*p == '/')
    p++;
  else
    p = s;
  return allocStr(p, -1);
}

const char *mydirname(const char *s) {
  const char *p = s;
  while (*p)
    p++;
  if (s != p)
    p--;
  while (s != p && *p == '/')
    p--;
  while (s != p && *p != '/')
    p--;
  if (*p != '/')
    return ".";
  while (s != p && *p == '/')
    p--;
  return allocStr(s, strlen(s) - strlen(p) + 1);
}

#define DEF_SAVE_FILE "index.html"

char *guess_filename(const char *file) {
  char *p = NULL;
  if (file != NULL)
    p = allocStr(mybasename(file), -1);
  if (p == NULL || *p == '\0')
    return DEF_SAVE_FILE;

  auto s = p;
  if (*p == '#')
    p++;
  while (*p != '\0') {
    if ((*p == '#' && *(p + 1) != '\0') || *p == '?') {
      *p = '\0';
      break;
    }
    p++;
  }
  return s;
}

bool dir_exist(const char *path) {
  if (path == NULL || *path == '\0')
    return false;
  struct stat stbuf;
  if (stat(path, &stbuf) == -1)
    return false;
  return IS_DIRECTORY(stbuf.st_mode);
}

const char *currentdir() {
  char *path;
#ifdef HAVE_GETCWD
#ifdef MAXPATHLEN
  path = NewAtom_N(char, MAXPATHLEN);
  getcwd(path, MAXPATHLEN);
#else
  path = getcwd(NULL, 0);
#endif
#else /* not HAVE_GETCWD */
#ifdef HAVE_GETWD
  path = NewAtom_N(char, 1024);
  getwd(path);
#else  /* not HAVE_GETWD */
  FILE *f;
  char *p;
  path = NewAtom_N(char, 1024);
  f = popen("pwd", "r");
  fgets(path, 1024, f);
  pclose(f);
  for (p = path; *p; p++)
    if (*p == '\n') {
      *p = '\0';
      break;
    }
#endif /* not HAVE_GETWD */
#endif /* not HAVE_GETCWD */
  return path;
}
