#pragma once
#include "htmlcommand.h"

/* Parsed Tag structure */

struct HtmlTag {
  HtmlCommand tagid;
  unsigned char *attrid;
  char **value;
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

struct Str;
HtmlTag *parse_tag(const char **s, int internal);
int parsedtag_get_value(HtmlTag *tag, int id, void *value);
int parsedtag_set_value(HtmlTag *tag, int id, char *value);
Str *parsedtag2str(HtmlTag *tag);

Str *process_img(HtmlTag *tag, int width);
Str *process_anchor(HtmlTag *tag, const char *tagbuf);
Str *process_input(HtmlTag *tag);
Str *process_button(HtmlTag *tag);
Str *process_n_button(void);
Str *process_select(HtmlTag *tag);
Str *process_n_select(void);
void feed_select(const char *str);
void process_option(void);
Str *process_textarea(HtmlTag *tag, int width);
Str *process_n_textarea(void);
void feed_textarea(const char *str);
Str *process_form(HtmlTag *tag);
Str *process_n_form(void);
int getMetaRefreshParam(const char *q, Str **refresh_uri);
