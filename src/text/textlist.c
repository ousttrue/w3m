#include "textlist.h"
#include "alloc.h"
#include "text/Str.h"
#include "text/myctype.h"

/* General doubly linked list */

struct ListItem *newListItem(void *s, struct ListItem *n, struct ListItem *p) {
  auto it = New(struct ListItem);
  it->ptr = s;
  it->next = n;
  it->prev = p;
  return it;
}

struct GeneralList *newGeneralList() {
  struct GeneralList *tl = New(struct GeneralList);
  tl->first = tl->last = NULL;
  tl->nitem = 0;
  return tl;
}

void pushValue(struct GeneralList *tl, void *s) {
  struct ListItem *it;
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

void *popValue(struct GeneralList *tl) {
  struct ListItem *f;

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

void *rpopValue(struct GeneralList *tl) {
  struct ListItem *f;

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

void delValue(struct GeneralList *tl, struct ListItem *it) {
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

struct GeneralList *appendGeneralList(struct GeneralList *tl,
                                      struct GeneralList *tl2) {
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

struct TextLine *newTextLine(Str line, int pos) {
  struct TextLine *lbuf = New(struct TextLine);
  if (line)
    lbuf->line = line;
  else
    lbuf->line = Strnew();
  lbuf->pos = pos;
  return lbuf;
}

void appendTextLine(struct TextLineList *tl, Str line, int pos) {
  struct TextLine *lbuf;

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

struct TextList *make_domain_list(const char *domain_list) {
  struct TextList *domains = nullptr;
  auto p = domain_list;
  auto tmp = Strnew_size(64);
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

struct TextList *newTextList() { return ((struct TextList *)newGeneralList()); }
void pushText(struct TextList *tl, const char *s) {
  pushValue((struct GeneralList *)(tl), (void *)allocStr((s), -1));
}
const char *popText(struct TextList *tl) {
  return (const char *)popValue((struct GeneralList *)(tl));
}
const char *rpopText(struct TextList *tl) {
  return (const char *)rpopValue((struct GeneralList *)(tl));
}
void delText(struct TextList *tl, void *i) {
  delValue((struct GeneralList *)(tl), i);
}
struct TextList *appendTextList(struct TextList *tl, struct TextList *tl2) {
  return ((struct TextList *)appendGeneralList((struct GeneralList *)(tl),
                                               (struct GeneralList *)(tl2)));
}

struct TextLineList *newTextLineList() {
  return ((struct TextLineList *)newGeneralList());
}
void pushTextLine(struct TextLineList *tl, struct TextLine *lbuf) {
  pushValue((struct GeneralList *)(tl), (void *)(lbuf));
}
struct TextLine *popTextLine(struct TextLineList *tl) {
  return ((struct TextLine *)popValue((struct GeneralList *)(tl)));
}
struct TextLine *rpopTextLine(struct TextLineList *tl) {
  return ((struct TextLine *)rpopValue((struct GeneralList *)(tl)));
}
struct TextLineList *appendTextLineList(struct TextLineList *tl,
                                        struct TextLineList *tl2) {
  return ((struct TextLineList *)appendGeneralList(
      (struct GeneralList *)(tl), (struct GeneralList *)(tl2)));
}
