#pragma once
#include "html_types.h"
#include "text/Str.h"

struct HtmlTag {
  enum HtmlTagType tagid;
  enum HtmlTagAttributeType *attrid;
  const char **value;
  unsigned char *map;
  bool need_reconstruct;
};

struct HtmlTag *parse_tag(const char **s, bool internal);
bool parsedtag_accepts(struct HtmlTag *tag, enum HtmlTagAttributeType id);
bool parsedtag_exists(struct HtmlTag *tag, enum HtmlTagAttributeType id);
bool parsedtag_get_value(struct HtmlTag *tag, enum HtmlTagAttributeType id,
                         void *value);
bool parsedtag_set_value(struct HtmlTag *tag, enum HtmlTagAttributeType id,
                         const char *value);
Str parsedtag2str(struct HtmlTag *tag);
