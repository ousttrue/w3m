#pragma once
#include <stdbool.h>

#define MENU_END 0
#define MENU_NOP 1
#define MENU_VALUE 2
#define MENU_FUNC 4
#define MENU_POPUP 8

enum MenuResult {
    MENU_NOTHING = -1,
    MENU_CANCEL = -2,
    MENU_CLOSE = -3,
};

struct MenuItem {
    int type;
    const char* label;
    int* variable;
    int value;
    void (*func)();
    struct Menu* popup;
    const char* keys;
    const char* data;
};

typedef enum MenuResult (*MenuKeyFunc)(char ch);

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

typedef struct _MenuList {
    const char* id;
    struct Menu* menu;
    struct MenuItem* item;
} MenuList;

void new_menu(struct Menu* menu, struct MenuItem* item);
void geom_menu(struct Menu* menu, int x, int y, int mselect);
void draw_all_menu(struct Menu* menu);
void draw_menu(struct Menu* menu);
void draw_menu_item(struct Menu* menu, int mselect);
int select_menu(struct Menu* menu, int mselect);
void goto_menu(struct Menu* menu, int mselect, int down);
void up_menu(struct Menu* menu, int n);

struct Buffer;
typedef struct Anchor* (*BufferMenuFunc)(struct Buffer*);
void down_menu(struct Menu* menu, int n);

bool action_menu(struct Menu* menu);
void popup_menu(struct Menu* parent, struct Menu* menu);
void guess_menu_xy(struct Menu* menu, int width, int* x, int* y);
void new_option_menu(struct Menu* menu, const char** label, int* variable,
    void (*func)());
int setMenuItem(struct MenuItem* item, const char* type, const char* line);
int addMenuList(MenuList** list, const char* id);
int getMenuN(MenuList* list, const char* id);
void popupMenu(int x, int y, struct Menu* menu);
void mainMenu(int x, int y);
void mainMn(void);
void selMn(void);
void tabMn(void);
void optionMenu(int x, int y, const char** label, int* variable, int initial,
    void (*func)());
void initMenu(void);
struct LinkList* link_menu(struct Buffer* buf);
struct Anchor* accesskey_menu(struct Buffer* buf);
struct Anchor* list_menu(struct Buffer* buf);
