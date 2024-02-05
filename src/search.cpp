#include "search.h"
#include "Str.h"
#include "app.h"
#include "terms.h"
#include "signal_util.h"
#include "w3m.h"
#include "message.h"
#include "display.h"
#include "buffer.h"
#include "regex.h"
#include "proto.h"
#include "tabbuffer.h"
#include <unistd.h>

int forwardSearch(Buffer *buf, const char *str) {
  const char *p, *first, *last;
  Line *l, *begin;
  int wrapped = false;

  if ((p = regexCompile(str, IgnoreCase)) != NULL) {
    message(p, 0, 0);
    return SR_NOTFOUND;
  }
  l = buf->currentLine;
  if (l == NULL) {
    return SR_NOTFOUND;
  }

  auto pos = buf->pos;
  if (l->bpos) {
    pos += l->bpos;
    while (l->bpos && l->prev)
      l = l->prev;
  }
  begin = l;
  if (pos < l->size() &&
      regexMatch(&l->lineBuf[pos], l->size() - pos, 0) == 1) {
    matchedPosition(&first, &last);
    pos = first - l->lineBuf.data();
    while (pos >= l->len && l->next && l->next->bpos) {
      pos -= l->len;
      l = l->next;
    }
    buf->pos = pos;
    if (l != buf->currentLine)
      gotoLine(buf, l->linenumber);
    arrangeCursor(buf);
    l->set_mark(pos, pos + last - first);
    return SR_FOUND;
  }
  for (l = l->next;; l = l->next) {
    if (l == NULL) {
      if (WrapSearch) {
        l = buf->firstLine;
        wrapped = true;
      } else {
        break;
      }
    }
    if (l->bpos)
      continue;
    if (regexMatch(l->lineBuf.data(), l->size(), 1) == 1) {
      matchedPosition(&first, &last);
      pos = first - l->lineBuf.data();
      while (pos >= l->len && l->next && l->next->bpos) {
        pos -= l->len;
        l = l->next;
      }
      buf->pos = pos;
      buf->currentLine = l;
      gotoLine(buf, l->linenumber);
      arrangeCursor(buf);
      l->set_mark(pos, pos + last - first);
      return SR_FOUND | (wrapped ? SR_WRAPPED : 0);
    }
    if (wrapped && l == begin) /* no match */
      break;
  }
  return SR_NOTFOUND;
}

int backwardSearch(Buffer *buf, const char *str) {
  const char *p, *q, *found, *found_last, *first, *last;
  Line *l, *begin;
  int wrapped = false;
  int pos;

  if ((p = regexCompile(str, IgnoreCase)) != NULL) {
    message(p, 0, 0);
    return SR_NOTFOUND;
  }
  l = buf->currentLine;
  if (l == NULL) {
    return SR_NOTFOUND;
  }
  pos = buf->pos;
  if (l->bpos) {
    pos += l->bpos;
    while (l->bpos && l->prev)
      l = l->prev;
  }
  begin = l;
  if (pos > 0) {
    pos--;
    p = &l->lineBuf[pos];
    found = NULL;
    found_last = NULL;
    q = l->lineBuf.data();
    while (regexMatch(q, &l->lineBuf[l->size()] - q, q == l->lineBuf.data()) ==
           1) {
      matchedPosition(&first, &last);
      if (first <= p) {
        found = first;
        found_last = last;
      }
      if (q - l->lineBuf.data() >= l->size())
        break;
      q++;
      if (q > p)
        break;
    }
    if (found) {
      pos = found - l->lineBuf.data();
      while (pos >= l->len && l->next && l->next->bpos) {
        pos -= l->len;
        l = l->next;
      }
      buf->pos = pos;
      if (l != buf->currentLine)
        gotoLine(buf, l->linenumber);
      arrangeCursor(buf);
      l->set_mark(pos, pos + found_last - found);
      return SR_FOUND;
    }
  }
  for (l = l->prev;; l = l->prev) {
    if (l == NULL) {
      if (WrapSearch) {
        l = buf->lastLine;
        wrapped = true;
      } else {
        break;
      }
    }
    found = NULL;
    found_last = NULL;
    q = l->lineBuf.data();
    while (regexMatch(q, &l->lineBuf[l->size()] - q, q == l->lineBuf.data()) ==
           1) {
      matchedPosition(&first, &last);
      found = first;
      found_last = last;
      if (q - l->lineBuf.data() >= l->size())
        break;
      q++;
    }
    if (found) {
      pos = found - l->lineBuf.data();
      while (pos >= l->len && l->next && l->next->bpos) {
        pos -= l->len;
        l = l->next;
      }
      buf->pos = pos;
      gotoLine(buf, l->linenumber);
      arrangeCursor(buf);
      l->set_mark(pos, pos + found_last - found);
      return SR_FOUND | (wrapped ? SR_WRAPPED : 0);
    }
    if (wrapped && l == begin) /* no match */
      break;
  }
  return SR_NOTFOUND;
}

JMP_BUF IntReturn;
static void intTrap(SIGNAL_ARG) { /* Interrupt catcher */
  LONGJMP(IntReturn, 0);
  SIGNAL_RETURN;
}

static const char *SearchString = nullptr;
int (*searchRoutine)(Buffer *, const char *);

static void clear_mark(Line *l) {
  int pos;
  if (!l)
    return;
  for (pos = 0; pos < l->size(); pos++)
    l->propBuf[pos] &= ~PE_MARK;
}

/* search by regular expression */
int srchcore(const char *str, int (*func)(Buffer *, const char *)) {
  MySignalHandler prevtrap = {};
  int i, result = SR_NOTFOUND;

  if (str != nullptr && str != SearchString)
    SearchString = str;
  if (SearchString == nullptr || *SearchString == '\0')
    return SR_NOTFOUND;

  str = SearchString;
  prevtrap = mySignal(SIGINT, intTrap);
  crmode();
  if (SETJMP(IntReturn) == 0) {
    for (i = 0; i < PREC_NUM; i++) {
      result = func(Currentbuf, str);
      if (i < PREC_NUM - 1 && result & SR_FOUND)
        clear_mark(Currentbuf->currentLine);
    }
  }
  mySignal(SIGINT, prevtrap);
  term_raw();
  return result;
}

static void disp_srchresult(int result, const char *prompt, const char *str) {
  if (str == nullptr)
    str = "";
  if (result & SR_NOTFOUND)
    disp_message(Sprintf("Not found: %s", str)->ptr, true);
  else if (result & SR_WRAPPED)
    disp_message(Sprintf("Search wrapped: %s", str)->ptr, true);
  else if (show_srch_str)
    disp_message(Sprintf("%s%s", prompt, str)->ptr, true);
}

/* Search regular expression forward */

void srch_nxtprv(int reverse) {
  int result;
  /* *INDENT-OFF* */
  static int (*routine[2])(Buffer *, const char *) = {forwardSearch,
                                                      backwardSearch};
  /* *INDENT-ON* */

  if (searchRoutine == nullptr) {
    disp_message("No previous regular expression", true);
    return;
  }
  if (reverse != 0)
    reverse = 1;
  if (searchRoutine == backwardSearch)
    reverse ^= 1;
  if (reverse == 0)
    Currentbuf->pos += 1;
  result = srchcore(SearchString, routine[reverse]);
  if (result & SR_FOUND)
    clear_mark(Currentbuf->currentLine);
  else {
    if (reverse == 0)
      Currentbuf->pos -= 1;
  }
  displayBuffer(Currentbuf, B_NORMAL);
  disp_srchresult(result, (reverse ? "Backward: " : "Forward: "), SearchString);
}

static int dispincsrch(int ch, Str *buf, Lineprop *prop) {
  bool do_next_search = false;

  static Buffer *sbuf = new Buffer(0);
  if (ch == 0 && buf == nullptr) {
    SAVE_BUFPOSITION(sbuf); /* search starting point */
    return -1;
  }

  auto str = buf->ptr;
  switch (ch) {
  case 022: /* C-r */
    searchRoutine = backwardSearch;
    do_next_search = true;
    break;
  case 023: /* C-s */
    searchRoutine = forwardSearch;
    do_next_search = true;
    break;

  default:
    if (ch >= 0)
      return ch; /* use InputKeymap */
  }

  if (do_next_search) {
    if (*str) {
      if (searchRoutine == forwardSearch)
        Currentbuf->pos += 1;
      SAVE_BUFPOSITION(sbuf);
      if (srchcore(str, searchRoutine) == SR_NOTFOUND &&
          searchRoutine == forwardSearch) {
        Currentbuf->pos -= 1;
        SAVE_BUFPOSITION(sbuf);
      }
      arrangeCursor(Currentbuf);
      displayBuffer(Currentbuf, B_FORCE_REDRAW);
      clear_mark(Currentbuf->currentLine);
      return -1;
    } else
      return 020; /* _prev completion for C-s C-s */
  } else if (*str) {
    RESTORE_BUFPOSITION(sbuf);
    arrangeCursor(Currentbuf);
    srchcore(str, searchRoutine);
    arrangeCursor(Currentbuf);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
  clear_mark(Currentbuf->currentLine);
  return -1;
}

void isrch(int (*func)(Buffer *, const char *), const char *prompt) {
  auto sbuf = new Buffer(0);
  SAVE_BUFPOSITION(sbuf);
  dispincsrch(0, nullptr, nullptr); /* initialize incremental search state */

  searchRoutine = func;
  // auto str =
  //     inputLineHistSearch(prompt, nullptr, IN_STRING, TextHist, dispincsrch);
  // if (str == nullptr) {
  //   RESTORE_BUFPOSITION(&sbuf);
  // }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void srch(int (*func)(Buffer *, const char *), const char *prompt) {
  const char *str;
  int result;
  int disp = false;
  int pos;

  str = searchKeyData();
  if (str == nullptr || *str == '\0') {
    // str = inputStrHist(prompt, nullptr, TextHist);
    if (str != nullptr && *str == '\0')
      str = SearchString;
    if (str == nullptr) {
      displayBuffer(Currentbuf, B_NORMAL);
      return;
    }
    disp = true;
  }
  pos = Currentbuf->pos;
  if (func == forwardSearch)
    Currentbuf->pos += 1;
  result = srchcore(str, func);
  if (result & SR_FOUND)
    clear_mark(Currentbuf->currentLine);
  else
    Currentbuf->pos = pos;
  displayBuffer(Currentbuf, B_NORMAL);
  if (disp)
    disp_srchresult(result, prompt, str);
  searchRoutine = func;
}
