#pragma once
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
struct Document;
struct HtmlTagParsed;

void addLink(struct Document* doc, struct HtmlTagParsed* tag);
