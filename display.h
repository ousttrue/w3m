#pragma once

struct Buffer;
void displayMsg(struct Buffer* buf);
struct Document;
struct Url;
void screen_from_lines(struct Document* buf, struct Url* url);
void drawAnchorCursor(struct Buffer* buf);
void bufferPosition(struct Buffer* buf);
