#pragma once
#include "geometry.h"
#include "textlist.h"
#include <Str.h>

typedef struct _MapList {
    Str name;
    GeneralList* area;
    struct _MapList* next;
} MapList;

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

struct Buffer;
struct KeyValue;
struct Anchor;

MapList* searchMapList(struct Buffer* buf, const char* name);
void follow_map(struct UI ui, struct KeyValue* arg);
MapArea* follow_map_menu(struct UI ui, struct Buffer* buf, const char* name, struct Anchor* a_img, int x, int y);
int getMapXY(struct Buffer* buf, struct Anchor* a, int* x, int* y);
MapArea* retrieveCurrentMapArea(struct Buffer* buf);
struct Anchor* retrieveCurrentMap(struct Buffer* buf);
MapArea* newMapArea(const char* url, const char* target, const char* alt, const char* shape, const char* coords);
struct Buffer* page_info_panel(struct UI ui, struct Buffer* buf);
int searchMapArea(struct Buffer* buf, MapList* ml, struct Anchor* a_img);
