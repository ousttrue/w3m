#pragma once

struct Url;
struct frame_element {
  char attr;
#define F_UNLOADED 0x00
#define F_BODY 0x01
#define F_FRAMESET 0x02
  char dummy;
  char *name;
};

struct frame_body {
  char attr;
  char flags;
#define FB_NO_BUFFER 0x01
  char *name;
  char *url;
  struct Url *baseURL;
  char *source;
  char *type;
  char *referer;
  struct AnchorList *nameList;
  struct FormList *request;
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
  struct Url *currentURL;
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
  struct AnchorList *formitem;
};

extern struct frameset *renderFrameSet;
