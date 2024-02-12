/* $Id: func.c,v 1.27 2003/09/26 17:59:51 ukai Exp $ */
/*
 * w3m func.c
 */
#include "w3m.h"
#include "terms.h"
#include "app.h"
#include "message.h"
#include "ctrlcode.h"
#include "func.h"
#include "myctype.h"
#include "regex.h"
#include "rc.h"
#include "buffer.h"
#include "history.h"
#include "alloc.h"
#include "proto.h"
#include "cookie.h"
#include "functable.c"
// #include "textlist.h"
// #include "hash.h"
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>

#define KEYDATA_HASH_SIZE 16
static Hash_iv *keyData = NULL;
static char keymap_initialized = false;
static struct stat sys_current_keymap_file;
static struct stat current_keymap_file;

void setKeymap(const char *p, int lineno, int verbose) {
  unsigned char *map = NULL;
  const char *s, *emsg;
  int c, f;

  s = getQWord(&p);
  c = getKey(s);
  if (c < 0) { /* error */
    if (lineno > 0)
      emsg = Sprintf("line %d: unknown key '%s'", lineno, s)->ptr;
    else
      emsg = Sprintf("defkey: unknown key '%s'", s)->ptr;
    record_err_message(emsg);
    if (verbose)
      disp_message_nsec(emsg, false, 1, true, false);
    return;
  }
  s = getWord(&p);
  f = getFuncList(s);
  if (f < 0) {
    if (lineno > 0)
      emsg = Sprintf("line %d: invalid command '%s'", lineno, s)->ptr;
    else
      emsg = Sprintf("defkey: invalid command '%s'", s)->ptr;
    record_err_message(emsg);
    if (verbose)
      disp_message_nsec(emsg, false, 1, true, false);
    return;
  }
  map = GlobalKeymap;
  map[c & 0x7F] = f;
  s = getQWord(&p);
  if (*s) {
    if (keyData == NULL)
      keyData = newHash_iv(KEYDATA_HASH_SIZE);
    putHash_iv(keyData, c, (void *)s);
  } else if (getKeyData(c))
    putHash_iv(keyData, c, NULL);
}

static void interpret_keymap(FILE *kf, struct stat *current, int force) {
  int fd;
  struct stat kstat;
  Str *line;
  const char *p, *s, *emsg;
  int lineno;
  int verbose = 1;

  if ((fd = fileno(kf)) < 0 || fstat(fd, &kstat) ||
      (!force && kstat.st_mtime == current->st_mtime &&
       kstat.st_dev == current->st_dev && kstat.st_ino == current->st_ino &&
       kstat.st_size == current->st_size))
    return;
  *current = kstat;

  lineno = 0;
  while (!feof(kf)) {
    line = Strfgets(kf);
    lineno++;
    Strchop(line);
    Strremovefirstspaces(line);
    if (line->length == 0)
      continue;
    p = line->ptr;
    s = getWord(&p);
    if (*s == '#') /* comment */
      continue;
    if (!strcmp(s, "keymap"))
      ;
    else if (!strcmp(s, "verbose")) {
      s = getWord(&p);
      if (*s)
        verbose = str_to_bool(s, verbose);
      continue;
    } else { /* error */
      emsg = Sprintf("line %d: syntax error '%s'", lineno, s)->ptr;
      record_err_message(emsg);
      if (verbose)
        disp_message_nsec(emsg, false, 1, true, false);
      continue;
    }
    setKeymap(p, lineno, verbose);
  }
}

void initKeymap(int force) {
  FILE *kf;

  if ((kf = fopen(confFile(KEYMAP_FILE), "rt")) != NULL) {
    interpret_keymap(kf, &sys_current_keymap_file,
                     force || !keymap_initialized);
    fclose(kf);
  }
  if ((kf = fopen(rcFile(keymap_file), "rt")) != NULL) {
    interpret_keymap(kf, &current_keymap_file, force || !keymap_initialized);
    fclose(kf);
  }
  keymap_initialized = true;
}

int getFuncList(const char *id) { return getHash_si(&functable, id, -1); }

const char *getKeyData(int key) {
  if (keyData == NULL)
    return NULL;
  return (char *)getHash_iv(keyData, key, NULL);
}

static int getKey2(const char **str) {
  const char *s = *str;
  int esc = 0, ctrl = 0;

  if (s == NULL || *s == '\0')
    return -1;

  if (strncasecmp(s, "C-", 2) == 0) { /* ^, ^[^ */
    s += 2;
    ctrl = 1;
  } else if (*s == '^' && *(s + 1)) { /* ^, ^[^ */
    s++;
    ctrl = 1;
  }

  if (ctrl) {
    *str = s + 1;
    if (*s >= '@' && *s <= '_') /* ^@ .. ^_ */
      return esc | (*s - '@');
    else if (*s >= 'a' && *s <= 'z') /* ^a .. ^z */
      return esc | (*s - 'a' + 1);
    else if (*s == '?') /* ^? */
      return esc | DEL_CODE;
    else
      return -1;
  }

  if (strncasecmp(s, "SPC", 3) == 0) { /* ' ' */
    *str = s + 3;
    return esc | ' ';
  } else if (strncasecmp(s, "TAB", 3) == 0) { /* ^i */
    *str = s + 3;
    return esc | '\t';
  } else if (strncasecmp(s, "DEL", 3) == 0) { /* ^? */
    *str = s + 3;
    return esc | DEL_CODE;
  }

  if (*s == '\\' && *(s + 1) != '\0') {
    s++;
    *str = s + 1;
    switch (*s) {
    case 'a': /* ^g */
      return esc | CTRL_G;
    case 'b': /* ^h */
      return esc | CTRL_H;
    case 't': /* ^i */
      return esc | CTRL_I;
    case 'n': /* ^j */
      return esc | CTRL_J;
    case 'r': /* ^m */
      return esc | CTRL_M;
    case 'e': /* ^[ */
      return esc | ESC_CODE;
    case '^': /* ^ */
      return esc | '^';
    case '\\': /* \ */
      return esc | '\\';
    default:
      return -1;
    }
  }
  *str = s + 1;
  if (IS_ASCII(*s)) /* Ascii */
    return esc | *s;
  else
    return -1;
}

int getKey(const char *s) {
  int c = getKey2(&s);
  return c;
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
