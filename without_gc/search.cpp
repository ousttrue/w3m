#include "search.h"
#include "line_layout.h"

bool IgnoreCase = true;
bool WrapSearch = false;
bool show_srch_str = true;

static const char *regexCompile(const char *ex, int igncase)
{
  return nullptr;
}
static int regexMatch(const char *str, int len, int firstp)
{
  return -1;
}
static void matchedPosition(const char **first, const char **last)
{
}

int forwardSearch(LineLayout *layout, const std::string &str) {

  if (auto p = regexCompile(str.c_str(), IgnoreCase)) {
    // App::instance().message(p);
    return SR_NOTFOUND;
  }

  auto l = layout->currentLine();
  if (l == NULL) {
    return SR_NOTFOUND;
  }

  auto pos = layout->cursorPos();
  auto begin = l;
  if (pos < l->len() &&
      regexMatch(&l->lineBuf()[pos], l->len() - pos, 0) == 1) {
    const char *first, *last;
    matchedPosition(&first, &last);
    pos = first - l->lineBuf();
    layout->cursorPos(pos);
    if (l != layout->currentLine()) {
      layout->visual.cursorRow(layout->linenumber(l));
    }
    l->set_mark(pos, pos + last - first);
    return SR_FOUND;
  }

  bool wrapped = false;
  ++l;
  for (;; ++l) {
    if (l == NULL) {
      if (WrapSearch) {
        l = layout->firstLine();
        wrapped = true;
      } else {
        break;
      }
    }
    if (regexMatch(l->lineBuf(), l->len(), 1) == 1) {
      const char *first, *last;
      matchedPosition(&first, &last);
      pos = first - l->lineBuf();
      layout->cursorPos(pos);
      layout->visual.cursorRow(layout->linenumber(l));
      l->set_mark(pos, pos + last - first);
      return SR_FOUND | (wrapped ? SR_WRAPPED : 0);
    }
    if (wrapped && l == begin) /* no match */
      break;
  }
  return SR_NOTFOUND;
}

int backwardSearch(LineLayout *layout, const std::string &str) {
  if (auto p = regexCompile(str.c_str(), IgnoreCase)) {
    // App::instance().message(p);
    return SR_NOTFOUND;
  }

  auto l = layout->currentLine();
  if (l == NULL) {
    return SR_NOTFOUND;
  }

  auto pos = layout->cursorPos();
  auto begin = l;
  if (pos > 0) {
    pos--;
    auto p = &l->lineBuf()[pos];
    const char *found = NULL;
    const char *found_last = NULL;
    auto q = l->lineBuf();
    while (regexMatch(q, &l->lineBuf()[l->len()] - q, q == l->lineBuf()) == 1) {
      const char *first, *last;
      matchedPosition(&first, &last);
      if (first <= p) {
        found = first;
        found_last = last;
      }
      if (q - l->lineBuf() >= l->len())
        break;
      q++;
      if (q > p)
        break;
    }
    if (found) {
      pos = found - l->lineBuf();
      layout->cursorPos(pos);
      if (l != layout->currentLine()) {
        layout->visual.cursorRow(layout->linenumber(l));
      }
      l->set_mark(pos, pos + found_last - found);
      return SR_FOUND;
    }
  }

  bool wrapped = false;
  --l;
  for (;; --l) {
    if (layout->isNull(l)) {
      if (WrapSearch) {
        l = layout->lastLine();
        wrapped = true;
      } else {
        break;
      }
    }
    const char *found = NULL;
    const char *found_last = NULL;
    auto q = l->lineBuf();
    while (regexMatch(q, &l->lineBuf()[l->len()] - q, q == l->lineBuf()) == 1) {
      const char *first, *last;
      matchedPosition(&first, &last);
      found = first;
      found_last = last;
      if (q - l->lineBuf() >= l->len())
        break;
      q++;
    }
    if (found) {
      pos = found - l->lineBuf();
      layout->cursorPos(pos);
      layout->visual.cursorRow(layout->linenumber(l));
      l->set_mark(pos, pos + found_last - found);
      return SR_FOUND | (wrapped ? SR_WRAPPED : 0);
    }
    if (wrapped && l == begin) /* no match */
      break;
  }
  return SR_NOTFOUND;
}

static SearchFunc searchRoutine;

static void clear_mark(Line *l) {
  int pos;
  if (!l)
    return;
  for (pos = 0; pos < l->len(); pos++) {
    l->propBufDel(pos, PE_MARK);
  }
}

/* search by regular expression */
static int srchcore(const std::shared_ptr<LineLayout> &layout,
                    const std::string &str, const SearchFunc &func) {
  int result = SR_NOTFOUND;

  // crmode();
  // if (SETJMP(IntReturn) == 0) {
  //   for (i = 0; i < PREC_NUM; i++) {
  //     result = func(&CurrentTab->currentBuffer()->layout, str);
  //     if (i < PREC_NUM - 1 && result & SR_FOUND)
  //       clear_mark(CurrentTab->currentBuffer()->layout.currentLine);
  //   }
  // }
  // mySignal(SIGINT, prevtrap);
  // term_raw();
  return result;
}

// static void disp_srchresult(int result, const char *prompt, const char *str)
// {
//   if (str == nullptr)
//     str = "";
//   if (result & SR_NOTFOUND)
//     App::instance().disp_message(Sprintf("Not found: %s", str)->ptr);
//   else if (result & SR_WRAPPED)
//     App::instance().disp_message(Sprintf("Search wrapped: %s", str)->ptr);
//   else if (show_srch_str)
//     App::instance().disp_message(Sprintf("%s%s", prompt, str)->ptr);
// }

/* Search regular expression forward */

void srch_nxtprv(const std::shared_ptr<LineLayout> &layout,
                 const std::string &str, int reverse) {
  static SearchFunc routine[2] = {forwardSearch, backwardSearch};

  if (searchRoutine == nullptr) {
    // App::instance().disp_message("No previous regular expression");
    return;
  }
  if (reverse != 0) {
    reverse = 1;
  }
  if (searchRoutine == backwardSearch) {
    reverse ^= 1;
  }
  if (reverse == 0) {
    layout->visual.cursorMoveCol(1);
  }

  auto result = srchcore(layout, str, routine[reverse]);
  if (result & SR_FOUND) {
    clear_mark(layout->currentLine());
  } else {
    if (reverse == 0) {
      layout->visual.cursorMoveCol(-1);
    }
  }
  // disp_srchresult(result, (reverse ? "Backward: " : "Forward: "),
  // SearchString);
}

// static int dispincsrch(const std::shared_ptr<LineLayout> &layout, int ch,
//                        Str *buf, Lineprop *prop) {
//
//   // static LineLayout sbuf();
//   if (ch == 0 && !buf) {
//     return -1;
//   }
//
//   bool do_next_search = false;
//   auto str = buf->ptr;
//   switch (ch) {
//   case 022: /* C-r */
//     searchRoutine = backwardSearch;
//     do_next_search = true;
//     break;
//   case 023: /* C-s */
//     searchRoutine = forwardSearch;
//     do_next_search = true;
//     break;
//
//   default:
//     if (ch >= 0)
//       return ch; /* use InputKeymap */
//   }
//
//   if (do_next_search) {
//     if (*str) {
//       if (searchRoutine == forwardSearch)
//         layout->visual.cursorMoveCol(1);
//       // sbuf.COPY_BUFROOT_FROM(CurrentTab->currentBuffer()->layout);
//       if (srchcore(layout, str, searchRoutine) == SR_NOTFOUND &&
//           searchRoutine == forwardSearch) {
//         layout->visual.cursorMoveCol(-1);
//         // sbuf.COPY_BUFROOT_FROM(CurrentTab->currentBuffer()->layout);
//       }
//       clear_mark(layout->currentLine());
//       return -1;
//     } else
//       return 020; /* _prev completion for C-s C-s */
//   } else if (*str) {
//     // CurrentTab->currentBuffer()->layout.COPY_BUFROOT_FROM(sbuf);
//     srchcore(layout, str, searchRoutine);
//   }
//   clear_mark(layout->currentLine());
//   return -1;
// }

void isrch(const std::shared_ptr<LineLayout> &layout, const SearchFunc &func,
           const std::string &str) {
  // LineLayout sbuf(0);
  // sbuf.COPY_BUFROOT_FROM(CurrentTab->currentBuffer()->layout);
  // dispincsrch(layout, 0, nullptr,
  //             nullptr); /* initialize incremental search state */

  searchRoutine = func;
  // auto str =
  //     inputLineHistSearch(prompt, nullptr, IN_STRING, TextHist, dispincsrch);
  // if (str == nullptr) {
  //   RESTORE_BUFPOSITION(&sbuf);
  // }
}

void srch(const std::shared_ptr<LineLayout> &layout, const SearchFunc &func,
          const std::string &str) {

  int col = layout->visual.cursor().col;
  if (func == forwardSearch)
    layout->visual.cursorMoveCol(1);

  int result = srchcore(layout, str, func);
  if (result & SR_FOUND)
    clear_mark(layout->currentLine());
  else
    layout->visual.cursorCol(col);
  // if (disp)
  //   disp_srchresult(result, prompt, str);
  searchRoutine = func;
}
