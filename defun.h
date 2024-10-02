#pragma once

#define BOOKMARK "bookmark.html"

struct Buffer;
void _newT(void);
void delBuffer(struct Buffer *buf);
void mainloop(char *line_str);
const char *searchKeyData();
