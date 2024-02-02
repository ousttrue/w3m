#pragma once
#include <memory>
#include "url.h"
#include "line.h"
#include <gc_cpp.h>
#include <stddef.h>

#define NO_BUFFER ((Buffer *)1)

#define _INIT_BUFFER_WIDTH (COLS - (showLineNum ? 6 : 1))
#define INIT_BUFFER_WIDTH ((_INIT_BUFFER_WIDTH > 0) ? _INIT_BUFFER_WIDTH : 0)
#define FOLD_BUFFER_WIDTH (FoldLine ? (INIT_BUFFER_WIDTH + 1) : -1)

/* Buffer Property */
enum BufferFlags : unsigned short {
  BP_NORMAL = 0x0,
  BP_PIPE = 0x1,
  BP_INTERNAL = 0x8,
  BP_NO_URL = 0x10,
  BP_REDIRECTED = 0x20,
  BP_CLOSE = 0x40,
};

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

struct Clone {
  int count = 1;
};
struct Buffer : public gc_cleanup {
  const char *filename = nullptr;
  const char *buffername = "";
  Line *firstLine = nullptr;
  Line *topLine = nullptr;
  Line *currentLine = nullptr;
  Line *lastLine = nullptr;
  Buffer *nextBuffer = nullptr;
  std::array<Buffer *, MAX_LB> linkBuffer = {0};
  short width = 0;
  short height = 0;
  const char *type = nullptr;
  const char *real_type = nullptr;
  int allLine = 0;
  BufferFlags bufferprop = BP_NORMAL;
  int currentColumn = 0;
  short cursorX = 0;
  short cursorY = 0;
  int pos = 0;
  int visualpos = 0;
  short rootX = 0;
  short rootY = 0;
  short COLS = 0;
  short LINES = 0;
  AnchorList *href = nullptr;
  AnchorList *name = nullptr;
  AnchorList *img = nullptr;
  AnchorList *formitem = nullptr;
  LinkList *linklist = nullptr;
  FormList *formlist = nullptr;
  MapList *maplist = nullptr;
  HmarkerList *hmarklist = nullptr;
  HmarkerList *imarklist = nullptr;
  ParsedURL currentURL = {.schema = SCM_UNKNOWN};
  ParsedURL *baseURL = nullptr;
  const char *baseTarget = nullptr;
  UrlSchema real_schema = {};
  const char *sourcefile = nullptr;
  std::shared_ptr<Clone> clone;

  size_t trbyte = 0;
  char check_url = 0;
  TextList *document_header = nullptr;
  FormItemList *form_submit = nullptr;
  const char *edit = nullptr;
  struct mailcap *mailcap = nullptr;
  const char *mailcap_source = nullptr;
  const char *header_source = nullptr;
  char search_header = 0;
  const char *ssl_certificate = nullptr;
  char image_flag = 0;
  char image_loaded = 0;

  // always reshape new buffers to mark URLs
  bool need_reshape = true;
  Anchor *submit = nullptr;
  BufferPos *undo = nullptr;
  AlarmEvent *event = nullptr;

  Buffer(int width);
  ~Buffer();
  Buffer(const Buffer &src) { *this = src; }

  // shallow copy
  Buffer &operator=(const Buffer &src);

  void pushLine(Line *l) {
    if (!this->lastLine || this->lastLine == this->currentLine) {
      this->lastLine = l;
    }
    this->currentLine = l;
    if (!this->firstLine) {
      this->firstLine = l;
    }
  }

  void addnewline(const char *line, Lineprop *prop, int byteLen, int breakWidth,
                  int realLinenum);
};

// void addnewline(Buffer *buf, const char *line, Lineprop *prop, int pos,
//                 int width, int nlines);

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

void gotoLine(Buffer *buf, int n);
void _followForm(int submit);
void set_buffer_environ(Buffer *buf);
char *GetWord(Buffer *buf);
char *getCurWord(Buffer *buf, int *spos, int *epos);

#define nextChar(s, l) (s)++
#define prevChar(s, l) (s)--
#define getChar(p) ((int)*(p))
