#pragma once
#include "geometry.h"
#include <Str.h>

enum LinkType {
    LINK_TYPE_NONE = 0,
    LINK_TYPE_REL = 1,
    LINK_TYPE_REV = 2,
};

struct LinkList {
    const char* url;
    const char* title; /* Next, Contents, ... */
    const char* ctype; /* Content-Type */
    enum LinkType type; /* Rel, Rev */
    struct LinkList* next;
};

struct Buffer;
struct HtmlTagParsed;

void append_link_info(struct Buffer* buf, Str html, struct LinkList* link);
struct LinkList* link_menu(struct UI ui);
struct Buffer* link_list_panel(struct UI ui, struct Buffer* buf);
void addLink(struct Buffer* buf, struct HtmlTagParsed* tag);
