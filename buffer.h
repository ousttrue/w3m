#pragma once
#include "document.h"
#include "anchor.h"
#include "form.h"
#include "url.h"
#include "textlist.h"
#include "line.h"

extern int FoldLine;
extern int showLineNum;

#define _INIT_BUFFER_WIDTH (COLS - (showLineNum ? 6 : 1))
#define INIT_BUFFER_WIDTH ((_INIT_BUFFER_WIDTH > 0) ? _INIT_BUFFER_WIDTH : 0)
#define FOLD_BUFFER_WIDTH (FoldLine ? (INIT_BUFFER_WIDTH + 1) : -1)

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
  char *url;
  char *title;         /* Next, Contents, ... */
  char *ctype;         /* Content-Type */
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

struct Buffer {
  const char *filename;
  const char *buffername;
  struct Buffer *nextBuffer;
  struct Buffer *linkBuffer[MAX_LB];
  struct Document document;
  const char *type;
  const char *real_type;
  enum BufferProperty bufferprop;
  struct Url currentURL;
  struct Url *baseURL;
  const char *baseTarget;
  int real_scheme;
  const char *sourcefile;
  int *clone;
  size_t trbyte;
  char check_url;
  struct TextList *document_header;
  struct FormItemList *form_submit;
  const char *edit;
  struct mailcap *mailcap;
  const char *mailcap_source;
  const char *header_source;
  char search_header;
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
struct Anchor *registerForm(struct Buffer *buf, struct FormList *flist,
                            struct parsed_tag *tag, int line, int pos);
int currentLn(struct Buffer *buf);
struct HmarkerList *putHmarker(struct HmarkerList *ml, int line, int pos,
                               int seq);

#define COPY_BUFROOT(dstbuf, srcbuf)                                           \
  {                                                                            \
    (dstbuf)->rootX = (srcbuf)->rootX;                       \
    (dstbuf)->rootY = (srcbuf)->rootY;                       \
    (dstbuf)->COLS = (srcbuf)->COLS;                         \
    (dstbuf)->LINES = (srcbuf)->LINES;                       \
  }

#define COPY_BUFPOSITION(dstbuf, srcbuf)                                       \
  {                                                                            \
    (dstbuf)->topLine = (srcbuf)->topLine;                   \
    (dstbuf)->currentLine = (srcbuf)->currentLine;           \
    (dstbuf)->pos = (srcbuf)->pos;                           \
    (dstbuf)->cursorX = (srcbuf)->cursorX;                   \
    (dstbuf)->cursorY = (srcbuf)->cursorY;                   \
    (dstbuf)->visualpos = (srcbuf)->visualpos;               \
    (dstbuf)->currentColumn = (srcbuf)->currentColumn;       \
  }
#define SAVE_BUFPOSITION(sbufp) COPY_BUFPOSITION(sbufp, &Currentbuf->document)
#define RESTORE_BUFPOSITION(sbufp) COPY_BUFPOSITION(&Currentbuf->document, sbufp)

