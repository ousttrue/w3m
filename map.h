#ifndef W3M_MAP_H_
#define W3M_MAP_H_

#include "Str.h"
#include "config.h"
#include "image.h"
#include "textlist.h"

typedef struct _MapArea {
    char *url;
    char *target;
    char *alt;
#ifdef USE_IMAGE
    char shape;
    short *coords;
    int ncoords;
    short center_x;
    short center_y;
#endif
} MapArea;

typedef struct _MapList {
    Str name;
    GeneralList *area;
    struct _MapList *next;
} MapList;
#endif
