#pragma once
#include "geometry.h"

struct TabBuffer;
struct Buffer;

struct TabBuffer* CurrentTab();
struct TabBuffer* FirstTab();
struct TabBuffer* LastTab();
int nTab();

size_t tabs_current();
struct TabBuffer* tabs_tab(size_t index);
struct TabBuffer* tabs_append(struct Buffer* buf);
struct TabBuffer* tabs_delete(struct TabBuffer* tab);
struct TabPosList tabs_calcPos(int cols);
void moveTab(struct TabBuffer* t, struct TabBuffer* t2, int right);
void tabs_set_current(struct TabBuffer* tab);
void tabs_next(int n);
void tabs_prev(int n);
