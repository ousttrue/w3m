#include "etc.h"
#include "alloc.h"
#include "strcase.h"
#include "app.h"
#include "indep.h"
#include "fm.h"
#include "buffer.h"
#include "tty.h"
#include "myctype.h"
#include "html.h"
#include "hash.h"
#include "terms.h"
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>

// #ifndef HAVE_STRERROR
// char *strerror(int errno) {
//   extern char *sys_errlist[];
//   return sys_errlist[errno];
// }
// #endif /* not HAVE_STRERROR */

// extern Str correct_irrtag(int status);
// Str correct_irrtag(int status) {
//   char c;
//   Str tmp = Strnew();
//
//   while (status != R_ST_NORMAL) {
//     switch (status) {
//     case R_ST_CMNT:   /* required "-->" */
//     case R_ST_NCMNT1: /* required "->" */
//       c = '-';
//       break;
//     case R_ST_NCMNT2:
//     case R_ST_NCMNT3:
//     case R_ST_IRRTAG:
//     case R_ST_CMNT1:
//     case R_ST_CMNT2:
//     case R_ST_TAG:
//     case R_ST_TAG0:
//     case R_ST_EQL: /* required ">" */
//     case R_ST_VALUE:
//       c = '>';
//       break;
//     case R_ST_QUOTE:
//       c = '\'';
//       break;
//     case R_ST_DQUOTE:
//       c = '"';
//       break;
//     case R_ST_AMP:
//       c = ';';
//       break;
//     default:
//       return tmp;
//     }
//     next_status(c, &status);
//     Strcat_char(tmp, c);
//   }
//   return tmp;
// }

/* get last modified time */
char *last_modified(struct Buffer *buf) {
  struct TextListItem *ti;
  struct stat st;

  if (buf->document_header) {
    for (ti = buf->document_header->first; ti; ti = ti->next) {
      if (strncasecmp(ti->ptr, "Last-modified: ", 15) == 0) {
        return ti->ptr + 15;
      }
    }
    return "unknown";
  } else if (buf->currentURL.scheme == SCM_LOCAL) {
    if (stat(buf->currentURL.file, &st) < 0)
      return "unknown";
    return ctime(&st.st_mtime);
  }
  return "unknown";
}

static char roman_num1[] = {
    'i', 'x', 'c', 'm', '*',
};
static char roman_num5[] = {
    'v',
    'l',
    'd',
    '*',
};

static Str romanNum2(int l, int n) {
  Str s = Strnew();

  switch (n) {
  case 1:
  case 2:
  case 3:
    for (; n > 0; n--)
      Strcat_char(s, roman_num1[l]);
    break;
  case 4:
    Strcat_char(s, roman_num1[l]);
    Strcat_char(s, roman_num5[l]);
    break;
  case 5:
  case 6:
  case 7:
  case 8:
    Strcat_char(s, roman_num5[l]);
    for (n -= 5; n > 0; n--)
      Strcat_char(s, roman_num1[l]);
    break;
  case 9:
    Strcat_char(s, roman_num1[l]);
    Strcat_char(s, roman_num1[l + 1]);
    break;
  }
  return s;
}

Str romanNumeral(int n) {
  Str r = Strnew();

  if (n <= 0)
    return r;
  if (n >= 4000) {
    Strcat_charp(r, "**");
    return r;
  }
  Strcat(r, romanNum2(3, n / 1000));
  Strcat(r, romanNum2(2, (n % 1000) / 100));
  Strcat(r, romanNum2(1, (n % 100) / 10));
  Strcat(r, romanNum2(0, n % 10));

  return r;
}

Str romanAlphabet(int n) {
  Str r = Strnew();
  int l;
  char buf[14];

  if (n <= 0)
    return r;

  l = 0;
  while (n) {
    buf[l++] = 'a' + (n - 1) % 26;
    n = (n - 1) / 26;
  }
  l--;
  for (; l >= 0; l--)
    Strcat_char(r, buf[l]);

  return r;
}

#ifndef SIGIOT
#define SIGIOT SIGABRT
#endif /* not SIGIOT */

#ifndef FOPEN_MAX
#define FOPEN_MAX 1024 /* XXX */
#endif

void myExec(char *command) {
  mySignal(SIGINT, SIG_DFL);
  execl("/bin/sh", "sh", "-c", command, NULL);
  exit(127);
}

Str myExtCommand(char *cmd, char *arg, int redirect) {
  Str tmp = NULL;
  char *p;
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

Str myEditor(char *cmd, char *file, int line) {
  Str tmp = NULL;
  char *p;
  int set_file = false, set_line = false;

  for (p = cmd; *p; p++) {
    if (*p == '%' && *(p + 1) == 's' && !set_file) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(cmd, (int)(p - cmd));
      Strcat_charp(tmp, file);
      set_file = true;
      p++;
    } else if (*p == '%' && *(p + 1) == 'd' && !set_line && line > 0) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(cmd, (int)(p - cmd));
      Strcat(tmp, Sprintf("%d", line));
      set_line = true;
      p++;
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (!set_file) {
    if (tmp == NULL)
      tmp = Strnew_charp(cmd);
    if (!set_line && line > 1 && strcasestr(cmd, "vi"))
      Strcat(tmp, Sprintf(" +%d", line));
    Strcat_m_charp(tmp, " ", file, NULL);
  }
  return tmp;
}

int is_localhost(const char *host) {
  if (!host || !strcasecmp(host, "localhost") || !strcmp(host, "127.0.0.1") ||
      (HostName && !strcasecmp(host, HostName)) || !strcmp(host, "[::1]"))
    return true;
  return false;
}

char *file_to_url(char *file) {
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

void (*mySignal(int signal_number, void (*action)(int)))(int) {
#ifdef SA_RESTART
  struct sigaction new_action, old_action;

  sigemptyset(&new_action.sa_mask);
  new_action.sa_handler = action;
  if (signal_number == SIGALRM) {
#ifdef SA_INTERRUPT
    new_action.sa_flags = SA_INTERRUPT;
#else
    new_action.sa_flags = 0;
#endif
  } else {
    new_action.sa_flags = SA_RESTART;
  }
  sigaction(signal_number, &new_action, &old_action);
  return (old_action.sa_handler);
#else
  return (signal(signal_number, action));
#endif
}
