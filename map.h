#ifndef W3M_MAP_H
#define W3M_MAP_H

#include "Str.h"
#include "config.h"
#include "image.h"
#include "textlist.h"

typedef struct _MapArea {
    const char* url;
    const char* target;
    const char* alt;
    char shape;
    short* coords;
    int ncoords;
    short center_x;
    short center_y;
} MapArea;

typedef struct _MapList {
    Str name;
    GeneralList* area;
    struct _MapList* next;
} MapList;

MapArea* newMapArea(const char* url,
    const char* target, const char* alt, const char* shape, const char* coords);

#endif
