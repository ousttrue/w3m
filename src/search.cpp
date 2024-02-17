#include "search.h"
#include "buffer.h"
#include "line_layout.h"
#include "Str.h"
#include "app.h"
#include "terms.h"
#include "w3m.h"
#include "message.h"
#include "display.h"
#include "regex.h"
#include "proto.h"
#include "tabbuffer.h"

int forwardSearch(LineLayout *layout, const char *str) {
  const char *p, *first, *last;
  Line *l, *begin;
  int wrapped = false;

  if ((p = regexCompile(str, IgnoreCase)) != NULL) {
    message(p, 0, 0);
    return SR_NOTFOUND;
  }
  l = layout->currentLine;
  if (l == NULL) {
    return SR_NOTFOUND;
  }

  auto pos = layout->pos;
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
    layout->pos = pos;
    if (l != layout->currentLine) {
      layout->gotoLine(l->linenumber);
    }
    layout->arrangeCursor();
    l->set_mark(pos, pos + last - first);
    return SR_FOUND;
  }
  for (l = l->next;; l = l->next) {
    if (l == NULL) {
      if (WrapSearch) {
        l = layout->firstLine;
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
      layout->pos = pos;
      layout->currentLine = l;
      layout->gotoLine(l->linenumber);
      layout->arrangeCursor();
      l->set_mark(pos, pos + last - first);
      return SR_FOUND | (wrapped ? SR_WRAPPED : 0);
    }
    if (wrapped && l == begin) /* no match */
      break;
  }
  return SR_NOTFOUND;
}

int backwardSearch(LineLayout *layout, const char *str) {
  const char *p, *q, *found, *found_last, *first, *last;
  Line *l, *begin;
  int wrapped = false;
  int pos;

  if ((p = regexCompile(str, IgnoreCase)) != NULL) {
    message(p, 0, 0);
    return SR_NOTFOUND;
  }
  l = layout->currentLine;
  if (l == NULL) {
    return SR_NOTFOUND;
  }
  pos = layout->pos;
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
      layout->pos = pos;
      if (l != layout->currentLine) {
        layout->gotoLine(l->linenumber);
      }
      layout->arrangeCursor();
      l->set_mark(pos, pos + found_last - found);
      return SR_FOUND;
    }
  }
  for (l = l->prev;; l = l->prev) {
    if (l == NULL) {
      if (WrapSearch) {
        l = layout->lastLine;
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
      layout->pos = pos;
      layout->gotoLine(l->linenumber);
      layout->arrangeCursor();
      l->set_mark(pos, pos + found_last - found);
      return SR_FOUND | (wrapped ? SR_WRAPPED : 0);
    }
    if (wrapped && l == begin) /* no match */
      break;
  }
  return SR_NOTFOUND;
}

static const char *SearchString = nullptr;
int (*searchRoutine)(LineLayout *, const char *);

static void clear_mark(Line *l) {
  int pos;
  if (!l)
    return;
  for (pos = 0; pos < l->size(); pos++)
    l->propBuf[pos] &= ~PE_MARK;
}

/* search by regular expression */
int srchcore(const char *str, int (*func)(LineLayout *, const char *)) {
  int result = SR_NOTFOUND;

  if (str != nullptr && str != SearchString)
    SearchString = str;
  if (SearchString == nullptr || *SearchString == '\0')
    return SR_NOTFOUND;

  str = SearchString;
  crmode();
  // if (SETJMP(IntReturn) == 0) {
  //   for (i = 0; i < PREC_NUM; i++) {
  //     result = func(&CurrentTab->currentBuffer()->layout, str);
  //     if (i < PREC_NUM - 1 && result & SR_FOUND)
  //       clear_mark(CurrentTab->currentBuffer()->layout.currentLine);
  //   }
  // }
  // mySignal(SIGINT, prevtrap);
  term_raw();
  return result;
}

static void disp_srchresult(int result, const char *prompt, const char *str) {
  if (str == nullptr)
    str = "";
  if (result & SR_NOTFOUND)
    disp_message(Sprintf("Not found: %s", str)->ptr);
  else if (result & SR_WRAPPED)
    disp_message(Sprintf("Search wrapped: %s", str)->ptr);
  else if (show_srch_str)
    disp_message(Sprintf("%s%s", prompt, str)->ptr);
}

/* Search regular expression forward */

void srch_nxtprv(int reverse) {
  int result;
  /* *INDENT-OFF* */
  static int (*routine[2])(LineLayout *, const char *) = {forwardSearch,
                                                          backwardSearch};
  /* *INDENT-ON* */

  if (searchRoutine == nullptr) {
    disp_message("No previous regular expression");
    return;
  }
  if (reverse != 0) {
    reverse = 1;
  }
  if (searchRoutine == backwardSearch) {
    reverse ^= 1;
  }
  if (reverse == 0) {
    CurrentTab->currentBuffer()->layout.pos += 1;
  }
  result = srchcore(SearchString, routine[reverse]);
  if (result & SR_FOUND) {
    clear_mark(CurrentTab->currentBuffer()->layout.currentLine);
  } else {
    if (reverse == 0) {
      CurrentTab->currentBuffer()->layout.pos -= 1;
    }
  }
  App::instance().invalidate();
  disp_srchresult(result, (reverse ? "Backward: " : "Forward: "), SearchString);
}

static int dispincsrch(int ch, Str *buf, Lineprop *prop) {

  static LineLayout sbuf(0);
  if (ch == 0 && !buf) {
    sbuf.COPY_BUFROOT_FROM(
        CurrentTab->currentBuffer()->layout); /* search starting point */
    return -1;
  }

  bool do_next_search = false;
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
        CurrentTab->currentBuffer()->layout.pos += 1;
      sbuf.COPY_BUFROOT_FROM(CurrentTab->currentBuffer()->layout);
      if (srchcore(str, searchRoutine) == SR_NOTFOUND &&
          searchRoutine == forwardSearch) {
        CurrentTab->currentBuffer()->layout.pos -= 1;
        sbuf.COPY_BUFROOT_FROM(CurrentTab->currentBuffer()->layout);
      }
      CurrentTab->currentBuffer()->layout.arrangeCursor();
      App::instance().invalidate();
      clear_mark(CurrentTab->currentBuffer()->layout.currentLine);
      return -1;
    } else
      return 020; /* _prev completion for C-s C-s */
  } else if (*str) {
    CurrentTab->currentBuffer()->layout.COPY_BUFROOT_FROM(sbuf);
    CurrentTab->currentBuffer()->layout.arrangeCursor();
    srchcore(str, searchRoutine);
    CurrentTab->currentBuffer()->layout.arrangeCursor();
  }
  App::instance().invalidate();
  clear_mark(CurrentTab->currentBuffer()->layout.currentLine);
  return -1;
}

void isrch(int (*func)(LineLayout *, const char *), const char *prompt) {
  LineLayout sbuf(0);
  sbuf.COPY_BUFROOT_FROM(CurrentTab->currentBuffer()->layout);
  dispincsrch(0, nullptr, nullptr); /* initialize incremental search state */

  searchRoutine = func;
  // auto str =
  //     inputLineHistSearch(prompt, nullptr, IN_STRING, TextHist, dispincsrch);
  // if (str == nullptr) {
  //   RESTORE_BUFPOSITION(&sbuf);
  // }
  App::instance().invalidate();
}

void srch(int (*func)(LineLayout *, const char *), const char *prompt) {
  int result;
  int disp = false;
  int pos;

  auto str = App::instance().searchKeyData();
  if (str == nullptr || *str == '\0') {
    // str = inputStrHist(prompt, nullptr, TextHist);
    if (str != nullptr && *str == '\0')
      str = SearchString;
    if (str == nullptr) {
      App::instance().invalidate();
      return;
    }
    disp = true;
  }
  pos = CurrentTab->currentBuffer()->layout.pos;
  if (func == forwardSearch)
    CurrentTab->currentBuffer()->layout.pos += 1;
  result = srchcore(str, func);
  if (result & SR_FOUND)
    clear_mark(CurrentTab->currentBuffer()->layout.currentLine);
  else
    CurrentTab->currentBuffer()->layout.pos = pos;
  App::instance().invalidate();
  if (disp)
    disp_srchresult(result, prompt, str);
  searchRoutine = func;
}
