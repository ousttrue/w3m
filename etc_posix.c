#include "etc.h"
#include "indep.h"
#include "tty.h"
#include "myctype.h"
#include "fm.h"
#include <pwd.h>
#include <sys/wait.h>
#ifdef INET6
#include <sys/socket.h>
#endif /* INET6 */
#include <netdb.h>
#include <unistd.h>

pid_t open_pipe_rw(FILE **fr, FILE **fw) {
  int fdr[2];
  int fdw[2];
  pid_t pid;

  if (fr && pipe(fdr) < 0)
    goto err0;
  if (fw && pipe(fdw) < 0)
    goto err1;

  tty_flush();
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

char *expandName(char *name) {
  char *p;
  struct passwd *passent, *getpwnam(const char *);
  Str extpath = NULL;

  if (name == NULL)
    return NULL;
  p = name;
  if (*p == '/') {
    if ((*(p + 1) == '~' && IS_ALPHA(*(p + 2))) && personal_document_root) {
      char *q;
      p += 2;
      q = strchr(p, '/');
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

void sleepSeconds(uint32_t seconds) { sleep(seconds); }
