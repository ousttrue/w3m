#pragma once

struct Buffer;
struct Document;
struct Url;
void screen_from_lines(struct Document* buf, struct Url* url);
void drawAnchorCursor(struct Document* doc, struct Url* base_url);
void bufferPosition(struct Buffer* buf);
