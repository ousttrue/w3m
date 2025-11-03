#pragma once
#include <gcstr/Str.h>
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

struct _Buffer;
struct MapList* searchMapList(struct _Buffer* buf, const char* name);
