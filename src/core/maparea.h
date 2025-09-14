#pragma once
#include "geometry.h"
#include "textlist.h"
#include "Content.h"
#include <Str.h>

struct MapList {
    Str name;
    GeneralList* area;
    struct MapList* next;
};

enum MapAreaShapeType {
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
    enum MapAreaShapeType shape;
    short* coords;
    int ncoords;
    short center_x;
    short center_y;
};

struct Buffer;
struct KeyValue;
struct Anchor;
struct Document;

struct MapList* searchMapList(struct Document* doc, const char* name);
void follow_map(struct UI ui, struct KeyValue* arg);
struct MapArea* follow_map_menu(struct UI ui, struct Document* doc, const char* name, struct Anchor* a_img, int x, int y);
bool getMapXY(struct Document* doc, struct Anchor* a, int* x, int* y);
struct MapArea* retrieveCurrentMapArea(struct Buffer* buf);
struct Anchor* retrieveCurrentMap(struct Buffer* buf);
struct MapArea* newMapArea(const char* url, const char* target, const char* alt, const char* shape, const char* coords);
struct Content page_info_panel(struct UI ui, struct Buffer* buf);
int searchMapArea(struct Document* doc, struct MapList* ml, struct Anchor* a_img);
