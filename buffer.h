#pragma once
#include "url.h"
#include "line.h"
#include <stddef.h>

#define NO_BUFFER ((Buffer *)1)

#define _INIT_BUFFER_WIDTH (COLS - (showLineNum ? 6 : 1))
#define INIT_BUFFER_WIDTH ((_INIT_BUFFER_WIDTH > 0) ? _INIT_BUFFER_WIDTH : 0)
#define FOLD_BUFFER_WIDTH (FoldLine ? (INIT_BUFFER_WIDTH + 1) : -1)

/* Buffer Property */
#define BP_NORMAL 0x0
#define BP_PIPE 0x1
#define BP_INTERNAL 0x8
#define BP_NO_URL 0x10
#define BP_REDIRECTED 0x20
#define BP_CLOSE 0x40

/* mark URL, Message-ID */
#define CHK_URL 1
#define CHK_NMID 2

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
  const char *filename;
  const char *buffername;
  Line *firstLine;
  Line *topLine;
  Line *currentLine;
  Line *lastLine;
  Buffer *nextBuffer;
  Buffer *linkBuffer[MAX_LB];
  short width;
  short height;
  const char *type;
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
  const char *baseTarget;
  UrlSchema real_schema;
  const char *sourcefile;
  int *clone;
  size_t trbyte;
  char check_url;
  TextList *document_header;
  FormItemList *form_submit;
  const char *savecache;
  const char *edit;
  struct mailcap *mailcap;
  const char *mailcap_source;
  const char *header_source;
  char search_header;
  const char *ssl_certificate;
  char image_flag;
  char image_loaded;
  char need_reshape;
  Anchor *submit;
  BufferPos *undo;
  AlarmEvent *event;
};

#define addnewline(a, b, c, d, e, f, g) _addnewline(a, b, c, e, f, g)
void addnewline(Buffer *buf, const char *line, Lineprop *prop, Linecolor *color,
                int pos, int width, int nlines);

#define COPY_BUFROOT(dstbuf, srcbuf)                                           \
  {                                                                            \
    (dstbuf)->rootX = (srcbuf)->rootX;                                         \
    (dstbuf)->rootY = (srcbuf)->rootY;                                         \
    (dstbuf)->COLS = (srcbuf)->COLS;                                           \
    (dstbuf)->LINES = (srcbuf)->LINES;                                         \
  }

#define COPY_BUFPOSITION(dstbuf, srcbuf)                                       \
  {                                                                            \
    (dstbuf)->topLine = (srcbuf)->topLine;                                     \
    (dstbuf)->currentLine = (srcbuf)->currentLine;                             \
    (dstbuf)->pos = (srcbuf)->pos;                                             \
    (dstbuf)->cursorX = (srcbuf)->cursorX;                                     \
    (dstbuf)->cursorY = (srcbuf)->cursorY;                                     \
    (dstbuf)->visualpos = (srcbuf)->visualpos;                                 \
    (dstbuf)->currentColumn = (srcbuf)->currentColumn;                         \
  }
#define SAVE_BUFPOSITION(sbufp) COPY_BUFPOSITION(sbufp, Currentbuf)
#define RESTORE_BUFPOSITION(sbufp) COPY_BUFPOSITION(Currentbuf, sbufp)
#define TOP_LINENUMBER(buf) ((buf)->topLine ? (buf)->topLine->linenumber : 1)
#define CUR_LINENUMBER(buf)                                                    \
  ((buf)->currentLine ? (buf)->currentLine->linenumber : 1)

struct TabBuffer {
  TabBuffer *nextTab;
  TabBuffer *prevTab;
  Buffer *currentBuffer;
  Buffer *firstBuffer;
  short x1;
  short x2;
  short y;
};

/*
 * global Buffer *Currentbuf;
 * global Buffer *Firstbuf;
 */
#define NO_TABBUFFER ((TabBuffer *)1)
#define Currentbuf (CurrentTab->currentBuffer)
#define Firstbuf (CurrentTab->firstBuffer)

extern TabBuffer *CurrentTab;
extern TabBuffer *FirstTab;
extern TabBuffer *LastTab;
extern bool open_tab_blank;
extern bool open_tab_dl_list;
extern bool close_tab_back;
extern int nTab;
extern int TabCols;

extern int REV_LB[];
