#include <pwd.h>
#include <sys/wait.h>
#ifdef INET6
#include <sys/socket.h>
#endif /* INET6 */
#include <netdb.h>
char *FQDN(char *host) {
  char *p;
#ifndef INET6
  struct hostent *entry;
#else  /* INET6 */
  int *af;
#endif /* INET6 */

  if (host == NULL)
    return NULL;

  if (strcasecmp(host, "localhost") == 0)
    return host;

  for (p = host; *p && *p != '.'; p++)
    ;

  if (*p == '.')
    return host;

#ifndef INET6
  if (!(entry = gethostbyname(host)))
    return NULL;

  return allocStr(entry->h_name, -1);
#else  /* INET6 */
  for (af = ai_family_order_table[DNS_order];; af++) {
    int error;
    struct addrinfo hints;
    struct addrinfo *res, *res0;
    char *namebuf;

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_CANONNAME;
    hints.ai_family = *af;
    hints.ai_socktype = SOCK_STREAM;
    error = getaddrinfo(host, NULL, &hints, &res0);
    if (error) {
      if (*af == PF_UNSPEC) {
        /* all done */
        break;
      }
      /* try next address family */
      continue;
    }
    for (res = res0; res != NULL; res = res->ai_next) {
      if (res->ai_canonname) {
        /* found */
        namebuf = strdup(res->ai_canonname);
        freeaddrinfo(res0);
        return namebuf;
      }
    }
    freeaddrinfo(res0);
    if (*af == PF_UNSPEC) {
      break;
    }
  }
  /* all failed */
  return NULL;
#endif /* INET6 */
}


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

void mySystem(char *command, int background) {
  if (background) {
    tty_flush();
    if (!fork()) {
      setup_child(FALSE, 0, -1);
      myExec(command);
    }
  } else {
    system(command);
  }
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


