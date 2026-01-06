#pragma once
struct TabBuffer;
struct Buffer;

struct TabBuffer* CurrentTab();
struct TabBuffer* FirstTab();
struct TabBuffer* LastTab();
int nTab();

struct TabBuffer* tabs_append(struct Buffer* buf);
struct TabBuffer* tabs_delete(struct TabBuffer* tab);
void tabs_calcPos(int cols);
void moveTab(struct TabBuffer* t, struct TabBuffer* t2, int right);
void tabs_set_current(struct TabBuffer* tab);
void tabs_next(int n);
void tabs_prev(int n);
