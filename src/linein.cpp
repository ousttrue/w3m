#include "linein.h"
#include "tabbuffer.h"
#include "etc.h"
#include "w3m.h"
#include "alloc.h"
#include "screen.h"
#include "rc.h"
#include "utf8.h"
#include "display.h"
#include "buffer.h"
#include "terms.h"
#include "proto.h"
#include "form.h"
#include "ctrlcode.h"
#include "local_cgi.h"
#include "myctype.h"
#include "history.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/dir.h>
typedef struct direct Directory;

/* Completion status. */
#define CPL_OK 0
#define CPL_AMBIG 1
#define CPL_FAIL 2
#define CPL_MENU 3

#define CPL_NEVER 0x0
#define CPL_OFF 0x1
#define CPL_ON 0x2
#define CPL_ALWAYS 0x4
#define CPL_URL 0x8

bool space_autocomplete = false;
bool emacs_like_lineedit = false;

#define STR_LEN 1024
#define CLEN (COLS - 2)

static Str *strBuf;
static Lineprop strProp[STR_LEN];

static Str *CompleteBuf;
static Str *CFileName;
static Str *CBeforeBuf;
static Str *CAfterBuf;
static Str *CDirBuf;
static char **CFileBuf = NULL;
static int NCFileBuf;
static int NCFileOffset;

static int strCmp(const void *s1, const void *s2) {
  return strcmp(*(const char **)s1, *(const char **)s2);
}

static void insertself(char c), _mvR(void), _mvL(void), _mvRw(void),
    _mvLw(void), delC(void), insC(void), _mvB(void), _mvE(void), _enter(void),
    _quo(void), _bs(void), _bsw(void), killn(void), killb(void), _inbrk(void),
    _esc(void), _editor(void), _prev(void), _next(void), _compl(void),
    _tcompl(void), _dcompl(void), _rdcompl(void), _rcompl(void);

static int terminated(unsigned char c);
#define iself ((void (*)())insertself)

static void next_compl(int next);
static void next_dcompl(int next);
static Str *doComplete(Str *ifn, int *status, int next);

/* *INDENT-OFF* */
void (*InputKeymap[32])() = {
    /*  C-@     C-a     C-b     C-c     C-d     C-e     C-f     C-g     */
    _compl,
    _mvB,
    _mvL,
    _inbrk,
    delC,
    _mvE,
    _mvR,
    _inbrk,
    /*  C-h     C-i     C-j     C-k     C-l     C-m     C-n     C-o     */
    _bs,
    iself,
    _enter,
    killn,
    iself,
    _enter,
    _next,
    _editor,
    /*  C-p     C-q     C-r     C-s     C-t     C-u     C-v     C-w     */
    _prev,
    _quo,
    _bsw,
    iself,
    _mvLw,
    killb,
    _quo,
    _bsw,
    /*  C-x     C-y     C-z     C-[     C-\     C-]     C-^     C-_     */
    _tcompl,
    _mvRw,
    iself,
    _esc,
    iself,
    iself,
    iself,
    iself,
};
/* *INDENT-ON* */

static int setStrType(Str *str, Lineprop *prop);
static void addPasswd(char *p, Lineprop *pr, int len, int pos, int limit);
static void addStr(char *p, Lineprop *pr, int len, int pos, int limit);

static int CPos, CLen, offset;
static int i_cont, i_broken, i_quote;
static int cm_mode, cm_next, cm_clear, cm_disp_next, cm_disp_clear;
static int need_redraw, is_passwd;
static int move_word;

static Hist *CurrentHist;
static Str *strCurrentBuf;
static int use_hist;

void inputLineHistSearch(const char *prompt, const char *def_str,
                         InputFlags flag, Hist *hist, IncFunc incrfunc,
                         const OnInput &onInput) {
  int opos, x, y, lpos, rpos, epos;
  unsigned char c;
  char *p;

  is_passwd = false;
  move_word = true;

  CurrentHist = hist;
  if (hist != NULL) {
    use_hist = true;
    strCurrentBuf = NULL;
  } else {
    use_hist = false;
  }
  if (flag & IN_URL) {
    cm_mode = CPL_ALWAYS | CPL_URL;
  } else if (flag & IN_FILENAME) {
    cm_mode = CPL_ALWAYS;
  } else if (flag & IN_PASSWORD) {
    cm_mode = CPL_NEVER;
    is_passwd = true;
    move_word = false;
  } else if (flag & IN_COMMAND)
    cm_mode = CPL_ON;
  else
    cm_mode = CPL_OFF;
  opos = get_strwidth(prompt);
  epos = CLEN - opos;
  if (epos < 0)
    epos = 0;
  lpos = epos / 3;
  rpos = epos * 2 / 3;
  offset = 0;

  if (def_str) {
    strBuf = Strnew_charp(def_str);
    CLen = CPos = setStrType(strBuf, strProp);
  } else {
    strBuf = Strnew();
    CLen = CPos = 0;
  }

  i_cont = true;
  i_broken = false;
  i_quote = false;
  cm_next = false;
  cm_disp_next = -1;
  need_redraw = false;

  do {
    x = bytePosToColumn(strBuf->ptr, strProp, CLen, CPos, 0, true);
    if (x - rpos > offset) {
      y = bytePosToColumn(strBuf->ptr, strProp, CLen, CLen, 0, false);
      if (y - epos > x - rpos)
        offset = x - rpos;
      else if (y - epos > 0)
        offset = y - epos;
    } else if (x - lpos < offset) {
      if (x - lpos > 0)
        offset = x - lpos;
      else
        offset = 0;
    }
    move(LASTLINE, 0);
    addstr(prompt);
    if (is_passwd)
      addPasswd(strBuf->ptr, strProp, CLen, offset, COLS - opos);
    else
      addStr(strBuf->ptr, strProp, CLen, offset, COLS - opos);
    clrtoeolx();
    move(LASTLINE, opos + x - offset);
    refresh(term_io());

  next_char:
    c = getch();
    cm_clear = true;
    cm_disp_clear = true;
    if (!i_quote && (((cm_mode & CPL_ALWAYS) &&
                      (c == CTRL_I || (space_autocomplete && c == ' '))) ||
                     ((cm_mode & CPL_ON) && (c == CTRL_I)))) {
      if (emacs_like_lineedit && cm_next) {
        _dcompl();
        need_redraw = true;
      } else {
        _compl();
        cm_disp_next = -1;
      }
    } else if (!i_quote && CLen == CPos &&
               (cm_mode & CPL_ALWAYS || cm_mode & CPL_ON) && c == CTRL_D) {
      if (!emacs_like_lineedit) {
        _dcompl();
        need_redraw = true;
      }
    } else if (!i_quote && c == DEL_CODE) {
      _bs();
      cm_next = false;
      cm_disp_next = -1;
    } else if (!i_quote && c < 0x20) { /* Control code */
      if (incrfunc == NULL || (c = incrfunc((int)c, strBuf, strProp)) < 0x20) {
        auto callback = (void (*)(char c))InputKeymap[(int)c];
        callback(c);
      }
      if (incrfunc && c != (unsigned char)-1 && c != CTRL_J)
        incrfunc(-1, strBuf, strProp);
      if (cm_clear)
        cm_next = false;
      if (cm_disp_clear)
        cm_disp_next = -1;
    } else {
      i_quote = false;
      cm_next = false;
      cm_disp_next = -1;
      if (CLen >= STR_LEN)
        goto next_char;
      insC();
      strBuf->ptr[CPos] = c;
      if (!is_passwd && get_mctype((const char *)&c) == PC_CTRL)
        strProp[CPos] = PC_CTRL;
      else
        strProp[CPos] = PC_ASCII;
      CPos++;
      if (incrfunc)
        incrfunc(-1, strBuf, strProp);
    }
    if (CLen && (flag & IN_CHAR))
      break;
  } while (i_cont);

  if (CurrentTab) {
    if (need_redraw)
      displayBuffer(Currentbuf, B_FORCE_REDRAW);
  }

  if (i_broken) {
    onInput(NULL);
    return;
  }

  move(LASTLINE, 0);
  refresh(term_io());
  p = strBuf->ptr;
  if (flag & (IN_FILENAME | IN_COMMAND)) {
    SKIP_BLANKS(p);
  }
  if (use_hist && !(flag & IN_URL) && *p != '\0') {
    const char *q = lastHist(hist);
    if (!q || strcmp(q, p))
      pushHist(hist, p);
  }
  if (flag & IN_FILENAME) {
    onInput(expandPath(p));
  } else {
    onInput(Strnew_charp(p)->ptr);
  }
}

static void addPasswd(char *p, Lineprop *pr, int len, int offset, int limit) {
  auto ncol = bytePosToColumn(p, pr, len, len, 0, false);
  if (ncol > offset + limit) {
    ncol = offset + limit;
  }
  int rcol = 0;
  if (offset) {
    addChar('{', 0);
    rcol = offset + 1;
  }
  for (; rcol < ncol; rcol++)
    addChar('*', 0);
}

static void addStr(char *p, Lineprop *pr, int len, int offset, int limit) {
  int i = 0, rcol = 0, ncol, delta = 1;

  if (offset) {
    for (i = 0; i < len; i++) {
      if (bytePosToColumn(p, pr, len, i, 0, false) > offset)
        break;
    }
    if (i >= len)
      return;
    addChar('{', 0);
    rcol = offset + 1;
    ncol = bytePosToColumn(p, pr, len, i, 0, false);
    for (; rcol < ncol; rcol++)
      addChar(' ', 0);
  }
  for (; i < len; i += delta) {
    ncol = bytePosToColumn(p, pr, len, i + delta, 0, false);
    if (ncol - offset > limit)
      break;
    if (p[i] == '\t') {
      for (; rcol < ncol; rcol++)
        addChar(' ', 0);
      continue;
    } else {
      addChar(p[i], pr[i]);
    }
    rcol = ncol;
  }
}

static void _esc(void) {
  char c;

  switch (c = getch()) {
  case '[':
  case 'O':
    switch (c = getch()) {
    case 'A':
      _prev();
      break;
    case 'B':
      _next();
      break;
    case 'C':
      _mvR();
      break;
    case 'D':
      _mvL();
      break;
    }
    break;
  case CTRL_I:
  case ' ':
    if (emacs_like_lineedit) {
      _rdcompl();
      cm_clear = false;
      need_redraw = true;
    } else
      _rcompl();
    break;
  case CTRL_D:
    if (!emacs_like_lineedit)
      _rdcompl();
    need_redraw = true;
    break;
  case 'f':
    if (emacs_like_lineedit)
      _mvRw();
    break;
  case 'b':
    if (emacs_like_lineedit)
      _mvLw();
    break;
  case CTRL_H:
    if (emacs_like_lineedit)
      _bsw();
    break;
  }
}

static void insC(void) {
  int i;

  Strinsert_char(strBuf, CPos, ' ');
  CLen = strBuf->length;
  for (i = CLen; i > CPos; i--) {
    strProp[i] = strProp[i - 1];
  }
}

static void delC(void) {
  int i = CPos;
  int delta = 1;

  if (CLen == CPos)
    return;
  for (i = CPos; i < CLen; i++) {
    strProp[i] = strProp[i + delta];
  }
  Strdelete(strBuf, CPos, delta);
  CLen -= delta;
}

static void _mvL(void) {
  if (CPos > 0)
    CPos--;
}

static void _mvLw(void) {
  int first = 1;
  while (CPos > 0 && (first || !terminated(strBuf->ptr[CPos - 1]))) {
    CPos--;
    first = 0;
    if (!move_word)
      break;
  }
}

static void _mvRw(void) {
  int first = 1;
  while (CPos < CLen && (first || !terminated(strBuf->ptr[CPos - 1]))) {
    CPos++;
    first = 0;
    if (!move_word)
      break;
  }
}

static void _mvR(void) {
  if (CPos < CLen)
    CPos++;
}

static void _bs(void) {
  if (CPos > 0) {
    _mvL();
    delC();
  }
}

static void _bsw(void) {
  int t = 0;
  while (CPos > 0 && !t) {
    _mvL();
    t = (move_word && terminated(strBuf->ptr[CPos - 1]));
    delC();
  }
}

static void _enter(void) { i_cont = false; }

static void insertself(char c) {
  if (CLen >= STR_LEN)
    return;
  insC();
  strBuf->ptr[CPos] = c;
  strProp[CPos] = (is_passwd) ? PC_ASCII : PC_CTRL;
  CPos++;
}

static void _quo(void) { i_quote = true; }

static void _mvB(void) { CPos = 0; }

static void _mvE(void) { CPos = CLen; }

static void killn(void) {
  CLen = CPos;
  Strtruncate(strBuf, CLen);
}

static void killb(void) {
  while (CPos > 0)
    _bs();
}

static void _inbrk(void) {
  i_cont = false;
  i_broken = true;
}

static void _compl(void) { next_compl(1); }

static void _rcompl(void) { next_compl(-1); }

static void _tcompl(void) {
  if (cm_mode & CPL_OFF)
    cm_mode = CPL_ON;
  else if (cm_mode & CPL_ON)
    cm_mode = CPL_OFF;
}

static void next_compl(int next) {
  int status;
  int b, a;
  Str *buf;
  Str *s;

  if (cm_mode == CPL_NEVER || cm_mode & CPL_OFF)
    return;
  cm_clear = false;
  if (!cm_next) {
    if (cm_mode & CPL_ALWAYS) {
      b = 0;
    } else {
      for (b = CPos - 1; b >= 0; b--) {
        if ((strBuf->ptr[b] == ' ' || strBuf->ptr[b] == CTRL_I) &&
            !((b > 0) && strBuf->ptr[b - 1] == '\\'))
          break;
      }
      b++;
    }
    a = CPos;
    CBeforeBuf = Strsubstr(strBuf, 0, b);
    buf = Strsubstr(strBuf, b, a - b);
    CAfterBuf = Strsubstr(strBuf, a, strBuf->length - a);
    s = doComplete(buf, &status, next);
  } else {
    s = doComplete(strBuf, &status, next);
  }
  if (next == 0)
    return;

  if (status != CPL_OK && status != CPL_MENU)
    bell();
  if (status == CPL_FAIL)
    return;

  strBuf = Strnew_m_charp(CBeforeBuf->ptr, s->ptr, CAfterBuf->ptr, NULL);
  CLen = setStrType(strBuf, strProp);
  CPos = CBeforeBuf->length + s->length;
  if (CPos > CLen)
    CPos = CLen;
}

static void _dcompl(void) { next_dcompl(1); }

static void _rdcompl(void) { next_dcompl(-1); }

static void next_dcompl(int next) {
  static int col, row;
  static unsigned int len;
  static Str *d;
  int i, j, y;
  Str *f;
  char *p;
  struct stat st;
  int comment, nline;

  if (cm_mode == CPL_NEVER || cm_mode & CPL_OFF)
    return;
  cm_disp_clear = false;
  if (CurrentTab)
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
  if (LASTLINE >= 3) {
    comment = true;
    nline = LASTLINE - 2;
  } else if (LASTLINE) {
    comment = false;
    nline = LASTLINE;
  } else {
    return;
  }

  if (cm_disp_next >= 0) {
    if (next == 1) {
      cm_disp_next += col * nline;
      if (cm_disp_next >= NCFileBuf)
        cm_disp_next = 0;
    } else if (next == -1) {
      cm_disp_next -= col * nline;
      if (cm_disp_next < 0)
        cm_disp_next = 0;
    }
    row = (NCFileBuf - cm_disp_next + col - 1) / col;
    goto disp_next;
  }

  cm_next = false;
  next_compl(0);
  if (NCFileBuf == 0)
    return;
  cm_disp_next = 0;

  d = CDirBuf->Strdup();
  if (d->length > 0 && Strlastchar(d) != '/')
    Strcat_char(d, '/');
  if (cm_mode & CPL_URL && d->ptr[0] == 'f') {
    p = d->ptr;
    if (strncmp(p, "file://localhost/", 17) == 0)
      p = &p[16];
    else if (strncmp(p, "file:///", 8) == 0)
      p = &p[7];
    else if (strncmp(p, "file:/", 6) == 0 && p[6] != '/')
      p = &p[5];
    d = Strnew_charp(p);
  }

  len = 0;
  for (i = 0; i < NCFileBuf; i++) {
    auto n = strlen(CFileBuf[i]) + 3;
    if (len < n)
      len = n;
  }
  if (len > 0 && COLS > static_cast<int>(len))
    col = COLS / len;
  else
    col = 1;
  row = (NCFileBuf + col - 1) / col;

disp_next:
  if (comment) {
    if (row > nline) {
      row = nline;
      y = 0;
    } else
      y = nline - row + 1;
  } else {
    if (row >= nline) {
      row = nline;
      y = 0;
    } else
      y = nline - row - 1;
  }
  if (y) {
    move(y - 1, 0);
    clrtoeolx();
  }
  if (comment) {
    move(y, 0);
    clrtoeolx();
    bold();
    addstr("----- Completion list -----");
    boldend();
    y++;
  }
  for (i = 0; i < row; i++) {
    for (j = 0; j < col; j++) {
      auto n = cm_disp_next + j * row + i;
      if (n >= NCFileBuf)
        break;
      move(y, j * len);
      clrtoeolx();
      f = d->Strdup();
      Strcat_charp(f, CFileBuf[n]);
      addstr(CFileBuf[n]);
      if (stat(expandPath(f->ptr), &st) != -1 && S_ISDIR(st.st_mode))
        addstr("/");
    }
    y++;
  }
  if (comment && y == LASTLINE - 1) {
    move(y, 0);
    clrtoeolx();
    bold();
    if (emacs_like_lineedit)
      addstr("----- Press TAB to continue -----");
    else
      addstr("----- Press CTRL-D to continue -----");
    boldend();
  }
}

static Str *escape_spaces(Str *s) {
  Str *tmp = NULL;
  char *p;

  if (s == NULL)
    return s;
  for (p = s->ptr; *p; p++) {
    if (*p == ' ' || *p == CTRL_I) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(s->ptr, (int)(p - s->ptr));
      Strcat_char(tmp, '\\');
    }
    if (tmp)
      Strcat_char(tmp, *p);
  }
  if (tmp)
    return tmp;
  return s;
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

static Str *doComplete(Str *ifn, int *status, int next) {
  int fl, i;
  const char *fn, *p;
  DIR *d;
  Directory *dir;
  struct stat st;

  if (!cm_next) {
    NCFileBuf = 0;
    if (cm_mode & CPL_ON)
      ifn = unescape_spaces(ifn);
    CompleteBuf = ifn->Strdup();
    while (Strlastchar(CompleteBuf) != '/' && CompleteBuf->length > 0)
      Strshrink(CompleteBuf, 1);
    CDirBuf = CompleteBuf->Strdup();
    if (cm_mode & CPL_URL) {
      if (strncmp(CompleteBuf->ptr, "file://localhost/", 17) == 0)
        Strdelete(CompleteBuf, 0, 16);
      else if (strncmp(CompleteBuf->ptr, "file:///", 8) == 0)
        Strdelete(CompleteBuf, 0, 7);
      else if (strncmp(CompleteBuf->ptr, "file:/", 6) == 0 &&
               CompleteBuf->ptr[6] != '/')
        Strdelete(CompleteBuf, 0, 5);
      else {
        CompleteBuf = ifn->Strdup();
        *status = CPL_FAIL;
        return CompleteBuf;
      }
    }
    if (CompleteBuf->length == 0) {
      Strcat_char(CompleteBuf, '.');
    }
    if (Strlastchar(CompleteBuf) == '/' && CompleteBuf->length > 1) {
      Strshrink(CompleteBuf, 1);
    }
    if ((d = opendir(expandPath(CompleteBuf->ptr))) == NULL) {
      CompleteBuf = ifn->Strdup();
      *status = CPL_FAIL;
      if (cm_mode & CPL_ON)
        CompleteBuf = escape_spaces(CompleteBuf);
      return CompleteBuf;
    }
    fn = lastFileName(ifn->ptr);
    fl = strlen(fn);
    CFileName = Strnew();
    for (;;) {
      dir = readdir(d);
      if (dir == NULL)
        break;
      if (fl == 0 && (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")))
        continue;
      if (!strncmp(dir->d_name, fn, fl)) { /* match */
        NCFileBuf++;
        CFileBuf = (char **)New_Reuse(char *, CFileBuf, NCFileBuf);
        CFileBuf[NCFileBuf - 1] =
            (char *)NewAtom_N(char, strlen(dir->d_name) + 1);
        strcpy(CFileBuf[NCFileBuf - 1], dir->d_name);
        if (NCFileBuf == 1) {
          CFileName = Strnew_charp(dir->d_name);
        } else {
          for (i = 0; CFileName->ptr[i] == dir->d_name[i]; i++)
            ;
          Strtruncate(CFileName, i);
        }
      }
    }
    closedir(d);
    if (NCFileBuf == 0) {
      CompleteBuf = ifn->Strdup();
      *status = CPL_FAIL;
      if (cm_mode & CPL_ON)
        CompleteBuf = escape_spaces(CompleteBuf);
      return CompleteBuf;
    }
    qsort(CFileBuf, NCFileBuf, sizeof(CFileBuf[0]), strCmp);
    NCFileOffset = 0;
    if (NCFileBuf >= 2) {
      cm_next = true;
      *status = CPL_AMBIG;
    } else {
      *status = CPL_OK;
    }
  } else {
    CFileName = Strnew_charp(CFileBuf[NCFileOffset]);
    NCFileOffset = (NCFileOffset + next + NCFileBuf) % NCFileBuf;
    *status = CPL_MENU;
  }
  CompleteBuf = CDirBuf->Strdup();
  if (CompleteBuf->length && Strlastchar(CompleteBuf) != '/')
    Strcat_char(CompleteBuf, '/');
  Strcat(CompleteBuf, CFileName);
  if (*status != CPL_AMBIG) {
    p = CompleteBuf->ptr;
    if (cm_mode & CPL_URL) {
      if (strncmp(p, "file://localhost/", 17) == 0)
        p = &p[16];
      else if (strncmp(p, "file:///", 8) == 0)
        p = &p[7];
      else if (strncmp(p, "file:/", 6) == 0 && p[6] != '/')
        p = &p[5];
    }
    if (stat(expandPath((char *)p), &st) != -1 && S_ISDIR(st.st_mode))
      Strcat_char(CompleteBuf, '/');
  }
  if (cm_mode & CPL_ON)
    CompleteBuf = escape_spaces(CompleteBuf);
  return CompleteBuf;
}

static void _prev(void) {
  Hist *hist = CurrentHist;
  const char *p;

  if (!use_hist)
    return;
  if (strCurrentBuf) {
    p = prevHist(hist);
    if (p == NULL)
      return;
  } else {
    p = lastHist(hist);
    if (p == NULL)
      return;
    strCurrentBuf = strBuf;
  }
  if (DecodeURL && (cm_mode & CPL_URL))
    p = url_decode0(p);
  strBuf = Strnew_charp(p);
  CLen = CPos = setStrType(strBuf, strProp);
  offset = 0;
}

static void _next(void) {
  Hist *hist = CurrentHist;
  const char *p;

  if (!use_hist)
    return;
  if (strCurrentBuf == NULL)
    return;
  p = nextHist(hist);
  if (p) {
    if (DecodeURL && (cm_mode & CPL_URL))
      p = url_decode0(p);
    strBuf = Strnew_charp(p);
  } else {
    strBuf = strCurrentBuf;
    strCurrentBuf = NULL;
  }
  CLen = CPos = setStrType(strBuf, strProp);
  offset = 0;
}

static int setStrType(Str *str, Lineprop *prop) {
  Lineprop ctype;
  char *p = str->ptr, *ep = p + str->length;
  int i, len = 1;

  for (i = 0; p < ep;) {
    if (i + len > STR_LEN)
      break;
    ctype = get_mctype(p);
    if (is_passwd) {
      if (ctype & PC_CTRL)
        ctype = PC_ASCII;
    }
    prop[i++] = ctype;
    p++;
  }
  return i;
}

static int terminated(unsigned char c) {
  int termchar[] = {'/', '&', '?', ' ', -1};
  int *tp;

  for (tp = termchar; *tp > 0; tp++) {
    if (c == *tp) {
      return 1;
    }
  }

  return 0;
}

static void _editor(void) {
  FormItemList fi;
  char *p;

  if (is_passwd)
    return;

  fi.readonly = false;
  fi.value = strBuf->Strdup();
  Strcat_char(fi.value, '\n');

  input_textarea(&fi);

  strBuf = Strnew();
  for (p = fi.value->ptr; *p; p++) {
    if (*p == '\r' || *p == '\n')
      continue;
    Strcat_char(strBuf, *p);
  }
  CLen = CPos = setStrType(strBuf, strProp);
  if (CurrentTab)
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

inline void inputChar(const char *p, const OnInput &onInput) {
  inputLine(p, "", IN_CHAR, onInput);
}

void inputAnswer(const char *prompt, const OnInput &onInput) {
  if (IsForkChild) {
    onInput("n");
    return;
  }

  if (!fmInitialized) {
    printf("%s", prompt);
    fflush(stdout);
    onInput(Strfgets(stdin)->ptr);
    return;
  }

  term_raw();
  inputChar(prompt, onInput);
}
