#pragma once
#include <Str.h>
#include "textlist.h"

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
struct _anchor;

MapList* searchMapList(struct Buffer* buf, const char* name);
void follow_map(struct KeyValue* arg);
MapArea* follow_map_menu(struct Buffer* buf, const char* name, struct _anchor* a_img, int x, int y);
int getMapXY(struct Buffer* buf, struct _anchor* a, int* x, int* y);
MapArea* retrieveCurrentMapArea(struct Buffer* buf);
struct _anchor* retrieveCurrentMap(struct Buffer* buf);
MapArea* newMapArea(const char* url, const char* target, const char* alt, const char* shape, const char* coords);
struct Buffer* page_info_panel(struct Buffer* buf);
