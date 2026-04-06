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
    const char* cmd;
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
typedef struct _Buffer Buffer;
struct LinkList* link_menu(Buffer* buf);

void new_menu(Menu* menu, MenuItem* item);
void geom_menu(Menu* menu, int x, int y, int mselect);
void draw_all_menu(Menu* menu);
void draw_menu(Menu* menu);
void draw_menu_item(Menu* menu, int mselect);
int select_menu(Menu* menu, int mselect);
void goto_menu(Menu* menu, int mselect, int down);
void up_menu(Menu* menu, int n);
void down_menu(Menu* menu, int n);
int action_menu(Menu* menu);
void popup_menu(Menu* parent, Menu* menu);
void guess_menu_xy(Menu* menu, int width, int* x, int* y);
void new_option_menu(Menu* menu, char** label, int* variable, const char* cmd);
int setMenuItem(MenuItem* item, const char* type, const char* line);
int addMenuList(MenuList** list, char* id);
int getMenuN(MenuList* list, char* id);
void popupMenu(int x, int y, Menu* menu);
void mainMenu(int x, int y);
void optionMenu(int x, int y, char** label, int* variable, int initial, const char* cmd);
void initMenu(void);
