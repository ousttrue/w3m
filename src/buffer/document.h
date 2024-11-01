// rendererd document
#pragma once
#include "input/charset.h"
#include "input/url.h"
#include "line.h"
#include "viewport.h"

extern bool showLineNum;
#define _INIT_BUFFER_WIDTH (COLS - (showLineNum ? 6 : 1))
#define INIT_BUFFER_WIDTH ((_INIT_BUFFER_WIDTH > 0) ? _INIT_BUFFER_WIDTH : 0)
extern bool FoldLine;
int FOLD_BUFFER_WIDTH();
extern bool MarkAllPages;

struct AnchorList;
struct LinkList;
struct FormList;
struct MapList;
struct HmarkerList;

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

struct Document {
  struct FormItemList *form_submit;
  struct Anchor *submit;
  bool need_reshape;
  const char *sourcefile;
  struct Url url;
  enum URL_SCHEME_TYPE real_scheme;
  const char *type;
  const char *filename;
  enum BufferProperty bufferprop;
  const char *savecache;
  const char *title;
  const char *baseTarget;
  // <base>
  // struct Url baseURL;
  short width;
  short height;
  struct Line *firstLine;
  struct Line *topLine;
  struct Line *currentLine;
  struct Line *lastLine;
  int allLine;
  struct AnchorList *href;
  struct AnchorList *name;
  struct AnchorList *img;
  struct AnchorList *formitem;
  struct LinkList *linklist;
  struct FormList *formlist;
  struct MapList *maplist;
  struct HmarkerList *hmarklist;
  struct HmarkerList *imarklist;
  struct Viewport viewport;
  enum CheckUrlFlags check_url;
};

struct Document *newDocument(int width);

#define TOP_LINENUMBER(doc) (doc->topLine ? doc->topLine->linenumber : 1)
#define CUR_LINENUMBER(doc)                                                    \
  (doc->currentLine ? doc->currentLine->linenumber : 1)

void addnewline(struct Document *doc, char *line, Lineprop *prop, int pos,
                int width, int nlines);
void gotoLine(struct Document *doc, int n);
void arrangeCursor(struct Document *doc);
void cursorUp0(struct Document *doc, int n);
void cursorUp(struct Document *doc, int n);
void cursorDown0(struct Document *doc, int n);
void cursorDown(struct Document *doc, int n);
void cursorUpDown(struct Document *doc, int n);
void cursorRight(struct Document *doc, int n);
void cursorLeft(struct Document *doc, int n);
void cursorHome(struct Document *doc);
void arrangeLine(struct Document *doc);
void cursorXY(struct Document *doc, int x, int y);
int columnSkip(struct Document *doc, int offset);
void restorePosition(struct Document *doc, struct Document *orig);
int writeBufferCache(struct Document *doc);
int readBufferCache(struct Document *doc);
void tmpClearBuffer(struct Document *doc);
void copyBuffer(struct Document *a, const struct Document *b);
void COPY_BUFROOT(struct Document *dstbuf, const struct Document *srcbuf);
void COPY_BUFPOSITION(struct Document *dstbuf, const struct Document *srcbuf);
struct HmarkerList *putHmarker(struct HmarkerList *ml, int line, int pos,
                               int seq);
int currentLn(struct Document *doc);
void gotoRealLine(struct Document *buf, int n);
void clearBuffer(struct Document *buf);
void chkURLBuffer(struct Document *buf);
struct Document *reshapeBuffer(struct Document *buf,
                               enum CharSet content_charset);
void _goLine(struct Document *doc, const char *l);
void save_buffer_position(struct Document *doc);
void resetPos(struct Document *doc, struct BufferPos *b);
int prev_nonnull_line(struct Document *doc, struct Line *line);
struct Url *baseURL(struct Document *buf);
