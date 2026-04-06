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

struct Buffer;
MapList* searchMapList(struct Buffer* buf, char* name);
struct parsed_tagarg;
void follow_map(struct parsed_tagarg* arg);
struct Anchor;
MapArea* follow_map_menu(struct Buffer* buf, char* name, struct Anchor* a_img, int x, int y);
int getMapXY(struct Buffer* buf, struct Anchor* a, int* x, int* y);
MapArea* retrieveCurrentMapArea(struct Buffer* buf);
struct Anchor* retrieveCurrentMap(struct Buffer* buf);
MapArea* newMapArea(char* url, char* target, char* alt, char* shape, char* coords);
