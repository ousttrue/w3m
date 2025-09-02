#pragma once
#include "Str.h"
#include "HtmlTag.h"
#include "HtmlTagAttribute.h"

struct HtmlTagParsed {
    enum HtmlTag tagid;
    enum HtmlTagAttribute* attrid;
    char** value;
    // HtmlTagAttribute to index
    unsigned char* map;
    bool need_reconstruct;
};

struct HtmlTagParsed* parse_tag(char** s, bool internal);

inline static bool parsedtag_accepts(struct HtmlTagParsed* tag, int id)
{
    return tag->map && tag->map[id] != MAX_TAGATTR;
}
inline static bool parsedtag_exists(struct HtmlTagParsed* tag, int id)
{
    return parsedtag_accepts(tag, id) && tag->attrid[(tag)->map[id]] != ATTR_UNKNOWN;
}
inline static void parsedtag_delete(struct HtmlTagParsed* tag, int id)
{
    (parsedtag_accepts(tag, id) && ((tag)->attrid[(tag)->map[id]] = ATTR_UNKNOWN));
}
bool parsedtag_get_value(struct HtmlTagParsed* tag, enum HtmlTagAttribute id, void* value);
bool parsedtag_set_value(struct HtmlTagParsed* tag, enum HtmlTagAttribute id, const char* value);
Str parsedtag2str(struct HtmlTagParsed* tag);
