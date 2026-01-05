#pragma once
// client-side image maps
#include "Str.h"
#include "textlist.h"
#include <stdint.h>

enum ShapeType : uint8_t {
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

struct MapList {
    Str name;
    GeneralList* area;
    struct MapList* next;
};

struct parsed_tagarg;
struct Buffer;
struct Anchor;

struct MapArea* newMapArea(const char* url,
    const char* target, const char* alt,
    const char* shape, const char* coords);

void follow_map(struct parsed_tagarg* arg);
struct MapList* searchMapList(struct Buffer* buf, char* name);
struct MapArea* follow_map_menu(struct Buffer* buf, char* name, struct Anchor* a_img, int x, int y);
/// information of current page and link
Str page_info_panel(struct Buffer* buf);
int searchMapArea(struct Buffer* buf, struct MapList* ml, struct Anchor* a_img);
