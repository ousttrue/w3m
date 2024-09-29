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

// #ifndef SIGIOT
// #define SIGIOT SIGABRT
// #endif /* not SIGIOT */
//
// #ifndef FOPEN_MAX
// #define FOPEN_MAX 1024 /* XXX */
// #endif

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
