#include "local_cgi.h"
#include "quote.h"
#include "ioutil.h"
#include "http_request.h"
#include "form.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <sstream>

#define HAVE_DIRENT_H 1
#define DEV_NULL_PATH "/dev/null"
#define HAVE_LSTAT 1

std::string document_root;
std::string cgi_bin;

static long seed() {
  auto p = std::make_shared<char>();
  return (long)(uint64_t)p.get() + (long)time(NULL);
}

// #ifndef S_IFMT
// #define S_IFMT 0170000
// #endif /* not S_IFMT */
// #ifndef S_IFREG
// #define S_IFREG 0100000
// #endif /* not S_IFREG */

// #ifdef HAVE_READLINK
// #ifndef S_IFLNK
// #define S_IFLNK 0120000
// #endif /* not S_IFLNK */
// #ifndef S_ISLNK
// #define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
// #endif /* not S_ISLNK */
// #endif /* not HAVE_READLINK */

#if !(_SVID_SOURCE || _XOPEN_SOURCE)
// https://stackoverflow.com/questions/74274179/i-cant-use-drand48-and-srand48-in-c
// static double drand48(void) { return rand() / (RAND_MAX + 1.0); }
static long int lrand48(void) { return rand(); }
// static long int mrand48(void) {
//   return rand() > RAND_MAX / 2 ? rand() : -rand();
// }
static void srand48(long int seedval) { srand(seedval); }
#endif

#define HAVE_CHDIR 1
#define HAVE_PUTENV 1
#define HAVE_SETENV 1
#define HAVE_READLINK 1

static std::string Local_cookie;
static std::string Local_cookie_file;

static void writeLocalCookie(){
#ifdef _MSC_VER
#else
// if (Local_cookie_file.size()) {
//   return;
// }
// Local_cookie_file = App::instance().tmpfname(TMPF_COOKIE, {});
// set_environ("LOCAL_COOKIE_FILE", Local_cookie_file.c_str());
// auto f = fopen(Local_cookie_file.c_str(), "wb");
// if (!f) {
//   return;
// }
// localCookie();
// fwrite(Local_cookie->ptr, sizeof(char), Local_cookie->length, f);
// fclose(f);
// chmod(Local_cookie_file.c_str(), S_IRUSR | S_IWUSR);
#endif
}

// setup cookie for local CGI
std::string localCookie() {
  if (Local_cookie.size()) {
    return Local_cookie;
  }
  srand48(seed());
  std::stringstream ss;
  ss << lrand48() << "@" << ioutil::hostname();
  Local_cookie = ss.str();
  return Local_cookie;
}

static int check_local_cgi(const char *file, int status) {
// #ifdef _MSC_VER
#if 1
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

void set_environ(const std::string &var, const std::string &value) {
#ifdef _MSC_VER
#else
#ifdef HAVE_SETENV
  if (var.size() && value.size())
    setenv(var.c_str(), value.c_str(), 1);
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
#endif
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

static std::string checkPath(const char *fn, const char *path) {
  while (*path) {
    std::string tmp;
    auto p = strchr(path, ':');
    if (p) {
      tmp = expandPath(std::string(path, p - path));
    } else {
      tmp = expandPath(path);
    }
    if (tmp.back() != '/') {
      tmp += '/';
    }
    tmp += fn;

    struct stat st;
    if (stat(tmp.c_str(), &st) == 0) {
      return tmp;
    }

    if (!p) {
      break;
    }
    path = p + 1;
    while (*path == ':') {
      path++;
    }
  }
  return {};
}

enum CgiFileName {
  CGIFN_NORMAL = 0,
  CGIFN_LIBDIR = 1,
  CGIFN_CGIBIN = 2,
};

struct CgiFilename {
  CgiFileName status;
  std::string fn;
  std::string name;
  std::string path_info;

  static CgiFilename get(const char *uri) {
    CgiFilename ret{
        .fn = uri,
        .name = uri,
    };

    if (cgi_bin.size() && strncmp(uri, "/cgi-bin/", 9) == 0) {
      auto offset = 9;
      if (auto path_info = strchr(uri + offset, '/')) {
        ret.path_info = path_info;
        ret.name = std::string(uri, path_info - uri);
      }
      auto tmp = checkPath(uri + offset, cgi_bin.c_str());
      if (tmp.empty()) {
        ret.status = CGIFN_NORMAL;
        return ret;
      }

      ret.fn = tmp;
      ret.status = CGIFN_CGIBIN;
      return ret;
    }

    {
      std::string tmp = CGIBIN_DIR;
      if (auto e = getenv("W3M_LIB_DIR")) {
        tmp = e;
      };
      if (tmp.back() != '/') {
        tmp.push_back('/');
      }
      int offset;
      if (strncmp(uri, "/$LIB/", 6) == 0)
        offset = 6;
      else if (strncmp(uri, tmp.c_str(), tmp.size()) == 0)
        offset = tmp.size();
      else if (*uri == '/' && document_root.size()) {
        std::string tmp2 = document_root;
        if (tmp2.back() != '/') {
          tmp2.push_back('/');
        }
        tmp2 += (uri + 1);
        if (strncmp(tmp2.c_str(), tmp.c_str(), tmp.size()) != 0) {
          ret.status = CGIFN_NORMAL;
          return ret;
        }
        ret.name = tmp2;
        offset = tmp.size();
      } else {
        ret.status = CGIFN_NORMAL;
        return ret;
      }

      if (auto path_info = strchr(uri + offset, '/')) {
        ret.path_info = path_info;
        ret.name = std::string(uri, path_info - uri);
      }
      tmp += ret.name.c_str() + offset;
      ret.fn = tmp;
      ret.status = CGIFN_LIBDIR;
      return ret;
    }
  }
};

FILE *localcgi_post(const char *uri, const char *qstr,
                    const std::shared_ptr<Form> &request,
                    const HttpOption &option) {
// #ifdef _MSC_VER
#if 1
  return {};
#else
  FILE *fr = NULL, *fw = NULL;
  int status;
  int pid;
  const char *file = uri, *name = uri, *path_info = NULL;
#ifdef HAVE_CHDIR
  const char *cgi_dir;
#endif
  const char *cgi_basename;

  status = cgi_filename(uri, &file, &name, &path_info);
  if (check_local_cgi(file, status) < 0)
    return NULL;
  writeLocalCookie();
  std::string tmpf;
  if (request && request->enctype != FORM_ENCTYPE_MULTIPART) {
    tmpf = TmpFile::instance().tmpfname(TMPF_DFL, {});
    fw = fopen(tmpf.c_str(), "w");
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
  if (option.referer.size() && !option.no_referer)
    set_environ("HTTP_REFERER", option.referer.c_str());
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
      freopen(tmpf.c_str(), "r", stdin);
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
#endif
}
