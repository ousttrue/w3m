#pragma once
#include "url.h"
/*
 * frame support
 */
struct AnchorList;

struct frame_element {
  char attr;
#define F_UNLOADED 0x00
#define F_BODY 0x01
#define F_FRAMESET 0x02
  char dummy;
  char *name;
};

struct FormList;
struct frame_body {
  char attr;
  char flags;
#define FB_NO_BUFFER 0x01
  char *name;
  char *url;
  ParsedURL *baseURL;
  char *source;
  char *type;
  char *referer;
  AnchorList *nameList;
  FormList *request;
  char *ssl_certificate;
};

union frameset_element {
  struct frame_element *element;
  struct frame_body *body;
  struct frameset *set;
};

struct frameset {
  char attr;
  char dummy;
  char *name;
  ParsedURL *currentURL;
  char **width;
  char **height;
  int col;
  int row;
  int i;
  union frameset_element *frame;
};

struct frameset_queue {
  struct frameset_queue *next;
  struct frameset_queue *back;
  struct frameset *frameset;
  long linenumber;
  long top_linenumber;
  int pos;
  int currentColumn;
  AnchorList *formitem;
};

extern struct frameset *renderFrameSet;
