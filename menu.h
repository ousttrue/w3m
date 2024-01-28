/* $Id: menu.h,v 1.2 2001/11/20 17:49:23 ukai Exp $ */
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

struct Menu;
struct MenuItem {
  int type;
  char *label;
  int *variable;
  int value;
  void (*func)();
  Menu *popup;
  char *keys;
  char *data;
};

struct Menu {
  Menu *parent;
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
  int (*keymap[128])(char c);
  int keyselect[128];
};

struct MenuList {
  char *id;
  Menu *menu;
  MenuItem *item;
};
