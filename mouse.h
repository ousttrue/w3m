#ifndef W3M_MOUSE_H
#define W3M_MOUSE_H

#include "config.h"

#ifdef USE_MOUSE

#define LIMIT_MOUSE_MENU 100
typedef struct {
    void (*func) (void);
    char *data;
} MouseActionMap;
typedef struct {
    char *menu_str;
    char *lastline_str;
    int menu_width;
    int lastline_width;
    int in_action;
    int cursorX;
    int cursorY;
    MouseActionMap default_map[3];
    MouseActionMap anchor_map[3];
    MouseActionMap active_map[3];
    MouseActionMap tab_map[3];
    MouseActionMap *menu_map[3];
    MouseActionMap *lastline_map[3];
} MouseAction;
extern MouseAction mouse_action;
#endif
#endif
