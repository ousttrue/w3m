#pragma once
// client-side image maps
#include "Str.h"
#include "textlist.h"

struct MapArea {
    char* url;
    char* target;
    char* alt;
    char shape;
    short* coords;
    int ncoords;
    short center_x;
    short center_y;
};

struct MapList {
    Str name;
    GeneralList* area;
    struct MapList* next;
};

struct MapArea* newMapArea(const char* url,
    const char* target, const char* alt,
    const char* shape, const char* coords);
