#pragma once
#include "Content.h"
#include "Document.h"

void do_internal(struct UI ui, const char* action, const char* data);
Str link_list_panel_html(struct Document* doc);
struct LinkList* link_menu(struct UI ui, struct Document* doc);
