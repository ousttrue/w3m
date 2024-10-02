#include "linein.h"
#include "utf8.h"
#include "alloc.h"
#include "display.h"
#include "filepath.h"
#include "ctrlcode.h"
#include "history.h"
#include "indep.h"
#include "form.h"
#include "tabbuffer.h"
#include "terms.h"
#include "termsize.h"
#include "tty.h"
#include "scr.h"
#include "localcgi.h"
#include "fm.h"
#include <dirent.h>

#define STR_LEN 1024
#define CLEN (COLS - 2)

static Str strBuf;
static Lineprop strProp[STR_LEN];

static Str CompleteBuf;
static Str CFileName;
static Str CBeforeBuf;
static Str CAfterBuf;
static Str CDirBuf;
static char **CFileBuf = NULL;
static int NCFileBuf;
static int NCFileOffset;

typedef void (*InputKeymapFunc)(int);
static void insertself(int c);
static void _mvR(int);
static void _mvL(int);
static void _mvRw(int);
static void _mvLw(int);
static void delC(int);
static void insC(void);
static void _mvB(int);
static void _mvE(int);
static void _enter(int);
static void _quo(int);
static void _bs(int);
static void _bsw(int);
static void killn(int);
static void killb(int);
static void _inbrk(int);
static void _esc(int);
static void _editor(int);
static void _prev(int);
static void _next(int);
static void _compl(int);
static void _tcompl(int);
static void _dcompl(void);
static void _rdcompl(void);
static void _rcompl(void);

static int terminated(unsigned char c);
#define iself insertself

static void next_compl(int next);
static void next_dcompl(int next);
static Str doComplete(Str ifn, int *status, int next);

InputKeymapFunc InputKeymap[32] = {
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

static int setStrType(Str str, Lineprop *prop);
static void addPasswd(char *p, Lineprop *pr, int len, int pos, int limit);
static void addStr(char *p, Lineprop *pr, int len, int pos, int limit);

static int CPos, CLen, offset;
static int i_cont, i_broken, i_quote;
static int cm_mode, cm_next, cm_clear, cm_disp_next, cm_disp_clear;
static int need_redraw, is_passwd;
static int move_word;

static struct Hist *CurrentHist;
static Str strCurrentBuf;
static int use_hist;
static void ins_char(char c);

const char *inputLineHistSearch(const char *prompt, const char *def_str,
                                enum InputlineFlags flag, struct Hist *hist,
                                IncFunc incrfunc) {
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
  opos = utf8str_width((const uint8_t *)prompt);
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
    x = calcPosition(strBuf->ptr, strProp, CLen, CPos, 0, CP_FORCE);
    if (x - rpos > offset) {
      y = calcPosition(strBuf->ptr, strProp, CLen, CLen, 0, CP_AUTO);
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
    scr_move(LASTLINE, 0);
    scr_addstr(prompt);
    if (is_passwd)
      addPasswd(strBuf->ptr, strProp, CLen, offset, COLS - opos);
    else
      addStr(strBuf->ptr, strProp, CLen, offset, COLS - opos);
    scr_clrtoeolx();
    scr_move(LASTLINE, opos + x - offset);
    term_refresh();

  next_char:
    c = tty_getch();
    cm_clear = true;
    cm_disp_clear = true;
    if (!i_quote && (((cm_mode & CPL_ALWAYS) &&
                      (c == CTRL_I || (space_autocomplete && c == ' '))) ||
                     ((cm_mode & CPL_ON) && (c == CTRL_I)))) {
      if (emacs_like_lineedit && cm_next) {
        _dcompl();
        need_redraw = true;
      } else {
        _compl(-1);
        cm_disp_next = -1;
      }
    } else if (!i_quote && CLen == CPos &&
               (cm_mode & CPL_ALWAYS || cm_mode & CPL_ON) && c == CTRL_D) {
      if (!emacs_like_lineedit) {
        _dcompl();
        need_redraw = true;
      }
    } else if (!i_quote && c == DEL_CODE) {
      _bs(-1);
      cm_next = false;
      cm_disp_next = -1;
    } else if (!i_quote && c < 0x20) { /* Control code */
      if (incrfunc == NULL || (c = incrfunc((int)c, strBuf, strProp)) < 0x20)
        (*InputKeymap[(int)c])(c);
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
      if (!is_passwd && get_mctype(&c) == PC_CTRL)
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

  if (i_broken)
    return NULL;

  scr_move(LASTLINE, 0);
  term_refresh();
  p = strBuf->ptr;
  if (flag & (IN_FILENAME | IN_COMMAND)) {
    SKIP_BLANKS(p);
  }
  if (use_hist && !(flag & IN_URL) && *p != '\0') {
    char *q = lastHist(hist);
    if (!q || strcmp(q, p))
      pushHist(hist, p);
  }
  if (flag & IN_FILENAME)
    return expandPath(p);
  else
    return allocStr(p, -1);
}

static void addPasswd(char *p, Lineprop *pr, int len, int offset, int limit) {
  int rcol = 0, ncol;

  ncol = calcPosition(p, pr, len, len, 0, CP_AUTO);
  if (ncol > offset + limit)
    ncol = offset + limit;
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
      if (calcPosition(p, pr, len, i, 0, CP_AUTO) > offset)
        break;
    }
    if (i >= len)
      return;
    addChar('{', 0);
    rcol = offset + 1;
    ncol = calcPosition(p, pr, len, i, 0, CP_AUTO);
    for (; rcol < ncol; rcol++)
      addChar(' ', 0);
  }
  for (; i < len; i += delta) {
    ncol = calcPosition(p, pr, len, i + delta, 0, CP_AUTO);
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

static void _esc(int) {
  char c;

  switch (c = tty_getch()) {
  case '[':
  case 'O':
    switch (c = tty_getch()) {
    case 'A':
      _prev(-1);
      break;
    case 'B':
      _next(-1);
      break;
    case 'C':
      _mvR(-1);
      break;
    case 'D':
      _mvL(-1);
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
      _mvRw(-1);
    break;
  case 'b':
    if (emacs_like_lineedit)
      _mvLw(-1);
    break;
  case CTRL_H:
    if (emacs_like_lineedit)
      _bsw(-1);
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

static void delC(int) {
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

static void _mvL(int) {
  if (CPos > 0)
    CPos--;
}

static void _mvLw(int) {
  int first = 1;
  while (CPos > 0 && (first || !terminated(strBuf->ptr[CPos - 1]))) {
    CPos--;
    first = 0;
    if (!move_word)
      break;
  }
}

static void _mvRw(int) {
  int first = 1;
  while (CPos < CLen && (first || !terminated(strBuf->ptr[CPos - 1]))) {
    CPos++;
    first = 0;
    if (!move_word)
      break;
  }
}

static void _mvR(int) {
  if (CPos < CLen)
    CPos++;
}

static void _bs(int) {
  if (CPos > 0) {
    _mvL(-1);
    delC(-1);
  }
}

static void _bsw(int) {
  int t = 0;
  while (CPos > 0 && !t) {
    _mvL(-1);
    t = (move_word && terminated(strBuf->ptr[CPos - 1]));
    delC(-1);
  }
}

static void _enter(int) { i_cont = false; }

static void insertself(int c) {
  if (CLen >= STR_LEN)
    return;
  insC();
  strBuf->ptr[CPos] = c;
  strProp[CPos] = (is_passwd) ? PC_ASCII : PC_CTRL;
  CPos++;
}

static void _quo(int) { i_quote = true; }

static void _mvB(int) { CPos = 0; }

static void _mvE(int) { CPos = CLen; }

static void killn(int) {
  CLen = CPos;
  Strtruncate(strBuf, CLen);
}

static void killb(int) {
  while (CPos > 0)
    _bs(-1);
}

static void _inbrk(int) {
  i_cont = false;
  i_broken = true;
}

static void _compl(int) { next_compl(1); }

static void _rcompl(void) { next_compl(-1); }

static void _tcompl(int) {
  if (cm_mode & CPL_OFF)
    cm_mode = CPL_ON;
  else if (cm_mode & CPL_ON)
    cm_mode = CPL_OFF;
}

static void next_compl(int next) {
  int status;
  int b, a;
  Str buf;
  Str s;

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
    term_bell();
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
  static Str d;
  int i, j, n, y;
  Str f;
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

  d = Str_conv_to_system(Strdup(CDirBuf));
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
    n = strlen(CFileBuf[i]) + 3;
    if (len < n)
      len = n;
  }
  if (len > 0 && COLS > len)
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
    scr_move(y - 1, 0);
    scr_clrtoeolx();
  }
  if (comment) {
    scr_move(y, 0);
    scr_clrtoeolx();
    scr_bold();
    /* FIXME: gettextize? */
    scr_addstr("----- Completion list -----");
    scr_boldend();
    y++;
  }
  for (i = 0; i < row; i++) {
    for (j = 0; j < col; j++) {
      n = cm_disp_next + j * row + i;
      if (n >= NCFileBuf)
        break;
      scr_move(y, j * len);
      scr_clrtoeolx();
      f = Strdup(d);
      Strcat_charp(f, CFileBuf[n]);
      scr_addstr(conv_from_system(CFileBuf[n]));
      if (stat(expandPath(f->ptr), &st) != -1 && S_ISDIR(st.st_mode))
        scr_addstr("/");
    }
    y++;
  }
  if (comment && y == LASTLINE - 1) {
    scr_move(y, 0);
    scr_clrtoeolx();
    scr_bold();
    if (emacs_like_lineedit)
      /* FIXME: gettextize? */
      scr_addstr("----- Press TAB to continue -----");
    else
      /* FIXME: gettextize? */
      scr_addstr("----- Press CTRL-D to continue -----");
    scr_boldend();
  }
}

Str escape_spaces(Str s) {
  Str tmp = NULL;
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

Str unescape_spaces(Str s) {
  Str tmp = NULL;
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

static Str doComplete(Str ifn, int *status, int next) {
  int fl, i;
  const char *fn;
  char *p;
  DIR *d;
  struct stat st;

  if (!cm_next) {
    NCFileBuf = 0;
    ifn = Str_conv_to_system(ifn);
    if (cm_mode & CPL_ON)
      ifn = unescape_spaces(ifn);
    CompleteBuf = Strdup(ifn);
    while (Strlastchar(CompleteBuf) != '/' && CompleteBuf->length > 0)
      Strshrink(CompleteBuf, 1);
    CDirBuf = Strdup(CompleteBuf);
    if (cm_mode & CPL_URL) {
      if (strncmp(CompleteBuf->ptr, "file://localhost/", 17) == 0)
        Strdelete(CompleteBuf, 0, 16);
      else if (strncmp(CompleteBuf->ptr, "file:///", 8) == 0)
        Strdelete(CompleteBuf, 0, 7);
      else if (strncmp(CompleteBuf->ptr, "file:/", 6) == 0 &&
               CompleteBuf->ptr[6] != '/')
        Strdelete(CompleteBuf, 0, 5);
      else {
        CompleteBuf = Strdup(ifn);
        *status = CPL_FAIL;
        return Str_conv_to_system(CompleteBuf);
      }
    }
    if (CompleteBuf->length == 0) {
      Strcat_char(CompleteBuf, '.');
    }
    if (Strlastchar(CompleteBuf) == '/' && CompleteBuf->length > 1) {
      Strshrink(CompleteBuf, 1);
    }
    if ((d = opendir(expandPath(CompleteBuf->ptr))) == NULL) {
      CompleteBuf = Strdup(ifn);
      *status = CPL_FAIL;
      if (cm_mode & CPL_ON)
        CompleteBuf = escape_spaces(CompleteBuf);
      return CompleteBuf;
    }
    fn = lastFileName(ifn->ptr);
    fl = strlen(fn);
    CFileName = Strnew();
    for (;;) {
      auto dir = readdir(d);
      if (dir == NULL)
        break;
      if (fl == 0 && (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")))
        continue;
      if (!strncmp(dir->d_name, fn, fl)) { /* match */
        NCFileBuf++;
        CFileBuf = New_Reuse(char *, CFileBuf, NCFileBuf);
        CFileBuf[NCFileBuf - 1] = NewAtom_N(char, strlen(dir->d_name) + 1);
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
      CompleteBuf = Strdup(ifn);
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
  CompleteBuf = Strdup(CDirBuf);
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
    if (stat(expandPath(p), &st) != -1 && S_ISDIR(st.st_mode))
      Strcat_char(CompleteBuf, '/');
  }
  if (cm_mode & CPL_ON)
    CompleteBuf = escape_spaces(CompleteBuf);
  return Str_conv_from_system(CompleteBuf);
}

static void _prev(int) {
  if (!use_hist)
    return;

  struct Hist *hist = CurrentHist;
  char *p;
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
    p = url_decode2(p, NULL);
  strBuf = Strnew_charp(p);
  CLen = CPos = setStrType(strBuf, strProp);
  offset = 0;
}

static void _next(int) {
  if (!use_hist)
    return;

  struct Hist *hist = CurrentHist;
  char *p;
  if (strCurrentBuf == NULL)
    return;
  p = nextHist(hist);
  if (p) {
    if (DecodeURL && (cm_mode & CPL_URL))
      p = url_decode2(p, NULL);
    strBuf = Strnew_charp(p);
  } else {
    strBuf = strCurrentBuf;
    strCurrentBuf = NULL;
  }
  CLen = CPos = setStrType(strBuf, strProp);
  offset = 0;
}

static int setStrType(Str str, Lineprop *prop) {
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

static void _editor(int) {
  struct FormItemList fi;
  char *p;

  if (is_passwd)
    return;

  fi.readonly = false;
  fi.value = Strdup(strBuf);
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

const char *inputLineHist(const char *p, const char *d, enum InputlineFlags f,
                          struct Hist *h) {
  return inputLineHistSearch(p, d, f, h, NULL);
}

const char *inputLine(const char *p, const char *d, enum InputlineFlags f) {
  return inputLineHist(p, d, f, NULL);
}

const char *inputStr(const char *p, const char *d) {
  return inputLine(p, d, IN_STRING);
}
const char *inputStrHist(const char *p, const char *d, struct Hist *h) {
  return inputLineHist(p, d, IN_STRING, h);
}
const char *inputFilenameHist(const char *p, const char *d, struct Hist *h) {
  return inputLineHist(p, d, IN_FILENAME, h);
}
const char *inputChar(const char *p) { return inputLine(p, "", IN_CHAR); }
