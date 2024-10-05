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
struct MapArea *follow_map_menu(struct Document *doc, char *name,
                                struct Anchor *a_img, int x, int y);

struct MapArea *newMapArea(const char *url, const char *target, const char *alt,
                           const char *shape, const char *coords);

struct MapList *searchMapList(struct Document *doc, char *name);
struct parsed_tagarg;
extern void follow_map(struct parsed_tagarg *arg);
extern struct Buffer *follow_map_panel(struct Buffer *buf, char *name);
extern struct Anchor *retrieveCurrentMap(struct Buffer *buf);
