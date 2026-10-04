// client-side image maps
#pragma once
#include "Str.h"
#include "textlist.h"

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
    pStr name;
    struct GeneralList* area;
    struct MapList* next;
};

struct MapArea* newMapArea(const char* url,
    const char* target, const char* alt,
    const char* shape, const char* coords);

struct _Buffer;
struct MapList* searchMapList(struct _Buffer* buf, char* name);
struct parsed_tagarg;
void follow_map(struct parsed_tagarg* arg);
struct Anchor;
struct MapArea* follow_map_menu(struct _Buffer* buf, char* name, struct Anchor* a_img, int x,
    int y);
struct _Buffer* follow_map_panel(struct _Buffer* buf, char* name);
int getMapXY(struct _Buffer* buf, struct Anchor* a, int* x, int* y);
struct MapArea* retrieveCurrentMapArea(struct _Buffer* buf);
struct Anchor* retrieveCurrentMap(struct _Buffer* buf);
