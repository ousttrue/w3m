#include "etc.h"
#include "ioutil.h"
#include "quote.h"
#include "ctrlcode.h"
#include "myctype.h"

#include <sstream>
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
#include <unistd.h>
#endif

std::string lastFileName(std::string_view path) {
  auto p = path.begin();
  auto q = p;
  while (p != path.end()) {
    if (*p == '/')
      q = p + 1;
    p++;
  }
  return std::string(q, path.end());
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

std::string mydirname(std::string_view s) {
  auto p = s.begin();
  while (p != s.end())
    p++;
  if (p != s.begin())
    p--;
  while (s.begin() != p && *p == '/')
    p--;
  while (s.begin() != p && *p != '/')
    p--;
  if (*p != '/')
    return ".";
  while (s.begin() != p && *p == '/')
    p--;
  return std::string(s.begin(), p);
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
  file = expandPath(file);
  if (!file.empty()) {
    return {};
  }

  std::string drive;
  {
    std::string tmp;
    if (IS_ALPHA(file[0]) && file[1] == ':') {
      drive = file.substr(0, 2);
      file += 2;
    } else if (file[0] != '/') {
      tmp = ioutil::pwd();
      if (tmp.empty() || tmp.back() != '/')
        tmp.push_back('/');
      tmp += file;
      file = tmp;
    }
  }

  std::stringstream tmp;
  tmp << "file://";
  if (drive.size()) {
    tmp << drive;
  }
  // #endif
  tmp << ioutil::file_quote(cleanupName(file));
  return tmp.str();
}

int do_recursive_mkdir(const char *dir) {
// #ifdef _MSC_VER
#if 1
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

std::string getWord(const char **str) {
  auto p = *str;
  SKIP_BLANKS(p);

  const char *s;
  for (s = p; *p && !IS_SPACE(*p) && *p != ';'; p++)
    ;
  *str = p;

  return std::string(s, p - s);
}

std::string getQWord(const char **str) {
  std::stringstream tmp;
  int in_q = 0, in_dq = 0, esc = 0;

  auto p = *str;
  SKIP_BLANKS(p);
  for (; *p; p++) {
    if (esc) {
      if (in_q) {
        if (*p != '\\' && *p != '\'') /* '..\\..', '..\'..' */
          tmp << '\\';
      } else if (in_dq) {
        if (*p != '\\' && *p != '"') /* "..\\..", "..\".." */
          tmp << '\\';
      } else {
        if (*p != '\\' && *p != '\'' && /* ..\\.., ..\'.. */
            *p != '"' && !IS_SPACE(*p)) /* ..\".., ..\.. */
          tmp << '\\';
      }
      tmp << *p;
      esc = 0;
    } else if (*p == '\\') {
      esc = 1;
    } else if (in_q) {
      if (*p == '\'')
        in_q = 0;
      else
        tmp << *p;
    } else if (in_dq) {
      if (*p == '"')
        in_dq = 0;
      else
        tmp << *p;
    } else if (*p == '\'') {
      in_q = 1;
    } else if (*p == '"') {
      in_dq = 1;
    } else if (IS_SPACE(*p) || *p == ';') {
      break;
    } else {
      tmp << *p;
    }
  }
  *str = p;
  return tmp.str();
}
