#pragma once
#include "anchor.h"
#include "form.h"
#include "url.h"
#include "textlist.h"
#include "line.h"

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

struct BufferPos {
  long top_linenumber;
  long cur_linenumber;
  int currentColumn;
  int pos;
  int bpos;
  struct BufferPos *next;
  struct BufferPos *prev;
};

struct Buffer {
  char *filename;
  char *buffername;
  struct Line *firstLine;
  struct Line *topLine;
  struct Line *currentLine;
  struct Line *lastLine;
  struct Buffer *nextBuffer;
  struct Buffer *linkBuffer[MAX_LB];
  short width;
  short height;
  char *type;
  const char *real_type;
  int allLine;
  short bufferprop;
  int currentColumn;
  short cursorX;
  short cursorY;
  int pos;
  int visualpos;
  short rootX;
  short rootY;
  short COLS;
  short LINES;
  struct AnchorList *href;
  struct AnchorList *name;
  struct AnchorList *img;
  struct AnchorList *formitem;
  struct LinkList *linklist;
  struct FormList *formlist;
  struct MapList *maplist;
  struct HmarkerList *hmarklist;
  struct HmarkerList *imarklist;
  struct Url currentURL;
  struct Url *baseURL;
  char *baseTarget;
  int real_scheme;
  char *sourcefile;
  int *clone;
  size_t trbyte;
  char check_url;
  struct TextList *document_header;
  struct FormItemList *form_submit;
  char *savecache;
  char *edit;
  struct mailcap *mailcap;
  char *mailcap_source;
  char *header_source;
  char search_header;
  char *ssl_certificate;
  char image_flag;
  char image_loaded;
  char need_reshape;
  struct Anchor *submit;
  struct BufferPos *undo;
};

#define NO_BUFFER ((struct Buffer *)1)

void chkURLBuffer(struct Buffer *buf);
void addnewline(struct Buffer *buf, char *line, Lineprop *prop, int pos,
                int width, int nlines);
