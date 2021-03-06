#pragma once
#include "frontend/tab.h"
#include <functional>

TabPtr CreateTabSetCurrent();
int GetTabCount();
int GetTabbarHeight();
TabPtr GetTabByIndex(int index);
TabPtr GetFirstTab();
TabPtr GetLastTab();
TabPtr GetCurrentTab();
TabPtr GetTabByPosition(int x, int y);
void SetCurrentTab(TabPtr tab);
void SelectRelativeTab(int prec);
void MoveTab(int x);
void deleteTab(TabPtr tab);
void DeleteCurrentTab();
void DeleteAllTabs();
void moveTab(TabPtr src, TabPtr dst, int right);
void calcTabPos();
