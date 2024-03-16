#include "etc.h"
#include "ioutil.h"
#include "quote.h"
#include "file_util.h"
#include "ctrlcode.h"
#include "app.h"
#include "html/readbuffer.h"
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
#define DEF_EXT_BROWSER "/usr/bin/firefox"
const char *ExtBrowser = DEF_EXT_BROWSER;
char *ExtBrowser2 = nullptr;
char *ExtBrowser3 = nullptr;
char *ExtBrowser4 = nullptr;
char *ExtBrowser5 = nullptr;
char *ExtBrowser6 = nullptr;
char *ExtBrowser7 = nullptr;
char *ExtBrowser8 = nullptr;
char *ExtBrowser9 = nullptr;

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

void mySystem(const char *command, int background) {
#ifdef _MSC_VER
#else
  if (background) {
    // flush_tty();
    if (!fork()) {
      setup_child(false, 0, -1);
      myExec(command);
    }
  } else
    system(command);
#endif
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

std::string expandPath(std::string_view name) {
  if (name.empty()) {
    return "";
  }

  auto p = name.begin();
  if (*p == '~') {
    p++;
    std::stringstream ss;
#ifdef _MSC_VER
    if (*p == '/' || *p == '\0') { /* ~/dir... or ~ */
      ss << getenv("USERPROFILE");
    }
#else
    if (IS_ALPHA(*p)) {
      auto q = strchr(p, '/');
      struct passwd *passent;
      if (q) { /* ~user/dir... */
        passent = getpwnam(allocStr(p, q - p));
        p = q;
      } else { /* ~user */
        passent = getpwnam(p);
        p = "";
      }
      if (!passent) {
        return name;
      }
      ss << passent->pw_dir;
    } else if (*p == '/' || *p == '\0') { /* ~/dir... or ~ */
      ss << getenv("HOME");
    }
#endif
    else {
      return {name.begin(), name.end()};
    }
    if (ss.str() == "/" && *p == '/') {
      p++;
    }
    ss << std::string_view{p, name.end()};
    return ss.str();
  }

  return {name.begin(), name.end()};
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

void myExec(const char *command) {
#ifdef _MSC_VER
#else
  // mySignal(SIGINT, SIG_DFL);
  execl("/bin/sh", "sh", "-c", command, 0);
  exit(127);
#endif
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

/* spawn external browser */
void invoke_browser(const char *url) {
  Str *cmd;
  int bg = 0, len;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  auto browser = App::instance().searchKeyData();
  if (browser == nullptr || *browser == '\0') {
    switch (prec_num) {
    case 0:
    case 1:
      browser = ExtBrowser;
      break;
    case 2:
      browser = ExtBrowser2;
      break;
    case 3:
      browser = ExtBrowser3;
      break;
    case 4:
      browser = ExtBrowser4;
      break;
    case 5:
      browser = ExtBrowser5;
      break;
    case 6:
      browser = ExtBrowser6;
      break;
    case 7:
      browser = ExtBrowser7;
      break;
    case 8:
      browser = ExtBrowser8;
      break;
    case 9:
      browser = ExtBrowser9;
      break;
    }
    if (browser == nullptr || *browser == '\0') {
      // browser = inputStr("Browse command: ", nullptr);
    }
  }
  if (browser == nullptr || *browser == '\0') {
    return;
  }

  if ((len = strlen(browser)) >= 2 && browser[len - 1] == '&' &&
      browser[len - 2] != '\\') {
    browser = allocStr(browser, len - 2);
    bg = 1;
  }
  cmd = myExtCommand((char *)browser, shell_quote(url), false);
  Strremovetrailingspaces(cmd);
  App::instance().endRawMode();
  mySystem(cmd->ptr, bg);
  App::instance().beginRawMode();
}
