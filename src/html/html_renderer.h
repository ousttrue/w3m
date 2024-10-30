#pragma once
#define SYMBOL_BASE 0x20
extern int symbol_width;
extern int symbol_width0;
extern bool MetaRefresh;

struct Url;
struct TextLineList;
struct Document *render_to_lines(int cols, struct Url url,
                                 struct TextLineList *lines);
