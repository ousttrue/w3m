#pragma once
#include "anchor.h"
#include "istream.h"
#include "form.h"
#include "url.h"
#include "textlist.h"

/* Link Buffer */
#define LB_NOLINK -1
#define LB_FRAME 0 /* rFrame() */
#define LB_N_FRAME 1
#define LB_INFO 2 /* pginfo() */
#define LB_N_INFO 3
#define LB_SOURCE 4 /* vwSrc() */
#define LB_N_SOURCE LB_SOURCE
#define MAX_LB 5
extern int REV_LB[];

#define NO_REFERER ((char *)-1)

#define LINK_TYPE_NONE 0
#define LINK_TYPE_REL 1
#define LINK_TYPE_REV 2
typedef struct _LinkList {
  char *url;
  char *title; /* Next, Contents, ... */
  char *ctype; /* Content-Type */
  char type;   /* Rel, Rev */
  struct _LinkList *next;
} LinkList;

typedef struct Buffer {
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
  char *real_type;
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
  InputStream pagerSource;
  AnchorList *href;
  AnchorList *name;
  AnchorList *img;
  AnchorList *formitem;
  LinkList *linklist;
  FormList *formlist;
  struct MapList *maplist;
  HmarkerList *hmarklist;
  HmarkerList *imarklist;
  ParsedURL currentURL;
  ParsedURL *baseURL;
  char *baseTarget;
  int real_scheme;
  char *sourcefile;
  struct frameset *frameset;
  struct frameset_queue *frameQ;
  int *clone;
  size_t trbyte;
  char check_url;
  TextList *document_header;
  FormItemList *form_submit;
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
  Anchor *submit;
  struct _BufferPos *undo;
} Buffer;
