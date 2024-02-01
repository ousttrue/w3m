#include "textlist.h"
#include "indep.h"
#include "Str.h"
#include <gc.h>
#include "myctype.h"

/* General doubly linked list */

ListItem *newListItem(void *s, ListItem *n, ListItem *p) {
  auto it = (ListItem *)New(ListItem);
  it->ptr = s;
  it->next = n;
  it->prev = p;
  return it;
}

GeneralList *newGeneralList() {
  auto tl = (GeneralList *)New(GeneralList);
  tl->first = tl->last = NULL;
  tl->nitem = 0;
  return tl;
}

void pushValue(GeneralList *tl, void *s) {
  ListItem *it;
  if (s == NULL || tl == NULL || tl->nitem >= GENERAL_LIST_MAX)
    return;
  it = newListItem(s, NULL, tl->last);
  if (tl->first == NULL) {
    tl->first = it;
    tl->last = it;
    tl->nitem = 1;
  } else {
    tl->last->next = it;
    tl->last = it;
    tl->nitem++;
  }
}

void *popValue(GeneralList *tl) {
  ListItem *f;

  if (tl == NULL || tl->first == NULL)
    return NULL;
  f = tl->first;
  tl->first = f->next;
  if (tl->first)
    tl->first->prev = NULL;
  else
    tl->last = NULL;
  tl->nitem--;
  return f->ptr;
}

void *rpopValue(GeneralList *tl) {
  ListItem *f;

  if (tl == NULL || tl->last == NULL)
    return NULL;
  f = tl->last;
  tl->last = f->prev;
  if (tl->last)
    tl->last->next = NULL;
  else
    tl->first = NULL;
  tl->nitem--;
  return f->ptr;
}

void delValue(GeneralList *tl, ListItem *it) {
  if (it->prev)
    it->prev->next = it->next;
  else
    tl->first = it->next;
  if (it->next)
    it->next->prev = it->prev;
  else
    tl->last = it->prev;
  tl->nitem--;
}

GeneralList *appendGeneralList(GeneralList *tl, GeneralList *tl2) {
  if (tl && tl2) {
    if (tl2->first) {
      if (tl->last) {
        if (tl->nitem + tl2->nitem > GENERAL_LIST_MAX) {
          return tl;
        }
        tl->last->next = tl2->first;
        tl2->first->prev = tl->last;
        tl->last = tl2->last;
        tl->nitem += tl2->nitem;
      } else {
        tl->first = tl2->first;
        tl->last = tl2->last;
        tl->nitem = tl2->nitem;
      }
    }
    tl2->first = tl2->last = NULL;
    tl2->nitem = 0;
  }

  return tl;
}

/* Line text list */

TextLine *newTextLine(Str *line, int pos) {
  auto lbuf = (TextLine *)New(TextLine);
  if (line)
    lbuf->line = line;
  else
    lbuf->line = Strnew();
  lbuf->pos = pos;
  return lbuf;
}

void appendTextLine(TextLineList *tl, Str *line, int pos) {
  TextLine *lbuf;

  if (tl->last == NULL) {
    pushTextLine(tl, newTextLine(Strdup(line), pos));
  } else {
    lbuf = tl->last->ptr;
    if (lbuf->line)
      Strcat(lbuf->line, line);
    else
      lbuf->line = line;
    lbuf->pos += pos;
  }
}

TextList *make_domain_list(const char *domain_list) {
  Str *tmp;
  TextList *domains = NULL;

  auto p = domain_list;
  tmp = Strnew_size(64);
  while (*p) {
    while (*p && IS_SPACE(*p))
      p++;
    Strclear(tmp);
    while (*p && !IS_SPACE(*p) && *p != ',')
      Strcat_char(tmp, *p++);
    if (tmp->length > 0) {
      if (domains == NULL)
        domains = newTextList();
      pushText(domains, tmp->ptr);
    }
    while (*p && IS_SPACE(*p))
      p++;
    if (*p == ',')
      p++;
  }
  return domains;
}
