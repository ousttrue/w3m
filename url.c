#include "url.h"
#include "app.h"
#include "text.h"
#include "etc.h"
#include "alloc.h"
#include "strcase.h"
#include "indep.h"
#include "myctype.h"
#include <string.h>

const char *HostName = nullptr;

struct {
  const char *cmdname;
  enum URL_SCHEME_TYPE cmd;
} schemetable[] = {
    {"http", SCM_HTTP},
    {"gopher", SCM_GOPHER},
    {"ftp", SCM_FTP},
    {"local", SCM_LOCAL},
    {"file", SCM_LOCAL},
    /*  {"exec", SCM_EXEC}, */
    {"nntp", SCM_NNTP},
    /*  {"nntp", SCM_NNTP_GROUP}, */
    {"news", SCM_NEWS},
    /*  {"news", SCM_NEWS_GROUP}, */
    {"data", SCM_DATA},
    {"mailto", SCM_MAILTO},
    {"https", SCM_HTTPS},
    {NULL, SCM_UNKNOWN},
};

const char *schemeNumToName(enum URL_SCHEME_TYPE scheme) {
  for (int i = 0; schemetable[i].cmdname != NULL; i++) {
    if (schemetable[i].cmd == scheme)
      return schemetable[i].cmdname;
  }
  return NULL;
}

int getURLScheme(const char **url) {
  const char *p = *url, *q;
  int i;
  int scheme = SCM_MISSING;

  while (*p && (IS_ALNUM(*p) || *p == '.' || *p == '+' || *p == '-'))
    p++;
  if (*p == ':') { /* scheme found */
    scheme = SCM_UNKNOWN;
    for (i = 0; (q = schemetable[i].cmdname) != NULL; i++) {
      int len = strlen(q);
      if (!strncasecmp(q, *url, len) && (*url)[len] == ':') {
        scheme = schemetable[i].cmd;
        *url = p + 1;
        break;
      }
    }
  }
  return scheme;
}

int is_localhost(const char *host) {
  if (!host || !strcasecmp(host, "localhost") || !strcmp(host, "127.0.0.1") ||
      (HostName && !strcasecmp(host, HostName)) || !strcmp(host, "[::1]"))
    return true;
  return false;
}

const char *file_to_url(const char *file) {
  char *drive = NULL;
  file = expandPath(file);
  if (IS_ALPHA(file[0]) && file[1] == ':') {
    drive = allocStr(file, 2);
    file += 2;
  } else if (file[0] != '/') {
    auto tmp = Strnew_charp(app_currentdir());
    if (Strlastchar(tmp) != '/')
      Strcat_char(tmp, '/');
    Strcat_charp(tmp, file);
    file = tmp->ptr;
  }
  auto tmp = Strnew_charp("file://");
  if (drive) {
    Strcat_charp(tmp, drive);
  }
  Strcat_charp(tmp, file_quote(cleanupName(file)));
  return tmp->ptr;
}

char *url_unquote_conv0(char *url) {
  Str tmp;
  tmp = Str_url_unquote(Strnew_charp(url), false, true);
  return tmp->ptr;
}

void parseURL2(const char *url, struct Url *pu, struct Url *current) {
  char *p;
  Str tmp;
  int relative_uri = false;

  parseURL(url, pu, current);
  if (pu->scheme == SCM_MAILTO)
    return;
  if (pu->scheme == SCM_DATA)
    return;
  if (pu->scheme == SCM_NEWS || pu->scheme == SCM_NEWS_GROUP) {
    if (pu->file && !strchr(pu->file, '@') &&
        (!(p = strchr(pu->file, '/')) || strchr(p + 1, '-') ||
         *(p + 1) == '\0'))
      pu->scheme = SCM_NEWS_GROUP;
    else
      pu->scheme = SCM_NEWS;
    return;
  }
  if (pu->scheme == SCM_NNTP || pu->scheme == SCM_NNTP_GROUP) {
    if (pu->file && *pu->file == '/')
      pu->file = allocStr(pu->file + 1, -1);
    if (pu->file && !strchr(pu->file, '@') &&
        (!(p = strchr(pu->file, '/')) || strchr(p + 1, '-') ||
         *(p + 1) == '\0'))
      pu->scheme = SCM_NNTP_GROUP;
    else
      pu->scheme = SCM_NNTP;
    if (current &&
        (current->scheme == SCM_NNTP || current->scheme == SCM_NNTP_GROUP)) {
      if (pu->host == NULL) {
        pu->host = current->host;
        pu->port = current->port;
      }
    }
    return;
  }
  if (pu->scheme == SCM_LOCAL) {
    auto q = expandName(file_unquote(pu->file));
    Str drive;
    if (IS_ALPHA(q[0]) && q[1] == ':') {
      drive = Strnew_charp_n(q, 2);
      Strcat_charp(drive, file_quote(q + 2));
      pu->file = drive->ptr;
    } else
      pu->file = file_quote(q);
  }

  if (current &&
      (pu->scheme == current->scheme ||
       (pu->scheme == SCM_FTP && current->scheme == SCM_FTPDIR) ||
       (pu->scheme == SCM_LOCAL && current->scheme == SCM_LOCAL_CGI)) &&
      pu->host == NULL) {
    /* Copy omitted element from the current URL */
    pu->user = current->user;
    pu->pass = current->pass;
    pu->host = current->host;
    pu->port = current->port;
    if (pu->file && *pu->file) {
      if (pu->file[0] != '/' &&
          !(pu->scheme == SCM_LOCAL && IS_ALPHA(pu->file[0]) &&
            pu->file[1] == ':')) {
        /* file is relative [process 1] */
        p = pu->file;
        if (current->file) {
          tmp = Strnew_charp(current->file);
          while (tmp->length > 0) {
            if (Strlastchar(tmp) == '/')
              break;
            Strshrink(tmp, 1);
          }
          Strcat_charp(tmp, p);
          pu->file = tmp->ptr;
          relative_uri = true;
        }
      }
    } else { /* scheme:[?query][#label] */
      pu->file = current->file;
      if (!pu->query)
        pu->query = current->query;
    }
    /* comment: query part need not to be completed
     * from the current URL. */
  }
  if (pu->file) {
    if (pu->scheme == SCM_LOCAL && pu->file[0] != '/' &&
        !(IS_ALPHA(pu->file[0]) && pu->file[1] == ':') &&
        strcmp(pu->file, "-")) {
      /* local file, relative path */
      tmp = Strnew_charp(app_currentdir());
      if (Strlastchar(tmp) != '/')
        Strcat_char(tmp, '/');
      Strcat_charp(tmp, file_unquote(pu->file));
      pu->file = file_quote(cleanupName(tmp->ptr));
    } else if (pu->scheme == SCM_HTTP || pu->scheme == SCM_HTTPS) {
      if (relative_uri) {
        /* In this case, pu->file is created by [process 1] above.
         * pu->file may contain relative path (for example,
         * "/foo/../bar/./baz.html"), cleanupName() must be applied.
         * When the entire abs_path is given, it still may contain
         * elements like `//', `..' or `.' in the pu->file. It is
         * server's responsibility to canonicalize such path.
         */
        pu->file = cleanupName(pu->file);
      }
    } else if (pu->file[0] == '/') {
      /*
       * this happens on the following conditions:
       * (1) ftp scheme (2) local, looks like absolute path.
       * In both case, there must be no side effect with
       * cleanupName(). (I hope so...)
       */
      pu->file = cleanupName(pu->file);
    }
    if (pu->scheme == SCM_LOCAL) {
#ifdef SUPPORT_NETBIOS_SHARE
      if (pu->host && !is_localhost(pu->host)) {
        Str tmp = Strnew_charp("//");
        Strcat_m_charp(tmp, pu->host, cleanupName(file_unquote(pu->file)),
                       NULL);
        pu->real_file = tmp->ptr;
      } else
#endif
        pu->real_file = cleanupName(file_unquote(pu->file));
    }
  }
}
