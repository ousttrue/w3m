#pragma once
#include "geometry.h"
#include "Content.h"
#include "Document.h"

void do_internal(struct UI ui, const char* action, const char* data);

struct Content link_list_panel(struct Document* doc);
struct LinkList* link_menu(struct UI ui, struct Document *doc);
