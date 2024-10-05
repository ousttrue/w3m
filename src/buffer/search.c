#include "buffer/search.h"
#include "buffer/display.h"
#include "buffer/document.h"
#include "buffer/tabbuffer.h"
#include "defun.h"
#include "history.h"
#include "html/anchor.h"
#include "linein.h"
#include "term/scr.h"
#include "term/terms.h"
#include "term/tty.h"
#include "text/regex.h"
#include "trap_jmp.h"
#include <unistd.h>

static const char *SearchString = NULL;
SearchRoutine searchRoutine = nullptr;
bool IgnoreCase = true;
bool WrapSearch = false;
bool show_srch_str = true;

static void set_mark(struct Line *l, int pos, int epos) {
  for (; pos < epos && pos < l->size; pos++)
    l->propBuf[pos] |= PE_MARK;
}

enum SearchResult forwardSearch(struct Document *doc, const char *str) {
  char *p, *first, *last;
  struct Line *l, *begin;
  bool wrapped = false;
  int pos;

  if ((p = regexCompile((char *)str, IgnoreCase)) != NULL) {
    scr_message(p, 0, 0);
    return SR_NOTFOUND;
  }
  l = doc->currentLine;
  if (l == NULL) {
    return SR_NOTFOUND;
  }
  pos = doc->pos;
  if (l->bpos) {
    pos += l->bpos;
    while (l->bpos && l->prev)
      l = l->prev;
  }
  begin = l;
  if (pos < l->size && regexMatch(&l->lineBuf[pos], l->size - pos, 0) == 1) {
    matchedPosition(&first, &last);
    pos = first - l->lineBuf;
    while (pos >= l->len && l->next && l->next->bpos) {
      pos -= l->len;
      l = l->next;
    }
    doc->pos = pos;
    if (l != doc->currentLine)
      gotoLine(doc, l->linenumber);
    arrangeCursor(doc);
    set_mark(l, pos, pos + last - first);
    return SR_FOUND;
  }
  for (l = l->next;; l = l->next) {
    if (l == NULL) {
      if (WrapSearch) {
        l = doc->firstLine;
        wrapped = true;
      } else {
        break;
      }
    }
    if (l->bpos)
      continue;
    if (regexMatch(l->lineBuf, l->size, 1) == 1) {
      matchedPosition(&first, &last);
      pos = first - l->lineBuf;
      while (pos >= l->len && l->next && l->next->bpos) {
        pos -= l->len;
        l = l->next;
      }
      doc->pos = pos;
      doc->currentLine = l;
      gotoLine(doc, l->linenumber);
      arrangeCursor(doc);
      set_mark(l, pos, pos + last - first);
      return SR_FOUND | (wrapped ? SR_WRAPPED : 0);
    }
    if (wrapped && l == begin) /* no match */
      break;
  }
  return SR_NOTFOUND;
}

enum SearchResult backwardSearch(struct Document *doc, const char *str) {
  char *p, *q, *found, *found_last, *first, *last;
  struct Line *l, *begin;
  bool wrapped = false;
  int pos;

  if ((p = regexCompile((char *)str, IgnoreCase)) != NULL) {
    scr_message(p, 0, 0);
    return SR_NOTFOUND;
  }
  l = doc->currentLine;
  if (l == NULL) {
    return SR_NOTFOUND;
  }
  pos = doc->pos;
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
    q = l->lineBuf;
    while (regexMatch(q, &l->lineBuf[l->size] - q, q == l->lineBuf) == 1) {
      matchedPosition(&first, &last);
      if (first <= p) {
        found = first;
        found_last = last;
      }
      if (q - l->lineBuf >= l->size)
        break;
      q++;
      if (q > p)
        break;
    }
    if (found) {
      pos = found - l->lineBuf;
      while (pos >= l->len && l->next && l->next->bpos) {
        pos -= l->len;
        l = l->next;
      }
      doc->pos = pos;
      if (l != doc->currentLine)
        gotoLine(doc, l->linenumber);
      arrangeCursor(doc);
      set_mark(l, pos, pos + found_last - found);
      return SR_FOUND;
    }
  }
  for (l = l->prev;; l = l->prev) {
    if (l == NULL) {
      if (WrapSearch) {
        l = doc->lastLine;
        wrapped = true;
      } else {
        break;
      }
    }
    found = NULL;
    found_last = NULL;
    q = l->lineBuf;
    while (regexMatch(q, &l->lineBuf[l->size] - q, q == l->lineBuf) == 1) {
      matchedPosition(&first, &last);
      found = first;
      found_last = last;
      if (q - l->lineBuf >= l->size)
        break;
      q++;
    }
    if (found) {
      pos = found - l->lineBuf;
      while (pos >= l->len && l->next && l->next->bpos) {
        pos -= l->len;
        l = l->next;
      }
      doc->pos = pos;
      gotoLine(doc, l->linenumber);
      arrangeCursor(doc);
      set_mark(l, pos, pos + found_last - found);
      return SR_FOUND | (wrapped ? SR_WRAPPED : 0);
    }
    if (wrapped && l == begin) /* no match */
      break;
  }
  return SR_NOTFOUND;
}

/* search by regular expression */
static enum SearchResult srchcore(struct Document *doc, const char *str,
                                  SearchRoutine func) {
  if (str != NULL && str != SearchString)
    SearchString = str;

  if (SearchString == NULL || *SearchString == '\0')
    return SR_NOTFOUND;

  auto result = SR_NOTFOUND;
  str = SearchString;
  tty_crmode();
  if (from_jmp()) {
    result = func(doc, str);
    // if (i < PREC_NUM - 1 && result & SR_FOUND)
    clear_mark(doc->currentLine);
  }
  tty_raw();
  return result;
}

static void disp_srchresult(int result, const char *prompt, const char *str) {
  if (str == NULL)
    str = "";
  if (result & SR_NOTFOUND)
    disp_message(Sprintf("Not found: %s", str)->ptr, true);
  else if (result & SR_WRAPPED)
    disp_message(Sprintf("Search wrapped: %s", str)->ptr, true);
  else if (show_srch_str)
    disp_message(Sprintf("%s%s", prompt, str)->ptr, true);
}

static int dispincsrch(struct Document *doc, int ch, Str buf, Lineprop *prop) {
  static struct Line *currentLine;
  static int pos;
  char *str;
  int do_next_search = false;

  static struct Document sbuf;
  if (ch == 0 && buf == NULL) {
    SAVE_BUFPOSITION(&sbuf); /* search starting point */
    currentLine = sbuf.currentLine;
    pos = sbuf.pos;
    return -1;
  }

  str = buf->ptr;
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
        doc->pos += 1;
      SAVE_BUFPOSITION(&sbuf);
      if (srchcore(doc, str, searchRoutine) == SR_NOTFOUND &&
          searchRoutine == forwardSearch) {
        doc->pos -= 1;
        SAVE_BUFPOSITION(&sbuf);
      }
      arrangeCursor(doc);
      displayInvalidate();
      clear_mark(doc->currentLine);
      return -1;
    } else
      return 020; /* _prev completion for C-s C-s */
  } else if (*str) {
    RESTORE_BUFPOSITION(&sbuf);
    arrangeCursor(doc);
    srchcore(doc, str, searchRoutine);
    arrangeCursor(doc);
    currentLine = doc->currentLine;
    pos = doc->pos;
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
  clear_mark(doc->currentLine);
  return -1;
}

void isrch(struct Document *doc, SearchRoutine func, const char *prompt) {
  struct Document sbuf;
  SAVE_BUFPOSITION(&sbuf);
  dispincsrch(doc, 0, NULL, NULL); /* initialize incremental search state */
  searchRoutine = func;
  auto str =
      inputLineHistSearch(doc, prompt, NULL, IN_STRING, TextHist, dispincsrch);
  if (!str) {
    RESTORE_BUFPOSITION(&sbuf);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void srch(struct Document *doc, SearchRoutine func, const char *prompt) {
  const char *str = searchKeyData();

  int disp = false;
  if (str == NULL || *str == '\0') {
    str = inputStrHist(doc, prompt, NULL, TextHist);
    if (str != NULL && *str == '\0')
      str = SearchString;
    if (str == NULL) {
      displayInvalidate();
      return;
    }
    disp = true;
  }
  auto pos = doc->pos;
  if (func == forwardSearch)
    doc->pos += 1;
  auto result = srchcore(doc, str, func);
  if (result & SR_FOUND)
    clear_mark(doc->currentLine);
  else
    doc->pos = pos;
  displayInvalidate();
  if (disp)
    disp_srchresult(result, prompt, str);
  searchRoutine = func;
}

void srch_nxtprv(struct Document *doc, bool reverse) {
  static SearchRoutine routine[2] = {forwardSearch, backwardSearch};

  if (searchRoutine == NULL) {
    /* FIXME: gettextize? */
    disp_message("No previous regular expression", true);
    return;
  }

  if (reverse != 0)
    reverse = 1;
  if (searchRoutine == backwardSearch)
    reverse ^= 1;
  if (reverse == 0)
    doc->pos += 1;
  auto result = srchcore(doc, SearchString, routine[reverse]);
  if (result & SR_FOUND)
    clear_mark(doc->currentLine);
  else {
    if (reverse == 0) {
      doc->pos -= 1;
    }
  }
  displayBuffer(Currentbuf, B_NORMAL);
  disp_srchresult(result, (reverse ? "Backward: " : "Forward: "), SearchString);
}
