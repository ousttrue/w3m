#pragma once
#include "menu_keybind.h"
#include "defun.h"
#include <stdbool.h>

enum MenuItemType {
    MENU_END = 0,
    MENU_NOP = 1,
    MENU_VALUE = 2,
    MENU_FUNC = 4,
    MENU_POPUP = 8,
};

struct MenuItem {
    enum MenuItemType type;
    const char* label;
    int* variable;
    int value;
    DefunFunc func;
    struct Menu* popup;
    const char* keys;
    const char* data;
};

struct Menu {
    struct Menu* parent;
    int cursorX;
    int cursorY;
    int x;
    int y;
    int width;
    int height;
    int nitem;
    struct MenuItem* item;
    int initial;
    int select;
    int offset;
    bool active;
    MenuKeyFunc keymap[128];
    int keyselect[128];
};

struct MenuList {
    const char* id;
    struct Menu* menu;
    struct MenuItem* item;
};

void optionMenu(int x, int y, const char** label, int* variable, int initial, void (*func)());
void initMenu(void);
struct LinkList* link_menu(struct Buffer* buf);
struct Anchor* accesskey_menu(struct Buffer* buf);
struct Anchor* list_menu(struct Buffer* buf);
