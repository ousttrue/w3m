#pragma once
#include "fm.h"

typedef struct _MapArea {
  char *url;
  char *target;
  char *alt;
} MapArea;

extern MapArea *follow_map_menu(Buffer *buf, char *name, Anchor *a_img, int x,
                                int y);

extern MapArea *newMapArea(char *url, char *target, char *alt, char *shape,
                           char *coords);
