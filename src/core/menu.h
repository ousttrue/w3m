#pragma once
#include "geometry.h"
#include "keymap.h"

extern int FRAME_WIDTH;

enum MenuType {
    MENU_END = 0,
    MENU_NOP = 1,
    MENU_VALUE = 2,
    MENU_FUNC = 4,
    MENU_POPUP = 8,
};

struct MenuItem {
    enum MenuType type;
    const char* label;
    int* variable;
    int value;
    CommandFunc func;
    struct Menu* popup;
    const char* keys;
    const char* data;
};

struct Menu {
    struct Menu* parent;
    // int cursorX;
    // int cursorY;
    int x;
    int y;
    int width;
    int height;
    int nitem;
    struct MenuItem* item;
    int initial;
    int select;
    int offset;
    int active;
    int (*keymap[128])(char c);
    int keyselect[128];
};

struct MenuList {
    const char* id;
    struct Menu* menu;
    struct MenuItem* item;
};

struct Buffer;
struct Anchor* accesskey_menu(struct Buffer* buf);
struct Anchor* list_menu(struct Buffer* buf);
void set_menu_frame(void);
void new_menu(struct Menu* menu, struct MenuItem* item);
void geom_menu(struct Menu* menu, int x, int y, int mselect);
void draw_all_menu(struct Menu* menu);
void draw_menu(struct Menu* menu);
void draw_menu_item(struct Menu* menu, int mselect);
int select_menu(struct Menu* menu, int mselect);
void goto_menu(struct Menu* menu, int mselect, int down);
void up_menu(struct Menu* menu, int n);
void down_menu(struct Menu* menu, int n);
int action_menu(struct Menu* menu);
void popup_menu(struct Menu* parent, struct Menu* menu);
void guess_menu_xy(struct Menu* menu, int width, int* x, int* y);
void new_option_menu(struct Menu* menu, const char** label, int* variable, CommandFunc func);
int setMenuItem(struct MenuItem* item, const char* type, const char* line);
int addMenuList(struct MenuList** list, const char* id);
int getMenuN(struct MenuList* list, const char* id);
void popupMenu(struct UI ui, struct Menu* menu);
void optionMenu(int x, int y, const char** label, int* variable, int initial, CommandFunc func);
void initMenu(void);
