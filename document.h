// rendererd document
#pragma once

struct Line;
struct AnchorList;
struct LinkList;
struct FormList;
struct MapList;
struct HmarkerList;

struct Document {
  short width;
  short height;
  struct Line *firstLine;
  struct Line *topLine;
  struct Line *currentLine;
  struct Line *lastLine;
  int allLine;
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
};
