/* $Id: parsetagx.h,v 1.4 2001/11/24 02:01:26 ukai Exp $ */
#pragma once
#include "htmlcommand.h"

/* Parsed Tag structure */

struct parsed_tag {
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
extern struct parsed_tag *parse_tag(char **s, int internal);
extern int parsedtag_get_value(struct parsed_tag *tag, int id, void *value);
extern int parsedtag_set_value(struct parsed_tag *tag, int id, char *value);
extern Str *parsedtag2str(struct parsed_tag *tag);

extern Str *process_img(struct parsed_tag *tag, int width);
extern Str *process_anchor(struct parsed_tag *tag, char *tagbuf);
extern Str *process_input(struct parsed_tag *tag);
extern Str *process_button(struct parsed_tag *tag);
extern Str *process_n_button(void);
extern Str *process_select(struct parsed_tag *tag);
extern Str *process_n_select(void);
extern void feed_select(char *str);
extern void process_option(void);
extern Str *process_textarea(struct parsed_tag *tag, int width);
extern Str *process_n_textarea(void);
extern void feed_textarea(char *str);
extern Str *process_form(struct parsed_tag *tag);
extern Str *process_n_form(void);
extern int getMetaRefreshParam(char *q, Str **refresh_uri);


