/* vi: set sw=4 ts=8 ai sm noet : */
#ifndef PARSETAGX_H
#define PARSETAGX_H

#include "Str.h"
#include "html.h"

/* Parsed Tag structure */

struct parsed_tag {
    unsigned char tagid;
    unsigned char *attrid;
    char **value;
    unsigned char *map;
    char need_reconstruct;
};

/* modes for align() */

#define ALIGN_CENTER 0
#define ALIGN_LEFT   1
#define ALIGN_RIGHT  2
#define ALIGN_MIDDLE 4
#define ALIGN_TOP    5
#define ALIGN_BOTTOM 6

#define VALIGN_MIDDLE 0
#define VALIGN_TOP    1
#define VALIGN_BOTTOM 2

#define parsedtag_accepts(tag, id) ((tag)->map&&(tag)->map[id]!=MAX_TAGATTR)
#define parsedtag_exists(tag, id)  (parsedtag_accepts(tag,id)&&((tag)->attrid[(tag)->map[id]]!=ATTR_UNKNOWN))
#define parsedtag_delete(tag, id)  (parsedtag_accepts(tag,id)&&((tag)->attrid[(tag)->map[id]]=ATTR_UNKNOWN))
#define parsedtag_need_reconstruct(tag) ((tag)->need_reconstruct)
#define parsedtag_attname(tag, i) (AttrMAP[(tag)->attrid[i]].name)

extern struct parsed_tag *parse_tag(char **s, int internal);
extern int parsedtag_get_value(struct parsed_tag *tag, int id, void *value);
extern int parsedtag_set_value(struct parsed_tag *tag, int id, char *value);
extern Str parsedtag2str(struct parsed_tag *tag);
#endif
