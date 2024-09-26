#include "fm.h"
#include "buffer.h"
#include "http_request.h"
#include "termsize.h"
#include "terms.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#ifdef HAVE_READLINK
#include <unistd.h>
#endif /* HAVE_READLINK */
#include "localcgi.h"
#include "hash.h"

static int check_local_cgi(char *file, int status) {
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
  if (var != NULL && value != NULL) {
    setenv(var, value, 1);
  }
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

static Str checkPath(char *fn, char *path) {
  char *p;
  Str tmp;
  struct stat st;
  while (*path) {
    p = strchr(path, ':');
    tmp = Strnew_charp(expandPath(p ? allocStr(path, p - path) : path));
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

static int cgi_filename(char *uri, char **fn, char **name, char **path_info) {
  Str tmp;
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

FILE *localcgi_post(char *uri, char *qstr, struct FormList *request,
                    char *referer) {
  FILE *fr = NULL, *fw = NULL;
  int status;
  pid_t pid;
  char *file = uri, *name = uri, *path_info = NULL, *tmpf = NULL;
#ifdef HAVE_CHDIR
  char *cgi_dir;
#endif
  char *cgi_basename;

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
  pid = open_pipe_rw(&fr, NULL);
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
  setup_child(TRUE, 2, fw ? fileno(fw) : -1);

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
