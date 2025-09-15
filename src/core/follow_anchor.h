#pragma once
#include "geometry.h"
#include <stdbool.h>
#include <Str.h>

extern int label_topline;

struct FormItem;
struct Anchor;
struct MapArea;
struct Buffer;

void _followForm(struct UI ui, bool submit, bool do_download);
void followAnchor(struct UI ui, bool do_download);
void gotoLabel(struct UI ui, const char* label);
void query_from_followform(struct Buffer *buf, struct BufferPoint bp, Str* query, struct FormItem* fi, int multipart);
void followImage(struct UI ui, bool do_download);
struct MapArea* follow_map_menu(struct UI ui, struct Document* doc, const char* name, struct Anchor* a_img, int x, int y);
