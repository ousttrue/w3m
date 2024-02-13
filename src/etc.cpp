#include "etc.h"
#include "app.h"
#include "url_stream.h"
#include "buffer.h"
#include "line.h"
#include "message.h"
#include "http_response.h"
#include "screen.h"
#include "terms.h"
#include "display.h"
#include "readbuffer.h"
#include "w3m.h"
#include "buffer.h"
#include "signal_util.h"
#include "myctype.h"
#include "local_cgi.h"
#include "textlist.h"
#include "proto.h"
#include <pwd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#define HAVE_STRERROR 1

char *personal_document_root = nullptr;

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

#ifdef USE_INCLUDED_SRAND48
static unsigned long R1 = 0x1234abcd;
static unsigned long R2 = 0x330e;
#define A1 0x5deec
#define A2 0xe66d
#define C 0xb

void srand48(long seed) {
  R1 = (unsigned long)seed;
  R2 = 0x330e;
}

long lrand48(void) {
  R1 = (A1 * R1 << 16) + A1 * R2 + A2 * R1 + ((A2 * R2 + C) >> 16);
  R2 = (A2 * R2 + C) & 0xffff;
  return (long)(R1 >> 1);
}
#endif

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

#ifndef HAVE_STRERROR
char *strerror(int errno) {
  extern char *sys_errlist[];
  return sys_errlist[errno];
}
#endif /* not HAVE_STRERROR */

/* get last modified time */
const char *last_modified(const std::shared_ptr<Buffer> &buf) {
  TextListItem *ti;
  struct stat st;

  if (buf->res->document_header) {
    for (ti = buf->res->document_header->first; ti; ti = ti->next) {
      if (strncasecmp(ti->ptr, "Last-modified: ", 15) == 0) {
        return ti->ptr + 15;
      }
    }
    return "unknown";
  } else if (buf->res->currentURL.schema == SCM_LOCAL) {
    if (stat(buf->res->currentURL.file.c_str(), &st) < 0)
      return "unknown";
    return ctime(&st.st_mtime);
  }
  return "unknown";
}

#ifndef SIGIOT
#define SIGIOT SIGABRT
#endif /* not SIGIOT */

pid_t open_pipe_rw(FILE **fr, FILE **fw) {
  int fdr[2];
  int fdw[2];
  pid_t pid;

  if (fr && pipe(fdr) < 0)
    goto err0;
  if (fw && pipe(fdw) < 0)
    goto err1;

  flush_tty();
  pid = fork();
  if (pid < 0)
    goto err2;
  if (pid == 0) {
    /* child */
    if (fr) {
      close(fdr[0]);
      dup2(fdr[1], 1);
    }
    if (fw) {
      close(fdw[1]);
      dup2(fdw[0], 0);
    }
  } else {
    if (fr) {
      close(fdr[1]);
      if (*fr == stdin)
        dup2(fdr[0], 0);
      else
        *fr = fdopen(fdr[0], "r");
    }
    if (fw) {
      close(fdw[0]);
      if (*fw == stdout)
        dup2(fdw[1], 1);
      else
        *fw = fdopen(fdw[1], "w");
    }
  }
  return pid;
err2:
  if (fw) {
    close(fdw[0]);
    close(fdw[1]);
  }
err1:
  if (fr) {
    close(fdr[0]);
    close(fdr[1]);
  }
err0:
  return (pid_t)-1;
}

void mySystem(const char *command, int background) {
  if (background) {
    flush_tty();
    if (!fork()) {
      setup_child(false, 0, -1);
      myExec(command);
    }
  } else
    system(command);
}

Str *myExtCommand(const char *cmd, const char *arg, int redirect) {
  Str *tmp = NULL;
  const char *p;
  int set_arg = false;

  for (p = cmd; *p; p++) {
    if (*p == '%' && *(p + 1) == 's' && !set_arg) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(cmd, (int)(p - cmd));
      Strcat_charp(tmp, arg);
      set_arg = true;
      p++;
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (!set_arg) {
    if (redirect)
      tmp = Strnew_m_charp("(", cmd, ") < ", arg, NULL);
    else
      tmp = Strnew_m_charp(cmd, " ", arg, NULL);
  }
  return tmp;
}

const char *expandPath(const char *name) {
  const char *p;
  struct passwd *passent, *getpwnam(const char *);
  Str *extpath = NULL;

  if (name == NULL)
    return NULL;
  p = name;
  if (*p == '~') {
    p++;
    if (IS_ALPHA(*p)) {
      auto q = strchr(p, '/');
      if (q) { /* ~user/dir... */
        passent = getpwnam(allocStr(p, q - p));
        p = q;
      } else { /* ~user */
        passent = getpwnam(p);
        p = "";
      }
      if (!passent)
        goto rest;
      extpath = Strnew_charp(passent->pw_dir);
    } else if (*p == '/' || *p == '\0') { /* ~/dir... or ~ */
      extpath = Strnew_charp(getenv("HOME"));
    } else
      goto rest;
    if (Strcmp_charp(extpath, "/") == 0 && *p == '/')
      p++;
    Strcat_charp(extpath, p);
    return extpath->ptr;
  }
rest:
  return name;
}
const char *expandName(const char *name) {
  const char *p;
  struct passwd *passent, *getpwnam(const char *);
  Str *extpath = NULL;

  if (name == NULL)
    return NULL;
  p = name;
  if (*p == '/') {
    if ((*(p + 1) == '~' && IS_ALPHA(*(p + 2))) && personal_document_root) {
      p += 2;
      auto q = strchr(p, '/');
      if (q) { /* /~user/dir... */
        passent = getpwnam(allocStr(p, q - p));
        p = q;
      } else { /* /~user */
        passent = getpwnam(p);
        p = "";
      }
      if (!passent)
        goto rest;
      extpath =
          Strnew_m_charp(passent->pw_dir, "/", personal_document_root, NULL);
      if (*personal_document_root == '\0' && *p == '/')
        p++;
    } else
      goto rest;
    if (Strcmp_charp(extpath, "/") == 0 && *p == '/')
      p++;
    Strcat_charp(extpath, p);
    return extpath->ptr;
  } else
    return expandPath(p);
rest:
  return name;
}

const char *file_to_url(const char *file) {
  Str *tmp;
#ifdef SUPPORT_DOS_DRIVE_PREFIX
  char *drive = NULL;
#endif
#ifdef SUPPORT_NETBIOS_SHARE
  char *host = NULL;
#endif

  if (!(file = expandPath(file)))
    return NULL;
#ifdef SUPPORT_NETBIOS_SHARE
  if (file[0] == '/' && file[1] == '/') {
    char *p;
    file += 2;
    if (*file) {
      p = strchr(file, '/');
      if (p != NULL && p != file) {
        host = allocStr(file, (p - file));
        file = p;
      }
    }
  }
#endif
#ifdef SUPPORT_DOS_DRIVE_PREFIX
  if (IS_ALPHA(file[0]) && file[1] == ':') {
    drive = allocStr(file, 2);
    file += 2;
  } else
#endif
      if (file[0] != '/') {
    tmp = Strnew(App::instance().pwd());
    if (Strlastchar(tmp) != '/')
      Strcat_char(tmp, '/');
    Strcat_charp(tmp, file);
    file = tmp->ptr;
  }
  tmp = Strnew_charp("file://");
#ifdef SUPPORT_NETBIOS_SHARE
  if (host)
    Strcat_charp(tmp, host);
#endif
#ifdef SUPPORT_DOS_DRIVE_PREFIX
  if (drive)
    Strcat_charp(tmp, drive);
#endif
  Strcat_charp(tmp, file_quote(cleanupName((char *)file)));
  return tmp->ptr;
}
