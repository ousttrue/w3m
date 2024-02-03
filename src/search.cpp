#include "search.h"
#include "w3m.h"
#include "message.h"
#include "display.h"
#include "buffer.h"
#include "regex.h"
#include "proto.h"
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
