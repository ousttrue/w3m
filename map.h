#pragma once
#include "fm.h"

struct MapArea {
  char *url;
  char *target;
  char *alt;
};

extern struct MapArea *follow_map_menu(Buffer *buf, char *name, Anchor *a_img,
                                       int x, int y);

extern struct MapArea *newMapArea(char *url, char *target, char *alt,
                                  char *shape, char *coords);
