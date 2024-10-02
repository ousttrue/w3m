#include "search.h"
#include "defun.h"
#include "fm.h"
#include "regex.h"
#include "buffer.h"
#include "document.h"
#include "scr.h"
#include "tty.h"
#include "trap_jmp.h"
#include "tabbuffer.h"
#include "terms.h"
#include "display.h"
#include "anchor.h"
#include "linein.h"
#include "history.h"
#include <unistd.h>

static const char *SearchString = NULL;
SearchRoutine searchRoutine = nullptr;

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
static enum SearchResult srchcore(const char *str, SearchRoutine func) {
  if (str != NULL && str != SearchString)
    SearchString = str;

  if (SearchString == NULL || *SearchString == '\0')
    return SR_NOTFOUND;

  auto result = SR_NOTFOUND;
  str = SearchString;
  tty_crmode();
  if (from_jmp()) {
    result = func(&Currentbuf->document, str);
    // if (i < PREC_NUM - 1 && result & SR_FOUND)
    //   clear_mark(Currentbuf->document.currentLine);
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

static int dispincsrch(int ch, Str buf, Lineprop *prop) {
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
        Currentbuf->document.pos += 1;
      SAVE_BUFPOSITION(&sbuf);
      if (srchcore(str, searchRoutine) == SR_NOTFOUND &&
          searchRoutine == forwardSearch) {
        Currentbuf->document.pos -= 1;
        SAVE_BUFPOSITION(&sbuf);
      }
      arrangeCursor(&Currentbuf->document);
      displayBuffer(Currentbuf, B_FORCE_REDRAW);
      clear_mark(Currentbuf->document.currentLine);
      return -1;
    } else
      return 020; /* _prev completion for C-s C-s */
  } else if (*str) {
    RESTORE_BUFPOSITION(&sbuf);
    arrangeCursor(&Currentbuf->document);
    srchcore(str, searchRoutine);
    arrangeCursor(&Currentbuf->document);
    currentLine = Currentbuf->document.currentLine;
    pos = Currentbuf->document.pos;
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
  clear_mark(Currentbuf->document.currentLine);
  return -1;
}

void isrch(SearchRoutine func, const char *prompt) {
  struct Document sbuf;
  SAVE_BUFPOSITION(&sbuf);
  dispincsrch(0, NULL, NULL); /* initialize incremental search state */
  searchRoutine = func;
  auto str =
      inputLineHistSearch(prompt, NULL, IN_STRING, TextHist, dispincsrch);
  if (!str) {
    RESTORE_BUFPOSITION(&sbuf);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void srch(SearchRoutine func, const char *prompt) {
  const char *str = searchKeyData();

  int disp = false;
  if (str == NULL || *str == '\0') {
    str = inputStrHist(prompt, NULL, TextHist);
    if (str != NULL && *str == '\0')
      str = SearchString;
    if (str == NULL) {
      displayBuffer(Currentbuf, B_NORMAL);
      return;
    }
    disp = true;
  }
  auto pos = Currentbuf->document.pos;
  if (func == forwardSearch)
    Currentbuf->document.pos += 1;
  auto result = srchcore(str, func);
  if (result & SR_FOUND)
    clear_mark(Currentbuf->document.currentLine);
  else
    Currentbuf->document.pos = pos;
  displayBuffer(Currentbuf, B_NORMAL);
  if (disp)
    disp_srchresult(result, prompt, str);
  searchRoutine = func;
}

void srch_nxtprv(bool reverse) {
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
    Currentbuf->document.pos += 1;
  auto result = srchcore(SearchString, routine[reverse]);
  if (result & SR_FOUND)
    clear_mark(Currentbuf->document.currentLine);
  else {
    if (reverse == 0)
      Currentbuf->document.pos -= 1;
  }
  displayBuffer(Currentbuf, B_NORMAL);
  disp_srchresult(result, (reverse ? "Backward: " : "Forward: "), SearchString);
}
