#pragma once
#include "url.h"
#include <stddef.h>

/* Link Buffer */
#define LB_NOLINK -1
#define LB_FRAME 0 /* rFrame() */
#define LB_N_FRAME 1
#define LB_INFO 2 /* pginfo() */
#define LB_N_INFO 3
#define LB_SOURCE 4 /* vwSrc() */
#define LB_N_SOURCE LB_SOURCE
#define MAX_LB 5

struct FormList;
struct Anchor;
struct AnchorList;
struct HmarkerList;
struct FormItemList;
struct MapList;
struct TextList;
union input_stream;
struct Line;
struct LinkList;
struct BufferPos;
struct AlarmEvent;
struct Buffer {
  char *filename;
  char *buffername;
  Line *firstLine;
  Line *topLine;
  Line *currentLine;
  Line *lastLine;
  Buffer *nextBuffer;
  Buffer *linkBuffer[MAX_LB];
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
  input_stream *pagerSource;
  AnchorList *href;
  AnchorList *name;
  AnchorList *img;
  AnchorList *formitem;
  LinkList *linklist;
  FormList *formlist;
  MapList *maplist;
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
  BufferPos *undo;
  AlarmEvent *event;
};

struct TabBuffer {
  TabBuffer *nextTab;
  TabBuffer *prevTab;
  Buffer *currentBuffer;
  Buffer *firstBuffer;
  short x1;
  short x2;
  short y;
};

extern int REV_LB[];
