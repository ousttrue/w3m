#pragma once
#include "geometry.h"
#include <stdbool.h>
#include <Str.h>

extern int label_topline;

struct FormItem;

void _followForm(struct UI ui, bool submit, bool do_download);
void followAnchor(struct UI ui, bool do_download);
void gotoLabel(struct UI ui, const char* label);
void query_from_followform(struct UI ui, Str* query, struct FormItem* fi, int multipart);

