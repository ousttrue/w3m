#pragma once

#define LINK_TYPE_NONE 0
#define LINK_TYPE_REL 1
#define LINK_TYPE_REV 2
typedef struct _LinkList {
    char* url;
    char* title; /* Next, Contents, ... */
    char* ctype; /* Content-Type */
    char type; /* Rel, Rev */
    struct _LinkList* next;
} LinkList;
