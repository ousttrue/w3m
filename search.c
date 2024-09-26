#include "fm.h"
#include "regex.h"
#include "buffer.h"
#include "scr.h"
#include <signal.h>
#include <errno.h>
#include <unistd.h>

static void set_mark(Line *l, int pos, int epos) {
  for (; pos < epos && pos < l->size; pos++)
    l->propBuf[pos] |= PE_MARK;
}

int forwardSearch(struct Buffer *buf, char *str) {
  char *p, *first, *last;
  Line *l, *begin;
  int wrapped = FALSE;
  int pos;

  if ((p = regexCompile(str, IgnoreCase)) != NULL) {
    scr_message(p, 0, 0);
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
  if (pos < l->size && regexMatch(&l->lineBuf[pos], l->size - pos, 0) == 1) {
    matchedPosition(&first, &last);
    pos = first - l->lineBuf;
    while (pos >= l->len && l->next && l->next->bpos) {
      pos -= l->len;
      l = l->next;
    }
    buf->pos = pos;
    if (l != buf->currentLine)
      gotoLine(buf, l->linenumber);
    arrangeCursor(buf);
    set_mark(l, pos, pos + last - first);
    return SR_FOUND;
  }
  for (l = l->next;; l = l->next) {
    if (l == NULL) {
      if (buf->pagerSource) {
        l = getNextPage(buf, 1);
        if (l == NULL) {
          if (WrapSearch && !wrapped) {
            l = buf->firstLine;
            wrapped = TRUE;
          } else {
            break;
          }
        }
      } else if (WrapSearch) {
        l = buf->firstLine;
        wrapped = TRUE;
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
      buf->pos = pos;
      buf->currentLine = l;
      gotoLine(buf, l->linenumber);
      arrangeCursor(buf);
      set_mark(l, pos, pos + last - first);
      return SR_FOUND | (wrapped ? SR_WRAPPED : 0);
    }
    if (wrapped && l == begin) /* no match */
      break;
  }
  return SR_NOTFOUND;
}

int backwardSearch(Buffer *buf, char *str) {
  char *p, *q, *found, *found_last, *first, *last;
  Line *l, *begin;
  int wrapped = FALSE;
  int pos;

  if ((p = regexCompile(str, IgnoreCase)) != NULL) {
    scr_message(p, 0, 0);
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
      buf->pos = pos;
      if (l != buf->currentLine)
        gotoLine(buf, l->linenumber);
      arrangeCursor(buf);
      set_mark(l, pos, pos + found_last - found);
      return SR_FOUND;
    }
  }
  for (l = l->prev;; l = l->prev) {
    if (l == NULL) {
      if (WrapSearch) {
        l = buf->lastLine;
        wrapped = TRUE;
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
      buf->pos = pos;
      gotoLine(buf, l->linenumber);
      arrangeCursor(buf);
      set_mark(l, pos, pos + found_last - found);
      return SR_FOUND | (wrapped ? SR_WRAPPED : 0);
    }
    if (wrapped && l == begin) /* no match */
      break;
  }
  return SR_NOTFOUND;
}
