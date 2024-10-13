#pragma once
#include "html/html.h"
#include "text/Str.h"

/* Parsed Tag structure */

struct parsed_tag {
  unsigned char tagid;
  unsigned char *attrid;
  const char **value;
  unsigned char *map;
  char need_reconstruct;
};

#define parsedtag_accepts(tag, id) ((tag)->map && (tag)->map[id] != MAX_TAGATTR)
#define parsedtag_exists(tag, id)                                              \
  (parsedtag_accepts(tag, id) &&                                               \
   ((tag)->attrid[(tag)->map[id]] != ATTR_UNKNOWN))
#define parsedtag_delete(tag, id)                                              \
  (parsedtag_accepts(tag, id) && ((tag)->attrid[(tag)->map[id]] = ATTR_UNKNOWN))
#define parsedtag_need_reconstruct(tag) ((tag)->need_reconstruct)
#define parsedtag_attname(tag, i) (AttrMAP[(tag)->attrid[i]].name)

extern struct parsed_tag *parse_tag(const char **s, bool internal);
extern int parsedtag_get_value(struct parsed_tag *tag, int id, void *value);
extern int parsedtag_set_value(struct parsed_tag *tag, int id, char *value);
extern Str parsedtag2str(struct parsed_tag *tag);
