#pragma once
#include "HtmlTags.h"
#include <gcstr/Str.h>
#include <stdbool.h>

#define ALIGN_CENTER 0
#define ALIGN_LEFT 1
#define ALIGN_RIGHT 2
#define ALIGN_MIDDLE 4
#define ALIGN_TOP 5
#define ALIGN_BOTTOM 6

#define VALIGN_MIDDLE 0
#define VALIGN_TOP 1
#define VALIGN_BOTTOM 2

struct HtmlTag {
    enum HtmlTags tagid;
    enum HtmlTagAttributes* attrid;
    char** value;
    unsigned char* map;
    char need_reconstruct;
};

#define parsedtag_accepts(tag, id) ((tag)->map && (tag)->map[id] != MAX_TAGATTR)
#define parsedtag_exists(tag, id) (parsedtag_accepts(tag, id) && ((tag)->attrid[(tag)->map[id]] != ATTR_UNKNOWN))
#define parsedtag_delete(tag, id) (parsedtag_accepts(tag, id) && ((tag)->attrid[(tag)->map[id]] = ATTR_UNKNOWN))
#define parsedtag_need_reconstruct(tag) ((tag)->need_reconstruct)
#define parsedtag_attname(tag, i) (AttrMAP[(tag)->attrid[i]].name)

extern struct HtmlTag* parse_tag(const char** s, bool internal);
extern int parsedtag_get_value(struct HtmlTag* tag, int id, void* value);
extern int parsedtag_set_value(struct HtmlTag* tag, int id, char* value);
extern Str parsedtag2str(struct HtmlTag* tag);
