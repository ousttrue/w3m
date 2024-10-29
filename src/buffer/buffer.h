#pragma once
#include "buffer/line.h"
#include "html/anchor.h"
#include "html/form.h"
#include "input/url.h"
#include "text/textlist.h"

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

/* mark URL, Message-ID */
enum CheckUrlFlags {
  CHK_URL = 1,
  CHK_NMID = 2,
};

struct HttpResponse;
struct Document;
struct Buffer {
  const char *buffername;
  struct Buffer *nextBuffer;
  struct Buffer *linkBuffer[MAX_LB];
  struct Document *document;
  int *clone;
  enum CheckUrlFlags check_url;
  struct HttpResponse *http_response;
  const char *edit;
  char image_flag;
  char image_loaded;
};

#define NO_BUFFER ((struct Buffer *)1)

struct Url *baseURL(struct Buffer *buf);
void chkURLBuffer(struct Buffer *buf);
struct Line;
char *last_modified(struct Buffer *buf);
struct HtmlTag;

extern void saveBuffer(struct Buffer *buf, FILE *f, int cont);
extern void saveBufferBody(struct Buffer *buf, FILE *f, int cont);
extern struct Buffer *newBuffer();
extern struct Buffer *nullBuffer(void);
extern void clearBuffer(struct Buffer *buf);
extern void discardBuffer(struct Buffer *buf);
extern struct Buffer *namedBuffer(struct Buffer *first, char *name);
extern struct Buffer *deleteBuffer(struct Buffer *first, struct Buffer *delbuf);
extern struct Buffer *replaceBuffer(struct Buffer *first, struct Buffer *delbuf,
                                    struct Buffer *newbuf);
extern struct Buffer *nthBuffer(struct Buffer *firstbuf, int n);
extern struct Buffer *selectBuffer(struct Buffer *firstbuf,
                                   struct Buffer *currentbuf, char *selectchar);
extern void reshapeBuffer(struct Buffer *buf);
extern struct Buffer *prevBuffer(struct Buffer *first, struct Buffer *buf);

extern struct Document *page_info_panel(struct Buffer *buf);
extern void saveBufferInfo(void);
extern struct Document *link_list_panel(struct Buffer *buf);
