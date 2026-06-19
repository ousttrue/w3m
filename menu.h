/* vi: set sw=4 ts=8 ai sm noet : */
#ifndef W3M_MENU_H
#define W3M_MENU_H

#include "config.h"

#ifdef USE_MENU

#define MENU_END    0
#define MENU_NOP    1
#define MENU_VALUE  2
#define MENU_FUNC   4
#define MENU_POPUP  8

#define MENU_NOTHING -1
#define MENU_CANCEL  -2
#define MENU_CLOSE   -3

typedef struct _MenuItem {
    int type;
    char *label;
    int *variable;
    int value;
    void (*func) (void);
    struct _Menu *popup;
    char *keys;
    char *data;
} MenuItem;

typedef struct _Menu {
    struct _Menu *parent;
    int cursorX;
    int cursorY;
    int x;
    int y;
    int width;
    int height;
    int nitem;
    MenuItem *item;
    int initial;
    int select;
    int offset;
    int active;
    int (*keymap[128]) (char c);
    int keyselect[128];
} Menu;

typedef struct _MenuList {
    char *id;
    Menu *menu;
    MenuItem *item;
} MenuList;

extern void new_menu(Menu *menu, MenuItem *item);
extern void draw_all_menu(Menu *menu);
extern void draw_menu(Menu *menu);
extern void draw_menu_item(Menu *menu, int mselect);
extern int select_menu(Menu *menu, int mselect);
extern void goto_menu(Menu *menu, int mselect, int down);
extern void up_menu(Menu *menu, int n);
extern void down_menu(Menu *menu, int n);
extern int action_menu(Menu *menu);
extern void popup_menu(Menu *parent, Menu *menu);
extern void guess_menu_xy(Menu *menu, int width, int *x, int *y);
extern void new_option_menu(Menu *menu, char **label, int *variable,
			    void (*func) (void));

extern int setMenuItem(MenuItem *item, char *type, char *line);
extern int addMenuList(MenuList **list, char *id);
extern int getMenuN(MenuList *list, char *id);

extern void popupMenu(int x, int y, Menu *menu);
extern void mainMn(void);
extern void selMn(void);
extern void tabMn(void);
extern void optionMenu(int x, int y, char **label, int *variable, int initial,
		       void (*func) (void));
extern void initMenu(void);

#else				/* not USE_MENU */
#define mainMn nulcmd
#define selMn selBuf
#define tabMn nulcmd
#endif				/* not USE_MENU */
#endif
