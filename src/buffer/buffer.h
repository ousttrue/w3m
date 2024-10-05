#pragma once
#include "html/anchor.h"
#include "html/form.h"
#include "input/url.h"
#include "textlist.h"
#include "buffer/line.h"

/* Link Buffer */
#define LB_NOLINK -1
#define LB_INFO 0 /* pginfo() */
#define LB_N_INFO 1
#define LB_SOURCE 2 /* vwSrc() */
#define LB_N_SOURCE LB_SOURCE
#define MAX_LB 3
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

/* Buffer Property */
enum BufferProperty {
  BP_NORMAL = 0x0,
  BP_PIPE = 0x1,
  BP_FRAME = 0x2,
  BP_INTERNAL = 0x8,
  BP_NO_URL = 0x10,
  BP_REDIRECTED = 0x20,
  BP_CLOSE = 0x40,
};

/* mark URL, Message-ID */
enum CheckUrlFlags {
  CHK_URL = 1,
  CHK_NMID = 2,
};

struct HttpResponse;
struct Document;
struct Buffer {
  const char *filename;
  const char *buffername;
  struct Buffer *nextBuffer;
  struct Buffer *linkBuffer[MAX_LB];
  struct Document *document;
  const char *type;
  const char *real_type;
  enum BufferProperty bufferprop;
  struct Url currentURL;
  int real_scheme;
  const char *sourcefile;
  int *clone;
  size_t trbyte;
  enum CheckUrlFlags check_url;
  struct HttpResponse *http_response;
  struct FormItemList *form_submit;
  const char *edit;
  const char *ssl_certificate;
  char image_flag;
  char image_loaded;
  char need_reshape;
  struct Anchor *submit;
};

#define NO_BUFFER ((struct Buffer *)1)

struct Url *baseURL(struct Buffer *buf);
void chkURLBuffer(struct Buffer *buf);
struct Line;
char *last_modified(struct Buffer *buf);
struct parsed_tag;

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
extern void gotoRealLine(struct Buffer *buf, int n);
extern struct Buffer *selectBuffer(struct Buffer *firstbuf,
                                   struct Buffer *currentbuf, char *selectchar);
extern void reshapeBuffer(struct Buffer *buf);
extern struct Buffer *prevBuffer(struct Buffer *first, struct Buffer *buf);

extern struct Buffer *page_info_panel(struct Buffer *buf);
extern const char *guess_save_name(struct Buffer *buf, const char *file);
extern void saveBufferInfo(void);
extern struct Buffer *link_list_panel(struct Buffer *buf);
