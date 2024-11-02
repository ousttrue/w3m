#include "mainloop.h"
#include "alloc.h"
#include "buffer/buffer.h"
#include "buffer/display.h"
#include "buffer/document.h"
#include "buffer/downloadlist.h"
#include "buffer/tabbuffer.h"
#include "core.h"
#include "func.h"
#include "funcname1.h"
#include "html/form.h"
#include "input/localcgi.h"
#include "linein.h"
#include "term/terms.h"
#include "term/termsize.h"
#include "term/tty.h"
#include "text/myctype.h"

bool vi_prec_num = false;
int CurrentKey = -1;
const char *CurrentKeyData = nullptr;
const char *CurrentCmdData = nullptr;
int prec_num = 0;
int prev_key = -1;
#define PREC_NUM (prec_num ? prec_num : 1)
#define PREC_LIMIT 10000

static struct Current makeCurrent(){
  struct Current current = {
      CurrentTab,
  };
  return current;
}

void clearKeyData() { CurrentKeyData = NULL; /* not allowed in w3m-control: */ }

const char *searchKeyData(void) {
  const char *data = NULL;
  if (CurrentKeyData != NULL && *CurrentKeyData != '\0')
    data = CurrentKeyData;
  else if (CurrentCmdData != NULL && *CurrentCmdData != '\0')
    data = CurrentCmdData;
  else if (CurrentKey >= 0)
    data = getKeyData(CurrentKey);
  CurrentKeyData = NULL;
  CurrentCmdData = NULL;
  if (data == NULL || *data == '\0')
    return NULL;
  return allocStr(data, -1);
}

int getHseq(int nmark) {
  int hseq = 0;
  if (prec_num > nmark)
    hseq = nmark - 1;
  else if (prec_num > 0)
    hseq = prec_num - 1;
  return hseq;
}

int precNum() { return PREC_NUM; }

int getLastHseq(int nmark) {
  int hseq;
  if (prec_num >= nmark)
    hseq = 0;
  else if (prec_num > 0)
    hseq = nmark - prec_num;
  else
    hseq = nmark - 1;
  return hseq;
}

const char *goLineStr() {
  const char *str = searchKeyData();
  if (prec_num)
    return "^";
  else if (str)
    return str;
  else
    return inputStr(Currentbuf->document, "Goto line: ", "");
}

int searchKeyNum() {
  int n = 1;
  const char *d = searchKeyData();
  if (d != NULL)
    n = atoi(d);
  return n * PREC_NUM;
}

int scrollNum() {
  if (vi_prec_num) {
    return searchKeyNum() * (Currentbuf->document->viewport.LINES - 1);
  } else if (prec_num) {
    return searchKeyNum();
  } else {
    return searchKeyNum() * (Currentbuf->document->viewport.LINES - 1);
  }
}

static void keyPressEventProc(int c) {
  CurrentKey = c;
  w3mFuncList[(int)GlobalKeymap[c]].func(makeCurrent());
}

void multiKeyProc() {
  char c = tty_getch();
  if (IS_ASCII(c)) {
    CurrentKey = K_MULTI | (CurrentKey << 16) | c;
    escKeyProc((int)c, 0, NULL);
  }
}

void escKeyProc(int c, int esc, unsigned char *map) {
  if (CurrentKey >= 0 && CurrentKey & K_MULTI) {
    unsigned char **mmap;
    mmap = (unsigned char **)getKeyData(MULTI_KEY(CurrentKey));
    if (!mmap)
      return;
    switch (esc) {
    case K_ESCD:
      map = mmap[3];
      break;
    case K_ESCB:
      map = mmap[2];
      break;
    case K_ESC:
      map = mmap[1];
      break;
    default:
      map = mmap[0];
      break;
    }
    esc |= (CurrentKey & ~0xFFFF);
  }
  CurrentKey = esc | c;
  struct Current current;
  w3mFuncList[(int)map[c]].func(current);
}

void set_buffer_environ(struct Buffer *buf) {
  static struct Buffer *prev_buf = NULL;
  static struct Line *prev_line = NULL;
  static int prev_pos = -1;

  if (buf == NULL)
    return;

  if (buf != prev_buf) {
    set_environ("W3M_SOURCEFILE", buf->document->sourcefile);
    set_environ("W3M_FILENAME", buf->document->filename);
    set_environ("W3M_TITLE", buf->buffername);
    set_environ("W3M_URL", parsedURL2Str(&buf->document->url)->ptr);
    set_environ("W3M_TYPE",
                buf->document->type ? buf->document->type : "unknown");
  }
  auto l = buf->document->currentLine;
  if (l && (buf != prev_buf || l != prev_line ||
            buf->document->viewport.pos != prev_pos)) {
    char *s = GetWord(buf->document);
    set_environ("W3M_CURRENT_WORD", s ? s : "");
    auto a = retrieveCurrentAnchor(buf->document);
    if (a) {
      auto pu = parseURL2(a->url, baseURL(buf->document));
      set_environ("W3M_CURRENT_LINK", parsedURL2Str(&pu)->ptr);
    } else
      set_environ("W3M_CURRENT_LINK", "");
    a = retrieveCurrentImg(buf->document);
    if (a) {
      auto pu = parseURL2(a->url, baseURL(buf->document));
      set_environ("W3M_CURRENT_IMG", parsedURL2Str(&pu)->ptr);
    } else
      set_environ("W3M_CURRENT_IMG", "");
    a = retrieveCurrentForm(buf->document);
    if (a)
      set_environ("W3M_CURRENT_FORM", form2str((struct FormItemList *)a->url));
    else
      set_environ("W3M_CURRENT_FORM", "");
    set_environ("W3M_CURRENT_LINE", Sprintf("%ld", l->real_linenumber)->ptr);
    set_environ("W3M_CURRENT_COLUMN",
                Sprintf("%d", buf->document->viewport.currentColumn +
                                  buf->document->viewport.cursorX + 1)
                    ->ptr);
  } else if (!l) {
    set_environ("W3M_CURRENT_WORD", "");
    set_environ("W3M_CURRENT_LINK", "");
    set_environ("W3M_CURRENT_IMG", "");
    set_environ("W3M_CURRENT_FORM", "");
    set_environ("W3M_CURRENT_LINE", "0");
    set_environ("W3M_CURRENT_COLUMN", "0");
  }
  prev_buf = buf;
  prev_line = l;
  prev_pos = buf->document->viewport.pos;
}

void mainloop() {
  for (;;) {
    download_update(makeCurrent());
    if (Currentbuf->document->submit) {
      struct Anchor *a = Currentbuf->document->submit;
      Currentbuf->document->submit = NULL;
      gotoLine(Currentbuf->document, a->start.line);
      Currentbuf->document->viewport.pos = a->start.pos;
      auto buf = _followForm(Currentbuf->document, true, makeCurrent());
      pushBuffer(CurrentTab, buf);
      continue;
    }
    /* event processing */
    if (CurrentEvent) {
      CurrentKey = -1;
      CurrentKeyData = NULL;
      CurrentCmdData = (char *)CurrentEvent->data;
      w3mFuncList[CurrentEvent->cmd].func(makeCurrent());
      CurrentCmdData = NULL;
      CurrentEvent = CurrentEvent->next;
      continue;
    }
#ifndef _WIN32
    /* get keypress event */
    mySignal(SIGWINCH, resize_hook);
#endif
    {
      // do {
      resize_screen_if_updated();
      // }
      // while (tty_sleep_till_anykey(1, 0) <= 0);
    }
    char c = tty_getch();
    if (c && IS_ASCII(c)) { /* Ascii */
      if (('0' <= c) && (c <= '9') &&
          (prec_num || (GlobalKeymap[c] == FUNCNAME_nulcmd))) {
        prec_num = prec_num * 10 + (int)(c - '0');
        if (prec_num > PREC_LIMIT)
          prec_num = PREC_LIMIT;
      } else {
        set_buffer_environ(Currentbuf);
        save_buffer_position(Currentbuf->document);
        keyPressEventProc((int)c);
        prec_num = 0;
      }
    }
    prev_key = CurrentKey;
    CurrentKey = -1;
    CurrentKeyData = NULL;
    display(Currentbuf->document);
    term_refresh();
  }
}
