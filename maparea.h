#pragma once
// client-side image maps
#include "Str.h"
#include "textlist.h"

#define SHAPE_UNKNOWN 0
#define SHAPE_DEFAULT 1
#define SHAPE_RECT 2
#define SHAPE_CIRCLE 3
#define SHAPE_POLY 4

struct MapArea {
    const char* url;
    const char* target;
    const char* alt;
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

struct parsed_tagarg;
void follow_map(struct parsed_tagarg* arg);
struct Buffer;
extern struct MapList* searchMapList(struct Buffer* buf, char* name);
struct Anchor;
extern struct MapArea* follow_map_menu(struct Buffer* buf, char* name, struct Anchor* a_img, int x,
    int y);
extern struct Buffer* follow_map_panel(struct Buffer* buf, char* name);
extern struct Buffer* page_info_panel(struct Buffer* buf);
