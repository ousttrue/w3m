#pragma once
#include "geometry.h"
#include <stdbool.h>
#include <Str.h>

enum DefaultUrlType {
    DEFAULT_URL_EMPTY = 0,
    DEFAULT_URL_CURRENT = 1,
    DEFAULT_URL_LINK = 2,
};

extern int label_topline;
extern enum DefaultUrlType DefaultURLString;

struct FormItem;
struct Anchor;
struct MapArea;
struct Buffer;

void _followForm(struct UI ui, bool submit, bool do_download);
void followAnchor(struct UI ui, bool do_download);
void gotoLabel(struct UI ui, const char* label);
void query_from_followform(struct Buffer* buf, struct BufferPoint bp, Str* query, struct FormItem* fi, int multipart);
void followImage(struct UI ui, bool do_download);
struct MapArea* follow_map_menu(struct UI ui, struct Document* doc, const char* name, struct Anchor* a_img, int x, int y);
void goURL0(struct UI ui, const char* prompt, bool relative);

typedef struct Anchor* (*AnchorMenuFunc)(struct UI ui, struct Buffer*);
void anchorMn(struct UI ui, AnchorMenuFunc menu_func, int go);
