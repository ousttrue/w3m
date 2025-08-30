#pragma once
#include <Str.h>
#include "textlist.h"

typedef struct _MapList {
    Str name;
    GeneralList* area;
    struct _MapList* next;
} MapList;

typedef struct _MapArea {
    char* url;
    char* target;
    char* alt;
    char shape;
    short* coords;
    int ncoords;
    short center_x;
    short center_y;
} MapArea;

struct _Buffer;
struct parsed_tagarg;
struct _anchor;

MapList* searchMapList(struct _Buffer* buf, char* name);
void follow_map(struct parsed_tagarg* arg);
MapArea* follow_map_menu(struct _Buffer* buf, char* name, struct _anchor* a_img, int x, int y);
int getMapXY(struct _Buffer* buf, struct _anchor* a, int* x, int* y);
MapArea* retrieveCurrentMapArea(struct _Buffer* buf);
struct _anchor* retrieveCurrentMap(struct _Buffer* buf);
MapArea* newMapArea(char* url, char* target, char* alt, char* shape, char* coords);
struct _Buffer* page_info_panel(struct _Buffer* buf);
