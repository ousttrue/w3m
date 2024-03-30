#include "etc.h"
#include "ioutil.h"
#include "quote.h"
#include "file_util.h"
#include "ctrlcode.h"
#include "app.h"
#include "myctype.h"
#include "local_cgi.h"
#include "proc.h"
#include "Str.h"
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>
#define HAVE_STRERROR 1

bool IsForkChild = 0;

#ifdef _MSC_VER
#include <direct.h>
#define mkdir _mkdir;
#else
#include <pwd.h>
#endif

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

#ifndef SIGIOT
#define SIGIOT SIGABRT
#endif /* not SIGIOT */

int open_pipe_rw(FILE **fr, FILE **fw) {
#ifdef _MSC_VER
  return {};
#else
  int fdr[2];
  int fdw[2];
  int pid;
  if (fr && pipe(fdr) < 0)
    goto err0;
  if (fw && pipe(fdw) < 0)
    goto err1;

  // flush_tty();
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
#endif
}

std::string file_to_url(std::string file) {
  Str *tmp;
#ifdef SUPPORT_DOS_DRIVE_PREFIX
  char *drive = NULL;
#endif
#ifdef SUPPORT_NETBIOS_SHARE
  char *host = NULL;
#endif
  file = expandPath(file);
  if (!file.empty())
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
    tmp = Strnew(ioutil::pwd());
    if (Strlastchar(tmp) != '/')
      Strcat_char(tmp, '/');
    Strcat(tmp, file);
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
  Strcat(tmp, ioutil::file_quote(cleanupName(file)));
  return tmp->ptr;
}

int do_recursive_mkdir(const char *dir) {
#ifdef _MSC_VER
  return {};
#else
  const char *ch, *dircpy;
  char tmp;
  struct stat st;

  if (*dir == '\0')
    return -1;

  dircpy = Strnew_charp(dir)->ptr;
  ch = dircpy + 1;
  do {
    while (!(*ch == '/' || *ch == '\0')) {
      ch++;
    }

    tmp = *ch;
    *(char *)ch = '\0';

    if (stat(dircpy, &st) < 0) {
      if (errno != ENOENT) { /* no directory */
        return -1;
      }
      if (mkdir(dircpy, 0700) < 0) {
        return -1;
      }
      stat(dircpy, &st);
    }
    if (!S_ISDIR(st.st_mode)) {
      /* not a directory */
      return -1;
    }
    if (!(st.st_mode & S_IWUSR)) {
      return -1;
    }

    *(char *)ch = tmp;

  } while (*ch++ != '\0');
#ifdef HAVE_FACCESSAT
  if (faccessat(AT_FDCWD, dir, W_OK | X_OK, AT_EACCESS) < 0) {
    return -1;
  }
#endif

  return 0;
#endif
}

int exec_cmd(const std::string &cmd) {
  App::instance().endRawMode();
  if (auto rv = system(cmd.c_str())) {
    printf("\n[Hit any key]");
    fflush(stdout);
    App::instance().beginRawMode();
    // getch();
    return rv;
  }
  App::instance().endRawMode();
  return 0;
}

Str *unescape_spaces(Str *s) {
  Str *tmp = NULL;
  char *p;

  if (s == NULL)
    return s;
  for (p = s->ptr; *p; p++) {
    if (*p == '\\' && (*(p + 1) == ' ' || *(p + 1) == CTRL_I)) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(s->ptr, (int)(p - s->ptr));
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (tmp)
    return tmp;
  return s;
}

static void close_all_fds_except(int i, int f) {
#ifdef _MSC_VER
#else
  switch (i) { /* fall through */
  case 0:
    dup2(open("/dev/null", O_RDONLY), 0);
  case 1:
    dup2(open("/dev/null", O_WRONLY), 1);
  case 2:
    dup2(open("/dev/null", O_WRONLY), 2);
  }
  /* close all other file descriptors (socket, ...) */
  for (i = 3; i < FOPEN_MAX; i++) {
    if (i != f)
      close(i);
  }
#endif
}

void setup_child(int child, int i, int f) {
  // reset_signals();
  // mySignal(SIGINT, SIG_IGN);
  // if (!child)
  //   SETPGRP();
  /*
   * I don't know why but close_tty() sometimes interrupts loadGeneralFile() in
   * loadImage() and corrupt image data can be cached in ~/.w3m.
   */
  close_all_fds_except(i, f);
  IsForkChild = true;
  // fmInitialized = false;
}

const char *getWord(const char **str) {
  const char *p, *s;

  p = *str;
  SKIP_BLANKS(p);
  for (s = p; *p && !IS_SPACE(*p) && *p != ';'; p++)
    ;
  *str = p;
  return Strnew_charp_n(s, p - s)->ptr;
}

const char *getQWord(const char **str) {
  Str *tmp = Strnew();
  const char *p;
  int in_q = 0, in_dq = 0, esc = 0;

  p = *str;
  SKIP_BLANKS(p);
  for (; *p; p++) {
    if (esc) {
      if (in_q) {
        if (*p != '\\' && *p != '\'') /* '..\\..', '..\'..' */
          Strcat_char(tmp, '\\');
      } else if (in_dq) {
        if (*p != '\\' && *p != '"') /* "..\\..", "..\".." */
          Strcat_char(tmp, '\\');
      } else {
        if (*p != '\\' && *p != '\'' && /* ..\\.., ..\'.. */
            *p != '"' && !IS_SPACE(*p)) /* ..\".., ..\.. */
          Strcat_char(tmp, '\\');
      }
      Strcat_char(tmp, *p);
      esc = 0;
    } else if (*p == '\\') {
      esc = 1;
    } else if (in_q) {
      if (*p == '\'')
        in_q = 0;
      else
        Strcat_char(tmp, *p);
    } else if (in_dq) {
      if (*p == '"')
        in_dq = 0;
      else
        Strcat_char(tmp, *p);
    } else if (*p == '\'') {
      in_q = 1;
    } else if (*p == '"') {
      in_dq = 1;
    } else if (IS_SPACE(*p) || *p == ';') {
      break;
    } else {
      Strcat_char(tmp, *p);
    }
  }
  *str = p;
  return tmp->ptr;
}
