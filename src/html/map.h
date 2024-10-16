#pragma once
#include "text/Str.h"

struct MapArea {
  const char *url;
  const char *target;
  const char *alt;
};

struct MapList {
  Str name;
  struct GeneralList *area;
  struct MapList *next;
};

struct Buffer;
struct Document;
struct Anchor;
struct MapArea *follow_map_menu(struct Document *doc, const char *name,
                                struct Anchor *a_img, int x, int y);

struct MapArea *newMapArea(const char *url, const char *target, const char *alt,
                           const char *shape, const char *coords);

struct MapList *searchMapList(struct Document *doc, const char *name);
struct LocalCgiHtml;
extern void follow_map(struct LocalCgiHtml *arg);
extern struct Document *follow_map_panel(struct Buffer *buf, const char *name);
extern struct Anchor *retrieveCurrentMap(struct Buffer *buf);
