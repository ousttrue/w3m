#pragma once
#include "fm.h"
#include "textlist.h"

struct MapArea {
  char *url;
  char *target;
  char *alt;
};

struct MapList {
  Str name;
  struct GeneralList *area;
  struct MapList *next;
};

struct MapArea *follow_map_menu(struct Buffer *buf, char *name, Anchor *a_img, int x,
                                int y);

struct MapArea *newMapArea(char *url, char *target, char *alt, char *shape,
                           char *coords);

struct MapList *searchMapList(struct Buffer *buf, char *name);
