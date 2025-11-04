#pragma once

enum LinkType {
    LINK_TYPE_NONE = 0,
    LINK_TYPE_REL = 1,
    LINK_TYPE_REV = 2,
};

struct LinkList {
    char* url;
    char* title; /* Next, Contents, ... */
    char* ctype; /* Content-Type */
    char type; /* Rel, Rev */
    struct LinkList* next;
};
