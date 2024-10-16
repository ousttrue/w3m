#include "input/localcgi.h"
#include "rc.h"
#include "alloc.h"
#include "core.h"
#include "etc.h"
#include "file/file.h"
#include "file/tmpfile.h"
#include "fm.h"
#include "html/form.h"
#include "html/html_text.h"
#include "input/http_request.h"
#include "input/url.h"
#include "rand48.h"
#include "term/termsize.h"
#include "trap_jmp.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static char *Local_cookie_file = NULL;
static Str Local_cookie = NULL;

static void writeLocalCookie() {
  if (no_rc_dir)
    return;

  if (Local_cookie_file)
    return;

  Local_cookie_file = tmpfname(TMPF_COOKIE, NULL)->ptr;
  set_environ("LOCAL_COOKIE_FILE", Local_cookie_file);
  auto f = fopen(Local_cookie_file, "wb");
  if (!f)
    return;
  localCookie();
  fwrite(Local_cookie->ptr, sizeof(char), Local_cookie->length, f);
  fclose(f);
  chmod(Local_cookie_file, S_IRUSR | S_IWUSR);
}

static int strCmp(const void *s1, const void *s2) {
  return strcmp(*(const char **)s1, *(const char **)s2);
}

/* setup cookie for local CGI */
Str localCookie() {
  if (Local_cookie)
    return Local_cookie;
  srand48((size_t)New(char) + (long)time(NULL));
  Local_cookie =
      Sprintf("%ld@%s", lrand48(), HostName ? HostName : "localhost");
  return Local_cookie;
}

#define CGIFN_NORMAL 0
#define CGIFN_LIBDIR 1
#define CGIFN_CGIBIN 2

Str loadLocalDir(const char *dname) {
  Directory *dir;
  struct stat st;
  char *p;
  Str fbuf = Strnew();
#ifndef _WIN32
  struct stat lst;
#ifdef HAVE_READLINK
  char lbuf[1024];
#endif /* HAVE_READLINK */
#endif /* HAVE_LSTAT */
  int i, l, nrow = 0, n = 0, maxlen = 0;
  int nfile, nfile_max = 100;

#ifdef _WIN32
  WIN32_FIND_DATA FindFileData;
  auto hFind = FindFirstFile(dname, &FindFileData);
#else
  auto d = opendir(dname);
  if (d == NULL)
    return NULL;
#endif

  auto dirname = Strnew_charp(dname);
  if (Strlastchar(dirname) != '/')
    Strcat_char(dirname, '/');

  auto qdir = html_quote(dirname->ptr);
  /* FIXME: gettextize? */
  auto tmp = Strnew_m_charp("<HTML>\n<HEAD>\n<BASE HREF=\"file://",
                            html_quote(file_quote(dirname->ptr)),
                            "\">\n<TITLE>Directory list of ", qdir,
                            "</TITLE>\n</HEAD>\n<BODY>\n<H1>Directory list of ",
                            qdir, "</H1>\n", NULL);
  char **flist = New_N(char *, nfile_max);
  nfile = 0;
#ifdef _WIN32
  for (auto enable = true; enable;
       enable = FindNextFile(hFind, &FindFileData)) {
    auto name = FindFileData.cFileName;
#else
  for (auto dir = readdir(d); dir; dir = readdir(d)) {
    auto name = dir->d_name;
#endif
    flist[nfile++] = allocStr(name, -1);
    if (nfile == nfile_max) {
      nfile_max *= 2;
      flist = New_Reuse(char *, flist, nfile_max);
    }
    if (multicolList) {
      l = strlen(name);
      if (l > maxlen)
        maxlen = l;
      n++;
    }
  }
#ifdef _WIN32
  FindClose(hFind);
#else
  closedir(d);
#endif

  if (multicolList) {
    l = COLS / (maxlen + 2);
    if (!l)
      l = 1;
    nrow = (n + l - 1) / l;
    n = 1;
    Strcat_charp(tmp, "<TABLE CELLPADDING=0>\n<TR VALIGN=TOP>\n");
  }
  qsort((void *)flist, nfile, sizeof(char *), strCmp);
  for (i = 0; i < nfile; i++) {
    p = flist[i];
    if (strcmp(p, ".") == 0)
      continue;
    Strcopy(fbuf, dirname);
    if (Strlastchar(fbuf) != '/')
      Strcat_char(fbuf, '/');
    Strcat_charp(fbuf, p);
#ifndef WIN32
    if (lstat(fbuf->ptr, &lst) < 0)
      continue;
#endif /* HAVE_LSTAT */
    if (stat(fbuf->ptr, &st) < 0)
      continue;
    if (multicolList) {
      if (n == 1)
        Strcat_charp(tmp, "<TD><NOBR>");
    } else {
#ifndef _WIN32
      if (S_ISLNK(lst.st_mode))
        Strcat_charp(tmp, "[LINK] ");
      else
#endif /* HAVE_LSTAT */
        if (S_ISDIR(st.st_mode))
          Strcat_charp(tmp, "[DIR]&nbsp; ");
        else
          Strcat_charp(tmp, "[FILE] ");
    }
    Strcat_m_charp(tmp, "<A HREF=\"", html_quote(file_quote(p)), NULL);
    if (S_ISDIR(st.st_mode))
      Strcat_char(tmp, '/');
    Strcat_m_charp(tmp, "\">", p, NULL);
    if (S_ISDIR(st.st_mode))
      Strcat_char(tmp, '/');
    Strcat_charp(tmp, "</A>");
    if (multicolList) {
      if (n++ == nrow) {
        Strcat_charp(tmp, "</NOBR></TD>\n");
        n = 1;
      } else {
        Strcat_charp(tmp, "<BR>\n");
      }
    } else {
#ifndef _WIN32
      if (S_ISLNK(lst.st_mode)) {
        if ((l = readlink(fbuf->ptr, lbuf, sizeof(lbuf) - 1)) > 0) {
          lbuf[l] = '\0';
          Strcat_m_charp(tmp, " -> ", html_quote(conv_from_system(lbuf)), NULL);
          if (S_ISDIR(st.st_mode))
            Strcat_char(tmp, '/');
        }
      }
#endif /* HAVE_LSTAT && HAVE_READLINK */
      Strcat_charp(tmp, "<br>\n");
    }
  }
  if (multicolList) {
    Strcat_charp(tmp, "</TR>\n</TABLE>\n");
  }
  Strcat_charp(tmp, "</BODY>\n</HTML>\n");

  return tmp;
}

static Str checkPath(const char *fn, const char *path) {
  while (*path) {
    auto p = strchr(path, ':');
    auto tmp = Strnew_charp(expandPath(p ? allocStr(path, p - path) : path));
    if (Strlastchar(tmp) != '/')
      Strcat_char(tmp, '/');
    Strcat_charp(tmp, fn);
    struct stat st;
    if (stat(tmp->ptr, &st) == 0)
      return tmp;
    if (!p)
      break;
    path = p + 1;
    while (*path == ':')
      path++;
  }
  return NULL;
}

static int cgi_filename(const char *uri, const char **fn, const char **name,
                        const char **path_info) {
  *fn = uri;
  *name = uri;
  *path_info = NULL;
  int offset;
  if (cgi_bin != NULL && strncmp(uri, "/cgi-bin/", 9) == 0) {
    offset = 9;
    if ((*path_info = strchr(uri + offset, '/')))
      *name = allocStr(uri, *path_info - uri);
    auto tmp = checkPath(*name + offset, cgi_bin);
    if (tmp == NULL)
      return CGIFN_NORMAL;
    *fn = tmp->ptr;
    return CGIFN_CGIBIN;
  }

  auto tmp = Strnew_charp(w3m_lib_dir());
  if (Strlastchar(tmp) != '/')
    Strcat_char(tmp, '/');
  if (strncmp(uri, "/$LIB/", 6) == 0)
    offset = 6;
  else if (strncmp(uri, tmp->ptr, tmp->length) == 0)
    offset = tmp->length;
  else if (*uri == '/' && document_root != NULL) {
    Str tmp2 = Strnew_charp(document_root);
    if (Strlastchar(tmp2) != '/')
      Strcat_char(tmp2, '/');
    Strcat_charp(tmp2, uri + 1);
    if (strncmp(tmp2->ptr, tmp->ptr, tmp->length) != 0)
      return CGIFN_NORMAL;
    uri = tmp2->ptr;
    *name = uri;
    offset = tmp->length;
  } else
    return CGIFN_NORMAL;
  if ((*path_info = strchr(uri + offset, '/')))
    *name = allocStr(uri, *path_info - uri);
  Strcat_charp(tmp, *name + offset);
  *fn = tmp->ptr;
  return CGIFN_LIBDIR;
}

static int check_local_cgi(const char *file, int status) {
#ifdef _WIN32
  return 0;
#else
  struct stat st;
  if (status != CGIFN_LIBDIR && status != CGIFN_CGIBIN)
    return -1;
  if (stat(file, &st) < 0)
    return -1;
  if (S_ISDIR(st.st_mode))
    return -1;
  if ((st.st_uid == geteuid() && (st.st_mode & S_IXUSR)) ||
      (st.st_gid == getegid() && (st.st_mode & S_IXGRP)) ||
      (st.st_mode & S_IXOTH)) /* executable */
    return 0;
  return -1;
#endif
}

static void set_cgi_environ(char *name, char *fn, char *req_uri) {
  set_environ("SERVER_SOFTWARE", w3m_version);
  set_environ("SERVER_PROTOCOL", "HTTP/1.0");
  set_environ("SERVER_NAME", "localhost");
  set_environ("SERVER_PORT", "80"); /* dummy */
  set_environ("REMOTE_HOST", "localhost");
  set_environ("REMOTE_ADDR", "127.0.0.1");
  set_environ("GATEWAY_INTERFACE", "CGI/1.1");

  set_environ("SCRIPT_NAME", name);
  set_environ("SCRIPT_FILENAME", fn);
  set_environ("REQUEST_URI", req_uri);
}

FILE *localcgi_post(const char *uri, const char *qstr, struct FormList *request,
                    const char *referer) {
  auto file = uri;
  auto name = uri;
  const char *path_info = NULL;
  const char *tmpf = NULL;
  const char *cgi_dir;

  int status = cgi_filename(uri, &file, &name, &path_info);
  if (check_local_cgi(file, status) < 0)
    return NULL;
  writeLocalCookie();

  FILE *fr = NULL, *fw = NULL;
  if (request && request->enctype != FORM_ENCTYPE_MULTIPART) {
    tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    fw = fopen(tmpf, "w");
    if (!fw)
      return NULL;
  }
  if (qstr)
    uri = Strnew_m_charp(uri, "?", qstr, NULL)->ptr;
  cgi_dir = mydirname(file);
  auto cgi_basename = mybasename(file);
  auto pid = open_pipe_rw(&fr, NULL);
  /* Don't invoke gc after here, or the program might crash in some platforms */
  if (pid < 0) {
    if (fw)
      fclose(fw);
    return NULL;
  } else if (pid) {
    if (fw)
      fclose(fw);
    return fr;
  }
  setup_child(true, 2, fw ? fileno(fw) : -1);

  set_cgi_environ(name, file, uri);
  if (path_info)
    set_environ("PATH_INFO", path_info);
  if (referer && referer != NO_REFERER)
    set_environ("HTTP_REFERER", referer);
  if (request) {
    set_environ("REQUEST_METHOD", "POST");
    if (qstr)
      set_environ("QUERY_STRING", qstr);
    set_environ("CONTENT_LENGTH", Sprintf("%d", request->length)->ptr);
    if (request->enctype == FORM_ENCTYPE_MULTIPART) {
      set_environ(
          "CONTENT_TYPE",
          Sprintf("multipart/form-data; boundary=%s", request->boundary)->ptr);
      freopen(request->body, "r", stdin);
    } else {
      set_environ("CONTENT_TYPE", "application/x-www-form-urlencoded");
      fwrite(request->body, sizeof(char), request->length, fw);
      fclose(fw);
      freopen(tmpf, "r", stdin);
    }
  } else {
    set_environ("REQUEST_METHOD", "GET");
    set_environ("QUERY_STRING", qstr ? qstr : "");
    freopen(DEV_NULL_PATH, "r", stdin);
  }

#ifdef HAVE_CHDIR /* ifndef __EMX__ ? */
  if (chdir(cgi_dir) == -1) {
    fprintf(stderr, "failed to chdir to %s: %s\n", cgi_dir, strerror(errno));
    exit(1);
  }
#endif
  execl(file, cgi_basename, NULL);
  fprintf(stderr, "execl(\"%s\", \"%s\", NULL): %s\n", file, cgi_basename,
          strerror(errno));
  exit(1);
}

FILE *localcgi_get(const char *u, const char *q, const char *r) {
  return localcgi_post((u), (q), NULL, (r));
}

const char *tag_get_value(struct LocalCgiHtml *t, const char *arg) {
  for (; t; t = t->next) {
    if (!strcasecmp(t->arg, arg))
      return t->value;
  }
  return NULL;
}

bool tag_exists(struct LocalCgiHtml *t, const char *arg) {
  for (; t; t = t->next) {
    if (!strcasecmp(t->arg, arg))
      return 1;
  }
  return 0;
}

struct LocalCgiHtml *cgistr2tagarg(const char *cgistr) {
  struct LocalCgiHtml *t0 = nullptr;
  struct LocalCgiHtml *t = nullptr;
  do {
    t = New(struct LocalCgiHtml);
    t->next = t0;
    t0 = t;
    auto tag = Strnew();
    while (*cgistr && *cgistr != '=' && *cgistr != '&')
      Strcat_char(tag, *cgistr++);
    t->arg = Str_form_unquote(tag)->ptr;
    t->value = NULL;
    if (*cgistr == '\0')
      return t;
    else if (*cgistr == '=') {
      cgistr++;
      auto value = Strnew();
      while (*cgistr && *cgistr != '&')
        Strcat_char(value, *cgistr++);
      t->value = Str_form_unquote(value)->ptr;
    } else if (*cgistr == '&')
      cgistr++;
  } while (*cgistr);
  return t;
}
