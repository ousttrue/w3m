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
