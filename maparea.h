#pragma once
#include <w3m.h>
#include "Str.h"
#include "textlist.h"

enum ShapeType {
    SHAPE_UNKNOWN = 0,
    SHAPE_DEFAULT = 1,
    SHAPE_RECT = 2,
    SHAPE_CIRCLE = 3,
    SHAPE_POLY = 4,
};

struct MapArea {
    const char* url;
    const char* target;
    const char* alt;
    enum ShapeType shape;
    short* coords;
    int ncoords;
    short center_x;
    short center_y;
};

typedef struct _MapList {
    Str name;
    GeneralList* area;
    struct _MapList* next;
} MapList;

struct Buffer;
MapList* searchMapList(struct Buffer* buf, const char* name);
struct parsed_tagarg;
void follow_map(struct CmdArgs args, struct parsed_tagarg* arg);
struct Anchor;
struct MapArea* follow_map_menu(struct CmdArgs args, struct Buffer* buf, const char* name, struct Anchor* a_img, int x, int y);
int getMapXY(struct Buffer* buf, struct Anchor* a, int* x, int* y);
struct MapArea* retrieveCurrentMapArea(struct Buffer* buf);
struct Anchor* retrieveCurrentMap(struct Buffer* buf);
struct MapArea* newMapArea(char* url, char* target, char* alt, char* shape, char* coords);
