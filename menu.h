#pragma once
#include <w3m.h>

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
    const char* label;
    int* variable;
    int value;
    const char* cmd;
    struct _Menu* popup;
    char* keys;
    char* data;
} MenuItem;

typedef int (*MenuFunc)(struct CmdArgs *args);

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
    MenuFunc keymap[128];
    int keyselect[128];
} Menu;

extern Menu MainMenu;
extern Menu SelectMenu;
extern Menu SelTabMenu;

typedef struct _MenuList {
    char* id;
    Menu* menu;
    MenuItem* item;
} MenuList;

extern MenuList* w3mMenuList;

#define LINK_TYPE_NONE 0
#define LINK_TYPE_REL 1
#define LINK_TYPE_REV 2
struct LinkList {
    char* url;
    char* title; /* Next, Contents, ... */
    char* ctype; /* Content-Type */
    char type; /* Rel, Rev */
    struct LinkList* next;
};
struct Buffer;
struct LinkList* link_menu(struct CmdArgs *args, struct Buffer* buf);

void new_menu(Menu* menu, MenuItem* item);
void geom_menu(Menu* menu, int x, int y, int mselect);
void draw_all_menu(Menu* menu);
void draw_menu(Menu* menu);
void draw_menu_item(Menu* menu, int mselect);
int select_menu(Menu* menu, int mselect);
void goto_menu(Menu* menu, int mselect, int down);
void up_menu(Menu* menu, int n);
void down_menu(Menu* menu, int n);
int action_menu(struct CmdArgs *args, Menu* menu);
void popup_menu(struct CmdArgs *args, Menu* parent, Menu* menu);
void guess_menu_xy(Menu* menu, int width, int* x, int* y);
void new_option_menu(Menu* menu, const char** label, int* variable, const char* cmd);
int setMenuItem(MenuItem* item, const char* type, const char* line);
int addMenuList(MenuList** list, char* id);
int getMenuN(MenuList* list, const char* id);
void popupMenu(struct CmdArgs *args, int x, int y, Menu* menu);
void mainMenu(struct CmdArgs *args, int x, int y);
void optionMenu(struct CmdArgs *args, int x, int y, const char** label, int* variable, int initial, const char* cmd);
void initMenu(void);
