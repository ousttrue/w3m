#pragma once

#define LINK_TYPE_NONE 0
#define LINK_TYPE_REL 1
#define LINK_TYPE_REV 2
struct LinkList {
  const char *url;
  const char *title; /* Next, Contents, ... */
  const char *ctype; /* Content-Type */
  char type;   /* Rel, Rev */
  LinkList *next;
};
