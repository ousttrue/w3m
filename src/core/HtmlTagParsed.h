#pragma once
#include "Str.h"
#include "HtmlTag.h"
#include "HtmlTagAttribute.h"
#include <wc.h>

extern int activeImage;
extern bool displayLinkNumber;
extern struct Url* cur_baseURL;
extern int n_textarea;
extern Str* textarea_str;
extern int form_max;
extern struct Form** forms;
extern struct FormSelectOption* select_option;
extern int pseudoInlines;
extern int ignore_null_img_alt;
extern double image_scale;
extern bool enable_inline_image;
extern bool displayImage;

#define DEFAULT_PIXEL_PER_CHAR 7.0 /* arbitrary */
#define DEFAULT_PIXEL_PER_LINE 14.0 /* arbitrary */
#define MINIMUM_PIXEL_PER_CHAR 4.0
#define MAXIMUM_PIXEL_PER_CHAR 32.0

#define REAL_WIDTH(w, limit) (((w) >= 0) ? (int)((w) / pixel_per_char) : -(w) * (limit) / 100)

extern double pixel_per_char;
extern int pixel_per_char_i;
extern int set_pixel_per_char;
extern double pixel_per_line;
extern int pixel_per_line_i;
extern int set_pixel_per_line;

enum VAlignType {
    VALIGN_MIDDLE = 0,
    VALIGN_TOP = 1,
    VALIGN_BOTTOM = 2,
};

void initParser(int* pMax_textarea, int* pMax_select);
void init2();

extern wc_ces cur_document_charset;

struct HtmlTagParsed {
    enum HtmlTag tagid;
    enum HtmlTagAttribute* attrid;
    const char** value;
    // HtmlTagAttribute to index
    unsigned char* map;
    bool need_reconstruct;
};

Str process_form_int(struct HtmlTagParsed* tag, int fid);
extern int n_selectitem;
extern int n_select;

struct HtmlTagParsed* parse_tag(const char** s, bool internal);

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

Str process_hr(struct HtmlTagParsed* tag, int width, int indent_width);
Str process_img(struct HtmlTagParsed* tag, int width);
Str process_anchor(struct HtmlTagParsed* tag, char* tagbuf);
Str process_input(struct HtmlTagParsed* tag);
Str process_button(struct HtmlTagParsed* tag);
Str process_n_button(void);
Str process_select(struct HtmlTagParsed* tag);
Str process_n_select(void);
void feed_select(const char* str);
void process_option(void);
Str process_textarea(struct HtmlTagParsed* tag, int width);
Str process_n_textarea(void);
void feed_textarea(const char* str);
Str process_form(struct HtmlTagParsed* tag);
Str process_n_form(void);
