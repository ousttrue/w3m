#pragma once

#define BOOKMARK "bookmark.html"

struct Buffer;
void delBuffer(struct Buffer *buf);
const char *searchKeyData();
void w3m_exit(int i);
