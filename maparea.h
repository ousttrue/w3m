#pragma once
#include "Str.h"
#include "textlist.h"

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

typedef struct _MapList {
    Str name;
    GeneralList* area;
    struct _MapList* next;
} MapList;

typedef struct _Buffer Buffer;
MapList* searchMapList(Buffer* buf, char* name);
struct parsed_tagarg;
void follow_map(struct parsed_tagarg* arg);
struct _anchor;
MapArea* follow_map_menu(Buffer* buf, char* name, struct _anchor* a_img, int x, int y);
int getMapXY(Buffer* buf, struct _anchor* a, int* x, int* y);
MapArea* retrieveCurrentMapArea(Buffer* buf);
struct _anchor* retrieveCurrentMap(Buffer* buf);
MapArea* newMapArea(char* url, char* target, char* alt, char* shape, char* coords);
