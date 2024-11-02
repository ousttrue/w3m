#pragma once
#include "html/anchor.h"
#include "html/form.h"
#include "input/url.h"

/* Link Buffer */
enum LinkBuffer {
  LB_NOLINK = -1,
  LB_INFO = 0, /* pginfo() */
  LB_N_INFO = 1,
  LB_SOURCE = 2, /* vwSrc() */
  LB_N_SOURCE = LB_SOURCE,
  MAX_LB = 3,
};
extern int REV_LB[];

enum LINK_TYPE {
  LINK_TYPE_NONE = 0,
  LINK_TYPE_REL = 1,
  LINK_TYPE_REV = 2,
};
struct LinkList {
  const char *url;
  const char *title;   /* Next, Contents, ... */
  const char *ctype;   /* Content-Type */
  enum LINK_TYPE type; /* Rel, Rev */
  struct LinkList *next;
};

struct HttpResponse;
struct Document;
struct Buffer {
  struct Buffer *nextBuffer;
  struct Content *content;
  struct Document *document;
  struct HttpResponse *http_response;
  const char *edit;
  char image_flag;
  char image_loaded;
  int *clone;
};

struct Line;
char *last_modified(struct Buffer *buf);
struct HtmlTag;

void saveBuffer(struct Buffer *buf, FILE *f, int cont);
void saveBufferBody(struct Buffer *buf, FILE *f, int cont);
struct Buffer *newBuffer();
// struct Buffer *nullBuffer(void);
void discardBuffer(struct Buffer *buf);
// struct Buffer *namedBuffer(struct Buffer *first, char *name);
struct Buffer *deleteBuffer(struct Buffer *first, struct Buffer *delbuf);
struct Buffer *replaceBuffer(struct Buffer *first, struct Buffer *delbuf,
                             struct Buffer *newbuf);
struct Buffer *nthBuffer(struct Buffer *firstbuf, int n);
struct Buffer *prevBuffer(struct Buffer *first, struct Buffer *buf);

Str page_info_panel(struct Buffer *buf);
void saveBufferInfo(void);
Str link_list_panel(struct Buffer *buf);
struct Content *loadLink(struct Buffer *src,
                         const char *url, const char *target,
                         const char *referer, struct FormList *form);
struct Content *_followForm(struct Buffer *doc, bool submit,
                            struct Current current);
struct Url *baseURL(struct Buffer *buf);
