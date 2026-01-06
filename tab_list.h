#pragma once
struct TabBuffer;
struct Buffer;

struct TabBuffer* tabs_append(struct Buffer* buf);
struct TabBuffer* tabs_delete(struct TabBuffer* tab);
void tabs_calcPos(int cols);
