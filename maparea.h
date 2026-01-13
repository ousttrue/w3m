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
struct Content;
struct Document;
struct Anchor;
struct Url;
struct FormItemList;
struct LinkList;
struct frameset;
struct MapArea* newMapArea(const char* url,
    const char* target, const char* alt,
    const char* shape, const char* coords);

struct MapList* searchMapList(struct Document* doc, const char* name);
struct MapArea* follow_map_menu(struct Document* doc, const char* name, struct Anchor* a_img, int x, int y);
/// information of current page and link
int searchMapArea(struct Document *doc, struct MapList* ml, struct Anchor* a_img);
int getMapXY(struct Document *doc, struct Anchor* a, int* x, int* y);
void append_map_info(struct Url* base_url, struct Document* doc, Str tmp, struct FormItemList* fi);
void append_link_info(struct Url* base_url, struct Document* doc, Str html, struct LinkList* link);
void append_frame_info(struct Url* base_url, struct Document* doc, Str html, struct frameset* set, int level);
