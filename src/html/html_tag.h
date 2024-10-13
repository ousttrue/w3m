#pragma once
#include "html_types.h"
#include "text/Str.h"

struct HtmlTag {
  unsigned char tagid;
  unsigned char *attrid;
  const char **value;
  unsigned char *map;
  bool need_reconstruct;
};

#define parsedtag_accepts(tag, id) ((tag)->map && (tag)->map[id] != MAX_TAGATTR)

#define parsedtag_exists(tag, id)                                              \
  (parsedtag_accepts(tag, id) &&                                               \
   ((tag)->attrid[(tag)->map[id]] != ATTR_UNKNOWN))

#define parsedtag_delete(tag, id)                                              \
  (parsedtag_accepts(tag, id) && ((tag)->attrid[(tag)->map[id]] = ATTR_UNKNOWN))

struct HtmlTag *parse_tag(const char **s, bool internal);
bool parsedtag_get_value(struct HtmlTag *tag, enum HtmlTagAttributeType id,
                         void *value);
bool parsedtag_set_value(struct HtmlTag *tag, enum HtmlTagAttributeType id,
                         const char *value);
Str parsedtag2str(struct HtmlTag *tag);
