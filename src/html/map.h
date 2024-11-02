#pragma once
#include "text/Str.h"
#include "current.h"

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
struct InternalAction;
struct Current;
void follow_map(struct InternalAction *arg, struct Current );
Str follow_map_panel(struct Buffer *buf, const char *name);
struct Anchor *retrieveCurrentMap(struct Document *doc);
