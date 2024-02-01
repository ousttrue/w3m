#include "etc.h"
#include "line.h"
#include "message.h"
#include "screen.h"
#include "terms.h"
#include "display.h"
#include "readbuffer.h"
#include "w3m.h"
#include "fm.h"
#include "buffer.h"
#include "signal_util.h"
#include <pwd.h>
#include "myctype.h"
#include "local_cgi.h"
#include "textlist.h"
#include "proto.h"
#include "indep.h"
#include <vector>

#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#if defined(HAVE_WAITPID) || defined(HAVE_WAIT3)
#include <sys/wait.h>
#endif

struct auth_pass {
  int bad;
  int is_proxy;
  Str *host;
  int port;
  /*    Str* file; */
  Str *realm;
  Str *uname;
  Str *pwd;
  struct auth_pass *next;
};

struct auth_pass *passwords = NULL;

int columnSkip(Buffer *buf, int offset) {
  int i, maxColumn;
  int column = buf->currentColumn + offset;
  int nlines = buf->LINES + 1;
  Line *l;

  maxColumn = 0;
  for (i = 0, l = buf->topLine; i < nlines && l != NULL; i++, l = l->next) {
    if (l->width < 0)
      l->width = COLPOS(l, l->len);
    if (l->width - 1 > maxColumn)
      maxColumn = l->width - 1;
  }
  maxColumn -= buf->COLS - 1;
  if (column < maxColumn)
    maxColumn = column;
  if (maxColumn < 0)
    maxColumn = 0;

  if (buf->currentColumn == maxColumn)
    return 0;
  buf->currentColumn = maxColumn;
  return 1;
}

int columnPos(Line *line, int column) {
  int i;

  for (i = 1; i < line->len; i++) {
    if (COLPOS(line, i) > column)
      break;
  }

  for (i--; i > 0 && line->propBuf[i] & PC_WCHAR2; i--)
    ;
  return i;
}

Line *lineSkip(Buffer *buf, Line *line, int offset, int last) {
  int i;
  Line *l;

  l = currentLineSkip(buf, line, offset, last);
  if (!nextpage_topline)
    for (i = buf->LINES - 1 - (buf->lastLine->linenumber - l->linenumber);
         i > 0 && l->prev != NULL; i--, l = l->prev)
      ;
  return l;
}

Line *currentLineSkip(Buffer *buf, Line *line, int offset, int last) {
  int i, n;
  Line *l = line;

  if (buf->pagerSource && !(buf->bufferprop & BP_CLOSE)) {
    n = line->linenumber + offset + buf->LINES;
    if (buf->lastLine->linenumber < n)
      getNextPage(buf, n - buf->lastLine->linenumber);
    while ((last || (buf->lastLine->linenumber < n)) &&
           (getNextPage(buf, 1) != NULL))
      ;
    if (last)
      l = buf->lastLine;
  }

  if (offset == 0)
    return l;
  if (offset > 0)
    for (i = 0; i < offset && l->next != NULL; i++, l = l->next)
      ;
  else
    for (i = 0; i < -offset && l->prev != NULL; i++, l = l->prev)
      ;
  return l;
}

/*
 * Check character type
 */

Str *checkType(Str *s, Lineprop **oprop, Linecolor **ocolor) {
  Lineprop mode;
  Lineprop effect = PE_NORMAL;
  Lineprop *prop;
  static Lineprop *prop_buffer = NULL;
  static int prop_size = 0;
  char *str = s->ptr, *endp = &s->ptr[s->length], *bs = NULL;
  int do_copy = FALSE;

  if (prop_size < s->length) {
    prop_size = (s->length > LINELEN) ? s->length : LINELEN;
    prop_buffer = (Lineprop *)New_Reuse(Lineprop, prop_buffer, prop_size);
  }
  prop = prop_buffer;

  if (ShowEffect) {
    bs = (char *)memchr(str, '\b', s->length);
    if ((bs != NULL)) {
      char *sp = str, *ep;
      s = Strnew_size(s->length);
      do_copy = TRUE;
      ep = endp;
      if (bs && ep > bs - 2)
        ep = bs - 2;
      for (; str < ep && IS_ASCII(*str); str++) {
        *(prop++) = PE_NORMAL | (IS_CNTRL(*str) ? PC_CTRL : PC_ASCII);
      }
      Strcat_charp_n(s, sp, (int)(str - sp));
    }
  }
  if (!do_copy) {
    for (; str < endp && IS_ASCII(*str); str++) {
      *(prop++) = PE_NORMAL | (IS_CNTRL(*str) ? PC_CTRL : PC_ASCII);
    }
  }

  while (str < endp) {
    if (prop - prop_buffer >= prop_size)
      break;
    if (bs != NULL) {
      if (str == bs - 1 && *str == '_') {
        str += 2;
        effect = PE_UNDER;
        if (str < endp)
          bs = (char *)memchr(str, '\b', endp - str);
        continue;
      } else if (str == bs) {
        if (*(str + 1) == '_') {
          if (s->length) {
            str += 2;
            *(prop - 1) |= PE_UNDER;
          } else {
            str++;
          }
        } else {
          if (s->length) {
            if (*(str - 1) == *(str + 1)) {
              *(prop - 1) |= PE_BOLD;
              str += 2;
            } else {
              Strshrink(s, 1);
              prop--;
              str++;
            }
          } else {
            str++;
          }
        }
        if (str < endp)
          bs = (char *)memchr(str, '\b', endp - str);
        continue;
      }
    }

    mode = get_mctype(str) | effect;
    *(prop++) = mode;
    {
      if (do_copy)
        Strcat_char(s, (char)*str);
      str++;
    }
    effect = PE_NORMAL;
  }
  *oprop = prop_buffer;
  return s;
}

// static int nextColumn(int n, char *p, Lineprop *pr) {
//   if (*pr & PC_CTRL) {
//     if (*p == '\t')
//       return (n + Tabstop) / Tabstop * Tabstop;
//     else if (*p == '\n')
//       return n + 1;
//     else if (*p != '\r')
//       return n + 2;
//     return n;
//   } else {
//     auto utf8 = Utf8::from((const char8_t *)p);
//     return n + utf8.width();
//   }
// }

int calcPosition(char *l, Lineprop *pr, int len, int pos, int bpos, int mode) {
  if (l == NULL || len == 0 || pos < 0)
    return bpos;

  static std::vector<int> realColumn;
  static char *prevl = NULL;
  if (l == prevl && mode == CP_AUTO) {
    if (pos <= len)
      return realColumn[pos];
  }

  realColumn.clear();
  prevl = l;
  int i = 0;
  int j = bpos;
  for (; i < len;) {
    if (pr[i] & PC_CTRL) {
      realColumn.push_back(j);
      if (l[i] == '\t')
        j += Tabstop - (j % Tabstop);
      else if (l[i] == '\n')
        j += 1;
      else if (l[i] != '\r')
        j += 2;
      ++i;
    } else {
      auto utf8 = Utf8::from((const char8_t *)&l[i]);
      auto [cp, bytes] = utf8.codepoint();
      for (int k = 0; k < bytes; ++k) {
        realColumn.push_back(j);
        ++i;
      }
      j += utf8.width();
    }
  }
  realColumn.push_back(j);
  if (pos < static_cast<int>(realColumn.size())) {
    return realColumn[pos];
  }
  return j;
}

int columnLen(Line *line, int column) {
  for (auto i = 0; i < line->len;) {
    auto j = calcPosition(&line->lineBuf[i], &line->propBuf[i], line->len, i, 0,
                          CP_AUTO);
    if (j > column)
      return i;
    auto utf8 = Utf8::from((const char8_t *)&line->lineBuf[i]);
    auto [cp, bytes] = utf8.codepoint();
    i += bytes;
  }
  return line->len;
}

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

const char *mybasename(const char *s) {
  const char *p = s;
  while (*p)
    p++;
  while (s <= p && *p != '/')
    p--;
  if (*p == '/')
    p++;
  else
    p = s;
  return allocStr(p, -1);
}

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

/*
 * RFC2617: 1.2 Access Authentication Framework
 *
 * The realm value (case-sensitive), in combination with the canonical root
 * URL (the absoluteURI for the server whose abs_path is empty; see section
 * 5.1.2 of RFC2616 ) of the server being accessed, defines the protection
 * space. These realms allow the protected resources on a server to be
 * partitioned into a set of protection spaces, each with its own
 * authentication schema and/or authorization database.
 *
 */
static void add_auth_pass_entry(const struct auth_pass *ent, int netrc,
                                int override) {
  if ((ent->host || netrc) /* netrc accept default (host == NULL) */
      && (ent->is_proxy || ent->realm || netrc) && ent->uname && ent->pwd) {
    struct auth_pass *newent = (struct auth_pass *)New(struct auth_pass);
    memcpy(newent, ent, sizeof(struct auth_pass));
    if (override) {
      newent->next = passwords;
      passwords = newent;
    } else {
      if (passwords == NULL)
        passwords = newent;
      else if (passwords->next == NULL)
        passwords->next = newent;
      else {
        struct auth_pass *ep = passwords;
        for (; ep->next; ep = ep->next)
          ;
        ep->next = newent;
      }
    }
  }
  /* ignore invalid entries */
}

static struct auth_pass *find_auth_pass_entry(const char *host, int port,
                                              const char *realm,
                                              const char *uname, int is_proxy) {
  struct auth_pass *ent;
  for (ent = passwords; ent != NULL; ent = ent->next) {
    if (ent->is_proxy == is_proxy && (ent->bad != TRUE) &&
        (!ent->host || !Strcasecmp_charp(ent->host, host)) &&
        (!ent->port || ent->port == port) &&
        (!ent->uname || !uname || !Strcmp_charp(ent->uname, uname)) &&
        (!ent->realm || !realm || !Strcmp_charp(ent->realm, realm)))
      return ent;
  }
  return NULL;
}

int find_auth_user_passwd(ParsedURL *pu, char *realm, Str **uname, Str **pwd,
                          int is_proxy) {
  struct auth_pass *ent;

  if (pu->user && pu->pass) {
    *uname = Strnew_charp(pu->user);
    *pwd = Strnew_charp(pu->pass);
    return 1;
  }
  ent = find_auth_pass_entry(pu->host, pu->port, realm, pu->user, is_proxy);
  if (ent) {
    *uname = ent->uname;
    *pwd = ent->pwd;
    return 1;
  }
  return 0;
}

void add_auth_user_passwd(ParsedURL *pu, char *realm, Str *uname, Str *pwd,
                          int is_proxy) {
  struct auth_pass ent;
  memset(&ent, 0, sizeof(ent));

  ent.is_proxy = is_proxy;
  ent.host = Strnew_charp(pu->host);
  ent.port = pu->port;
  ent.realm = Strnew_charp(realm);
  ent.uname = uname;
  ent.pwd = pwd;
  add_auth_pass_entry(&ent, 0, 1);
}

void invalidate_auth_user_passwd(ParsedURL *pu, char *realm, Str *uname,
                                 Str *pwd, int is_proxy) {
  struct auth_pass *ent;
  ent = find_auth_pass_entry(pu->host, pu->port, realm, NULL, is_proxy);
  if (ent) {
    ent->bad = TRUE;
  }
  return;
}

/* passwd */
/*
 * machine <host>
 * host <host>
 * port <port>
 * proxy
 * path <file>	; not used
 * realm <realm>
 * login <login>
 * passwd <passwd>
 * password <passwd>
 */

static Str *next_token(Str *arg) {
  Str *narg = NULL;
  char *p, *q;
  if (arg == NULL || arg->length == 0)
    return NULL;
  p = arg->ptr;
  q = p;
  SKIP_NON_BLANKS(q);
  if (*q != '\0') {
    *q++ = '\0';
    SKIP_BLANKS(q);
    if (*q != '\0')
      narg = Strnew_charp(q);
  }
  return narg;
}

static void parsePasswd(FILE *fp, int netrc) {
  struct auth_pass ent;
  Str *line = NULL;

  bzero(&ent, sizeof(struct auth_pass));
  while (1) {
    Str *arg = NULL;
    char *p;

    if (line == NULL || line->length == 0)
      line = Strfgets(fp);
    if (line->length == 0)
      break;
    Strchop(line);
    Strremovefirstspaces(line);
    p = line->ptr;
    if (*p == '#' || *p == '\0') {
      line = NULL;
      continue; /* comment or empty line */
    }
    arg = next_token(line);

    if (!strcmp(p, "machine") || !strcmp(p, "host") ||
        (netrc && !strcmp(p, "default"))) {
      add_auth_pass_entry(&ent, netrc, 0);
      bzero(&ent, sizeof(struct auth_pass));
      if (netrc)
        ent.port = 21; /* XXX: getservbyname("ftp"); ? */
      if (strcmp(p, "default") != 0) {
        line = next_token(arg);
        ent.host = arg;
      } else {
        line = arg;
      }
    } else if (!netrc && !strcmp(p, "port") && arg) {
      line = next_token(arg);
      ent.port = atoi(arg->ptr);
    } else if (!netrc && !strcmp(p, "proxy")) {
      ent.is_proxy = 1;
      line = arg;
    } else if (!netrc && !strcmp(p, "path")) {
      line = next_token(arg);
      /* ent.file = arg; */
    } else if (!netrc && !strcmp(p, "realm")) {
      /* XXX: rest of line becomes arg for realm */
      line = NULL;
      ent.realm = arg;
    } else if (!strcmp(p, "login")) {
      line = next_token(arg);
      ent.uname = arg;
    } else if (!strcmp(p, "password") || !strcmp(p, "passwd")) {
      line = next_token(arg);
      ent.pwd = arg;
    } else if (netrc && !strcmp(p, "machdef")) {
      while ((line = Strfgets(fp))->length != 0) {
        if (*line->ptr == '\n')
          break;
      }
      line = NULL;
    } else if (netrc && !strcmp(p, "account")) {
      /* ignore */
      line = next_token(arg);
    } else {
      /* ignore rest of line */
      line = NULL;
    }
  }
  add_auth_pass_entry(&ent, netrc, 0);
}

/* FIXME: gettextize? */
#define FILE_IS_READABLE_MSG                                                   \
  "SECURITY NOTE: file %s must not be accessible by others"

FILE *openSecretFile(const char *fname) {
  const char *efname;
  struct stat st;

  if (fname == NULL)
    return NULL;
  efname = expandPath(fname);
  if (stat(efname, &st) < 0)
    return NULL;

  /* check permissions, if group or others readable or writable,
   * refuse it, because it's insecure.
   *
   * XXX: disable_secret_security_check will introduce some
   *    security issues, but on some platform such as Windows
   *    it's not possible (or feasible) to disable group|other
   *    readable and writable.
   *   [w3m-dev 03368][w3m-dev 03369][w3m-dev 03370]
   */
  if (disable_secret_security_check)
    /* do nothing */;
  else if ((st.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
    if (fmInitialized) {
      message(Sprintf(FILE_IS_READABLE_MSG, fname)->ptr, 0, 0);
      refresh(term_io());
    } else {
      fputs(Sprintf(FILE_IS_READABLE_MSG, fname)->ptr, stderr);
      fputc('\n', stderr);
    }
    sleep(2);
    return NULL;
  }

  return fopen(efname, "r");
}

void loadPasswd(void) {
  FILE *fp;

  passwords = NULL;
  fp = openSecretFile(passwd_file);
  if (fp != NULL) {
    parsePasswd(fp, 0);
    fclose(fp);
  }

  /* for FTP */
  fp = openSecretFile("~/.netrc");
  if (fp != NULL) {
    parsePasswd(fp, 1);
    fclose(fp);
  }
  return;
}

/* get last modified time */
const char *last_modified(Buffer *buf) {
  TextListItem *ti;
  struct stat st;

  if (buf->document_header) {
    for (ti = buf->document_header->first; ti; ti = ti->next) {
      if (strncasecmp(ti->ptr, "Last-modified: ", 15) == 0) {
        return ti->ptr + 15;
      }
    }
    return "unknown";
  } else if (buf->currentURL.schema == SCM_LOCAL) {
    if (stat(buf->currentURL.file, &st) < 0)
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
      setup_child(FALSE, 0, -1);
      myExec(command);
    }
  } else
    system(command);
}

Str *myExtCommand(const char *cmd, const char *arg, int redirect) {
  Str *tmp = NULL;
  const char *p;
  int set_arg = FALSE;

  for (p = cmd; *p; p++) {
    if (*p == '%' && *(p + 1) == 's' && !set_arg) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(cmd, (int)(p - cmd));
      Strcat_charp(tmp, arg);
      set_arg = TRUE;
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

Str *myEditor(const char *cmd, const char *file, int line) {
  Str *tmp = NULL;
  int set_file = FALSE, set_line = FALSE;

  for (auto p = cmd; *p; p++) {
    if (*p == '%' && *(p + 1) == 's' && !set_file) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(cmd, (int)(p - cmd));
      Strcat_charp(tmp, file);
      set_file = TRUE;
      p++;
    } else if (*p == '%' && *(p + 1) == 'd' && !set_line && line > 0) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(cmd, (int)(p - cmd));
      Strcat(tmp, Sprintf("%d", line));
      set_line = TRUE;
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

int is_localhost(const char *host) {
  if (!host || !strcasecmp(host, "localhost") || !strcmp(host, "127.0.0.1") ||
      (HostName && !strcasecmp(host, HostName)) || !strcmp(host, "[::1]"))
    return TRUE;
  return FALSE;
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
    tmp = Strnew_charp(CurrentDir);
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

const char *url_unquote_conv0(const char *url) {
  Str *tmp;
  tmp = Str_url_unquote(Strnew_charp(url), FALSE, TRUE);
  return tmp->ptr;
}

static auto Base64Table =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

Str *base64_encode(const char *src, int len) {

  auto k = len;
  if (k % 3)
    k += 3 - (k % 3);
  k = k / 3 * 4;

  if (!len || k + 1 < len)
    return Strnew();

  auto dest = Strnew_size(k);
  if (dest->area_size <= k) {
    Strfree(dest);
    return Strnew();
  }

  auto s = (unsigned char *)src;
  auto in = s;

  auto endw = s + len - 2;

  while (in < endw) {
    auto j = *in++;
    j = j << 8 | *in++;
    j = j << 8 | *in++;

    Strcatc(dest, Base64Table[(j >> 18) & 0x3f]);
    Strcatc(dest, Base64Table[(j >> 12) & 0x3f]);
    Strcatc(dest, Base64Table[(j >> 6) & 0x3f]);
    Strcatc(dest, Base64Table[j & 0x3f]);
  }

  if (s + len - in) {
    auto j = *in++;
    if (s + len - in) {
      j = j << 8 | *in++;
      j = j << 8;
      Strcatc(dest, Base64Table[(j >> 18) & 0x3f]);
      Strcatc(dest, Base64Table[(j >> 12) & 0x3f]);
      Strcatc(dest, Base64Table[(j >> 6) & 0x3f]);
    } else {
      j = j << 8;
      j = j << 8;
      Strcatc(dest, Base64Table[(j >> 18) & 0x3f]);
      Strcatc(dest, Base64Table[(j >> 12) & 0x3f]);
      Strcatc(dest, '=');
    }
    Strcatc(dest, '=');
  }
  Strnulterm(dest);
  return dest;
}
