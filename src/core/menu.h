/*
 * w3m menu.h
 */
#pragma once

#define MENU_END 0
#define MENU_NOP 1
#define MENU_VALUE 2
#define MENU_FUNC 4
#define MENU_POPUP 8

#define MENU_NOTHING -1
#define MENU_CANCEL -2
#define MENU_CLOSE -3

typedef struct _MenuItem {
    int type;
    char* label;
    int* variable;
    int value;
    void (*func)();
    struct _Menu* popup;
    char* keys;
    char* data;
} MenuItem;

typedef struct _Menu {
    struct _Menu* parent;
    int cursorX;
    int cursorY;
    int x;
    int y;
    int width;
    int height;
    int nitem;
    MenuItem* item;
    int initial;
    int select;
    int offset;
    int active;
    int (*keymap[128])(char c);
    int keyselect[128];
} Menu;

typedef struct _MenuList {
    char* id;
    Menu* menu;
    MenuItem* item;
} MenuList;

struct _Buffer;
struct _LinkList* link_menu(struct _Buffer* buf);
struct _anchor* accesskey_menu(struct _Buffer* buf);
struct _anchor* list_menu(struct _Buffer* buf);
