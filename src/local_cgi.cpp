#include <sys/types.h>
#include <fcntl.h>

#include "local_cgi.h"
#include "http_response.h"
#include "terms.h"
#include "rc.h"
#include "w3m.h"
#include "tmpfile.h"
#include "http_request.h"
#include "etc.h"
#include "form.h"
#include "signal_util.h"
#include "alloc.h"
#include "proto.h"
#include "hash.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#define HAVE_DIRENT_H 1
#define DEV_NULL_PATH "/dev/null"
#define HAVE_LSTAT 1

char *cgi_bin = nullptr;

#define CGIFN_NORMAL 0
#define CGIFN_LIBDIR 1
#define CGIFN_CGIBIN 2
#define HAVE_CHDIR 1
#define HAVE_PUTENV 1
#define HAVE_SETENV 1
#define HAVE_READLINK 1

static Str *Local_cookie = NULL;
static char *Local_cookie_file = NULL;

static void writeLocalCookie() {
  FILE *f;

  if (Local_cookie_file)
    return;
  Local_cookie_file = tmpfname(TMPF_COOKIE, NULL)->ptr;
  set_environ("LOCAL_COOKIE_FILE", Local_cookie_file);
  f = fopen(Local_cookie_file, "wb");
  if (!f)
    return;
  localCookie();
  fwrite(Local_cookie->ptr, sizeof(char), Local_cookie->length, f);
  fclose(f);
  chmod(Local_cookie_file, S_IRUSR | S_IWUSR);
}

/* setup cookie for local CGI */
Str *localCookie() {
  if (Local_cookie)
    return Local_cookie;
  srand48((long)New(char) + (long)time(NULL));
  Local_cookie =
      Sprintf("%ld@%s", lrand48(), HostName ? HostName : "localhost");
  return Local_cookie;
}



static int check_local_cgi(const char *file, int status) {
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
}

void set_environ(const char *var, const char *value) {
#ifdef HAVE_SETENV
  if (var != NULL && value != NULL)
    setenv(var, value, 1);
#else /* not HAVE_SETENV */
  static Hash_sv *env_hash = NULL;
  Str *tmp = Strnew_m_charp(var, "=", value, NULL);

  if (env_hash == NULL)
    env_hash = newHash_sv(20);
  putHash_sv(env_hash, var, (void *)tmp->ptr);
#ifdef HAVE_PUTENV
  putenv(tmp->ptr);
#else  /* not HAVE_PUTENV */
  extern char **environ;
  char **ne;
  int i, l, el;
  char **e, **newenv;

  /* I have no setenv() nor putenv() */
  /* This part is taken from terms.c of skkfep */
  l = strlen(var);
  for (e = environ, i = 0; *e != NULL; e++, i++) {
    if (strncmp(e, var, l) == 0 && (*e)[l] == '=') {
      el = strlen(*e) - l - 1;
      if (el >= strlen(value)) {
        strcpy(*e + l + 1, value);
        return 0;
      } else {
        for (; *e != NULL; e++, i++) {
          *e = *(e + 1);
        }
        i--;
        break;
      }
    }
  }
  newenv = (char **)GC_malloc((i + 2) * sizeof(char *));
  if (newenv == NULL)
    return;
  for (e = environ, ne = newenv; *e != NULL; *(ne++) = *(e++))
    ;
  *(ne++) = tmp->ptr;
  *ne = NULL;
  environ = newenv;
#endif /* not HAVE_PUTENV */
#endif /* not HAVE_SETENV */
}

static void set_cgi_environ(const char *name, const char *fn,
                            const char *req_uri) {
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

static Str *checkPath(const char *fn, const char *path) {
  const char *p;
  Str *tmp;
  struct stat st;
  while (*path) {
    p = strchr(path, ':');
    tmp = Strnew_charp(expandPath(p ? allocStr(path, p - path) : (char *)path));
    if (Strlastchar(tmp) != '/')
      Strcat_char(tmp, '/');
    Strcat_charp(tmp, fn);
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
  Str *tmp;
  int offset;

  *fn = uri;
  *name = uri;
  *path_info = NULL;

  if (cgi_bin != NULL && strncmp(uri, "/cgi-bin/", 9) == 0) {
    offset = 9;
    if ((*path_info = strchr(uri + offset, '/')))
      *name = allocStr(uri, *path_info - uri);
    tmp = checkPath(*name + offset, cgi_bin);
    if (tmp == NULL)
      return CGIFN_NORMAL;
    *fn = tmp->ptr;
    return CGIFN_CGIBIN;
  }

  tmp = Strnew_charp(w3m_lib_dir());
  if (Strlastchar(tmp) != '/')
    Strcat_char(tmp, '/');
  if (strncmp(uri, "/$LIB/", 6) == 0)
    offset = 6;
  else if (strncmp(uri, tmp->ptr, tmp->length) == 0)
    offset = tmp->length;
  else if (*uri == '/' && document_root != NULL) {
    Str *tmp2 = Strnew_charp(document_root);
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

FILE *localcgi_post(const char *uri, const char *qstr, FormList *request,
                    const char *referer) {
  FILE *fr = NULL, *fw = NULL;
  int status;
  pid_t pid;
  const char *file = uri, *name = uri, *path_info = NULL, *tmpf = NULL;
#ifdef HAVE_CHDIR
  const char *cgi_dir;
#endif
  const char *cgi_basename;

  status = cgi_filename(uri, &file, &name, &path_info);
  if (check_local_cgi(file, status) < 0)
    return NULL;
  writeLocalCookie();
  if (request && request->enctype != FORM_ENCTYPE_MULTIPART) {
    tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    fw = fopen(tmpf, "w");
    if (!fw)
      return NULL;
  }
  if (qstr)
    uri = Strnew_m_charp(uri, "?", qstr, NULL)->ptr;
#ifdef HAVE_CHDIR
  cgi_dir = mydirname(file);
#endif
  cgi_basename = mybasename(file);
  pid = open_pipe_rw(&fr, NULL); /* open_pipe_rw() forks */
  /* Don't invoke gc after here, or the program might crash in some platforms */
  if (pid < 0) {
    if (fw)
      fclose(fw);
    return NULL;
  } else if (pid) {
    /* parent */
    if (fw)
      fclose(fw);
    return fr;
  }
  /* child */
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
  /*
   * Suppress compiler warning: function might return no value
   * This code is never reached.
   */
  return NULL;
}
