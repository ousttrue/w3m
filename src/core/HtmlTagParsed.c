#define _GNU_SOURCE
#include "HtmlTagParsed.h"
#include "url.h"
#include "display.h"
#include "HtmlTag.h"
#include "HtmlTagAttribute.h"
#include "fm.h"
#include "image.h"
#include "myctype.h"
#include "indep.h"
#include "Str.h"
#include "hash.h"
#include "table.h"
#include "html_tag_info.h"
#include "html_tag_attribute_info.h"
#include "form.h"
#include "compression.h"
#include "symbol.h"
#include "readbuffer.h"
#include "etc.h"
#include <strings.h>

wc_ces cur_document_charset = 0;

#define MAX_INPUT_SIZE 80 /* TODO - max should be screen line length */

#define FORMSTACK_SIZE 10
#define FRAMESTACK_SIZE 10
#define INITIAL_FORM_SIZE 10
FormList** forms;
static int* form_stack;
int form_max = -1;
static int forms_size = 0;
#define cur_form_id ((form_sp >= 0) ? form_stack[form_sp] : -1)
static int form_sp = 0;

static Str cur_select;
static Str select_str;
static int select_is_multiple;
int n_selectitem;
static Str cur_option;
static Str cur_option_value;
static Str cur_option_label;
static int cur_option_selected;
static int cur_status;
/* menu based <select>  */
FormSelectOption* select_option;
int max_select = MAX_SELECT;
int n_select;
static int cur_option_maxwidth;

static Str cur_textarea;
Str* textarea_str;
static int cur_textarea_size;
static int cur_textarea_rows;
static int cur_textarea_readonly;
int n_textarea;
static int ignore_nl_textarea;
int max_textarea = MAX_TEXTAREA;

void initParser(int* pMax_textarea, int* pMax_select)
{
    n_textarea = -1;
    if (!max_textarea) { /* halfload */
        max_textarea = MAX_TEXTAREA;
        textarea_str = New_N(Str, max_textarea);
        // a_textarea = New_N(Anchor*, max_textarea);
    }
    *pMax_textarea = max_textarea;

    n_select = -1;
    if (!max_select) { /* halfload */
        max_select = MAX_SELECT;
        select_option = New_N(FormSelectOption, max_select);
        // a_select = New_N(Anchor*, max_select);
    }
    *pMax_select = max_select;
}

void init2()
{
    n_textarea = 0;
    cur_textarea = NULL;
    max_textarea = MAX_TEXTAREA;
    textarea_str = New_N(Str, max_textarea);
    n_select = 0;
    max_select = MAX_SELECT;
    select_option = New_N(FormSelectOption, max_select);
    cur_select = NULL;
    form_sp = -1;
    form_max = -1;
    forms_size = 0;
    forms = NULL;
    cur_hseq = 1;
    cur_iseq = 1;
}

static bool
noConv(const char* oval, void* str)
{
    *(const char**)str = oval;
    return true;
}

static bool
toNumber(const char* oval, void* num)
{
    char* ep;
    int x = strtol(oval, &ep, 10);
    if (ep > oval) {
        *(int*)num = x;
        return true;
    } else {
        return true;
    }
}

static bool toLength(const char* oval, void* len)
{
    if (!IS_DIGIT(oval[0]))
        return false;
    int w = atoi(oval);
    if (w < 0)
        return false;
    if (w == 0)
        w = 1;
    if (oval[strlen(oval) - 1] == '%')
        *(int*)len = -w;
    else
        *(int*)len = w;
    return true;
}

static bool toAlign(const char* oval, void* align)
{
    if (strcasecmp(oval, "left") == 0)
        *(int*)align = ALIGN_LEFT;
    else if (strcasecmp(oval, "right") == 0)
        *(int*)align = ALIGN_RIGHT;
    else if (strcasecmp(oval, "center") == 0)
        *(int*)align = ALIGN_CENTER;
    else if (strcasecmp(oval, "top") == 0)
        *(int*)align = ALIGN_TOP;
    else if (strcasecmp(oval, "bottom") == 0)
        *(int*)align = ALIGN_BOTTOM;
    else if (strcasecmp(oval, "middle") == 0)
        *(int*)align = ALIGN_MIDDLE;
    else
        return false;
    return true;
}

static bool toVAlign(const char* oval, void* valign)
{
    if (strcasecmp(oval, "top") == 0 || strcasecmp(oval, "baseline") == 0)
        *(int*)valign = VALIGN_TOP;
    else if (strcasecmp(oval, "bottom") == 0)
        *(int*)valign = VALIGN_BOTTOM;
    else if (strcasecmp(oval, "middle") == 0)
        *(int*)valign = VALIGN_MIDDLE;
    else
        return false;
    return true;
}

typedef bool (*ToValFunc)(const char*, void*);

static ToValFunc toValFunc[] = {
    noConv, /* VTYPE_NONE    */
    noConv, /* VTYPE_STR     */
    toNumber, /* VTYPE_NUMBER  */
    toLength, /* VTYPE_LENGTH  */
    toAlign, /* VTYPE_ALIGN   */
    toVAlign, /* VTYPE_VALIGN  */
    noConv, /* VTYPE_ACTION  */
    noConv, /* VTYPE_ENCTYPE */
    noConv, /* VTYPE_METHOD  */
    noConv, /* VTYPE_MLENGTH */
    noConv, /* VTYPE_TYPE    */
};

extern Hash_si tagtable;
#define MAX_TAG_LEN 64

struct HtmlTagParsed* parse_tag(char** s, bool internal)
{
    /* Parse tag name */
    char tagname[MAX_TAG_LEN];
    tagname[0] = '\0';
    char* q = (*s) + 1;
    char* p = tagname;
    if (*q == '/') {
        *(p++) = *(q++);
        SKIP_BLANKS(q);
    }
    while (*q && !IS_SPACE(*q) && !(tagname[0] != '/' && *q == '/') && *q != '>' && p - tagname < MAX_TAG_LEN - 1) {
        *(p++) = TOLOWER(*q);
        q++;
    }
    *p = '\0';
    while (*q && !IS_SPACE(*q) && !(tagname[0] != '/' && *q == '/') && *q != '>')
        q++;

    enum HtmlTag tag_id = getHash_si(&tagtable, tagname, HTML_UNKNOWN);
    if (tag_id == HTML_UNKNOWN || (!internal && TagMAP[tag_id].flag & TFLG_INT))
        goto skip_parse_tagarg;

    struct HtmlTagParsed* tag = New(struct HtmlTagParsed);
    memset(tag, 0, sizeof(struct HtmlTagParsed));
    tag->tagid = tag_id;

    int nattr = TagMAP[tag_id].max_attribute;
    if (nattr > 0) {
        tag->map = NewAtom_N(unsigned char, MAX_TAGATTR);
        for (int i = 0; i < MAX_TAGATTR; i++) {
            tag->map[i] = MAX_TAGATTR;
        }
        for (int i = 0; i < nattr; i++) {
            tag->map[TagMAP[tag_id].accept_attribute[i]] = i;
        }

        tag->attrid = NewAtom_N(enum HtmlTagAttribute, nattr);
        for (int i = 0; i < nattr; i++) {
            tag->attrid[i] = ATTR_UNKNOWN;
        }

        tag->value = New_N(char*, nattr);
    }

    /* Parse tag arguments */
    char attrname[MAX_TAG_LEN];
    SKIP_BLANKS(q);
    while (1) {
        Str value = NULL, value_tmp = NULL;
        if (*q == '>' || *q == '\0')
            goto done_parse_tag;
        p = attrname;
        while (*q && *q != '=' && !IS_SPACE(*q) && *q != '>' && p - attrname < MAX_TAG_LEN - 1) {
            *(p++) = TOLOWER(*q);
            q++;
        }
        *p = '\0';
        while (*q && *q != '=' && !IS_SPACE(*q) && *q != '>')
            q++;
        SKIP_BLANKS(q);
        if (*q == '=') {
            /* get value */
            value_tmp = Strnew();
            q++;
            SKIP_BLANKS(q);
            if (*q == '"') {
                q++;
                while (*q && *q != '"') {
                    Strcat_char(value_tmp, *q);
                    if (!tag->need_reconstruct && is_html_quote(*q))
                        tag->need_reconstruct = TRUE;
                    q++;
                }
                if (*q == '"')
                    q++;
            } else if (*q == '\'') {
                q++;
                while (*q && *q != '\'') {
                    Strcat_char(value_tmp, *q);
                    if (!tag->need_reconstruct && is_html_quote(*q))
                        tag->need_reconstruct = TRUE;
                    q++;
                }
                if (*q == '\'')
                    q++;
            } else if (*q) {
                while (*q && !IS_SPACE(*q) && *q != '>') {
                    Strcat_char(value_tmp, *q);
                    if (!tag->need_reconstruct && is_html_quote(*q))
                        tag->need_reconstruct = TRUE;
                    q++;
                }
            }
        }

        int attr_id = 0;
        int i = 0;
        for (; i < nattr; i++) {
            enum HtmlTagAttribute attr = tag->attrid[i];
            if (attr == ATTR_UNKNOWN) {
                if (strcmp(AttrMAP[TagMAP[tag_id].accept_attribute[i]].name, attrname) == 0) {
                    attr_id = TagMAP[tag_id].accept_attribute[i];
                    break;
                }
            }
        }

        if (value_tmp) {
            int j, hidden = FALSE;
            for (j = 0; j < i; j++) {
                if (tag->attrid[j] == ATTR_TYPE && tag->value[j] && strcmp("hidden", tag->value[j]) == 0) {
                    hidden = TRUE;
                    break;
                }
            }
            if ((tag_id == HTML_INPUT || tag_id == HTML_INPUT_ALT) && attr_id == ATTR_VALUE && hidden) {
                value = value_tmp;
            } else {
                char* x;
                value = Strnew();
                for (x = value_tmp->ptr; *x; x++) {
                    if (*x != '\n')
                        Strcat_char(value, *x);
                }
            }
        }

        if (i != nattr) {
            if (!internal && ((AttrMAP[attr_id].flag & AFLG_INT) || (value && AttrMAP[attr_id].vtype == VTYPE_METHOD && !strcasecmp(value->ptr, "internal")))) {
                tag->need_reconstruct = TRUE;
                continue;
            }
            tag->attrid[i] = attr_id;
            if (value)
                tag->value[i] = html_unquote(value->ptr);
            else
                tag->value[i] = NULL;
        } else {
            tag->need_reconstruct = TRUE;
        }
    }

skip_parse_tagarg:
    while (*q != '>' && *q)
        q++;
done_parse_tag:
    if (*q == '>')
        q++;
    *s = q;
    return tag;
}

bool parsedtag_set_value(struct HtmlTagParsed* tag, enum HtmlTagAttribute id, const char* value)
{
    if (!parsedtag_accepts(tag, id))
        return 0;

    int i = tag->map[id];
    tag->attrid[i] = id;
    if (value)
        tag->value[i] = allocStr(value, -1);
    else
        tag->value[i] = NULL;
    tag->need_reconstruct = TRUE;
    return 1;
}

bool parsedtag_get_value(struct HtmlTagParsed* tag, enum HtmlTagAttribute id, void* value)
{
    int i;
    if (!parsedtag_exists(tag, id) || !tag->value[i = tag->map[id]])
        return false;
    return toValFunc[AttrMAP[id].vtype](tag->value[i], value);
}

Str parsedtag2str(struct HtmlTagParsed* tag)
{
    enum HtmlTag tag_id = tag->tagid;
    TagInfo tag_info = TagMAP[tag_id];
    Str tagstr = Strnew();
    Strcat_char(tagstr, '<');
    Strcat_charp(tagstr, tag_info.name);
    for (int i = 0; i < tag_info.max_attribute; i++) {
        enum HtmlTagAttribute attr = tag->attrid[i];
        if (attr != ATTR_UNKNOWN) {
            Strcat_char(tagstr, ' ');
            TagAttrInfo attr_info = AttrMAP[attr];
            Strcat_charp(tagstr, attr_info.name);
            if (tag->value[i])
                Strcat(tagstr, Sprintf("=\"%s\"", html_quote(tag->value[i])));
        }
    }
    Strcat_char(tagstr, '>');
    return tagstr;
}

Str process_img(struct HtmlTagParsed* tag, int width)
{
    char *p, *q, *r, *r2 = NULL, *s, *t;
    int w, i, nw, ni = 1, n, w0 = -1, i0 = -1;
    int align, xoffset, yoffset, top, bottom, ismap = 0;
    int use_image = activeImage && displayImage;
    int pre_int = FALSE, ext_pre_int = FALSE;
    Str tmp = Strnew();

    if (!parsedtag_get_value(tag, ATTR_SRC, &p))
        return tmp;
    p = url_encode(remove_space(p), cur_baseURL, cur_document_charset);
    q = NULL;
    parsedtag_get_value(tag, ATTR_ALT, &q);
    if (!pseudoInlines && (q == NULL || (*q == '\0' && ignore_null_img_alt)))
        return tmp;
    t = q;
    parsedtag_get_value(tag, ATTR_TITLE, &t);
    w = -1;
    if (parsedtag_get_value(tag, ATTR_WIDTH, &w)) {
        if (w < 0) {
            if (width > 0)
                w = (int)(-width * pixel_per_char * w / 100 + 0.5);
            else
                w = -1;
        }
        if (use_image) {
            if (w > 0) {
                w = (int)(w * image_scale / 100 + 0.5);
                if (w == 0)
                    w = 1;
                else if (w > MAX_IMAGE_SIZE)
                    w = MAX_IMAGE_SIZE;
            }
        }
    }
    i = -1;
    if (use_image) {
        if (parsedtag_get_value(tag, ATTR_HEIGHT, &i)) {
            if (i > 0) {
                i = (int)(i * image_scale / 100 + 0.5);
                if (i == 0)
                    i = 1;
                else if (i > MAX_IMAGE_SIZE)
                    i = MAX_IMAGE_SIZE;
            } else {
                i = -1;
            }
        }
        align = -1;
        parsedtag_get_value(tag, ATTR_ALIGN, &align);
        ismap = 0;
        if (parsedtag_exists(tag, ATTR_ISMAP))
            ismap = 1;
    } else
        parsedtag_get_value(tag, ATTR_HEIGHT, &i);
    r = NULL;
    parsedtag_get_value(tag, ATTR_USEMAP, &r);
    if (parsedtag_exists(tag, ATTR_PRE_INT))
        ext_pre_int = TRUE;

    tmp = Strnew_size(128);
    if (use_image) {
        switch (align) {
        case ALIGN_LEFT:
            Strcat_charp(tmp, "<div_int align=left>");
            break;
        case ALIGN_CENTER:
            Strcat_charp(tmp, "<div_int align=center>");
            break;
        case ALIGN_RIGHT:
            Strcat_charp(tmp, "<div_int align=right>");
            break;
        }
    }
    if (r) {
        Str tmp2;
        r2 = strchr(r, '#');
        s = "<form_int method=internal action=map>";
        tmp2 = process_form(parse_tag(&s, TRUE));
        if (tmp2)
            Strcat(tmp, tmp2);
        Strcat(tmp, Sprintf("<input_alt fid=\"%d\" "
                            "type=hidden name=link value=\"",
                        cur_form_id));
        Strcat_charp(tmp, html_quote((r2) ? r2 + 1 : r));
        Strcat(tmp, Sprintf("\"><input_alt hseq=\"%d\" fid=\"%d\" "
                            "type=submit no_effect=true>",
                        cur_hseq++, cur_form_id));
    }
    if (use_image) {
        w0 = w;
        i0 = i;
        if (w < 0 || i < 0) {
            Image image;
            ParsedURL u;

            parseURL2(p, &u, cur_baseURL);
            image.url = parsedURL2Str(&u)->ptr;
            if (!uncompressed_file_type(u.file, &image.ext))
                image.ext = filename_extension(u.file, TRUE);
            image.cache = NULL;
            image.width = w;
            image.height = i;

            image.cache = getImage(&image, cur_baseURL, IMG_FLAG_SKIP);
            if (image.cache && image.cache->width > 0 && image.cache->height > 0) {
                w = w0 = image.cache->width;
                i = i0 = image.cache->height;
            }
            if (w < 0)
                w = 8 * pixel_per_char;
            if (i < 0)
                i = pixel_per_line;
        }
        if (enable_inline_image) {
            nw = (w > 1) ? ((w - 1) / pixel_per_char_i + 1) : 1;
            ni = (i > 1) ? ((i - 1) / pixel_per_line_i + 1) : 1;
        } else {
            nw = (w > 3) ? (int)((w - 3) / pixel_per_char + 1) : 1;
            ni = (i > 3) ? (int)((i - 3) / pixel_per_line + 1) : 1;
        }
        Strcat(tmp,
            Sprintf("<pre_int><img_alt hseq=\"%d\" src=\"", cur_iseq++));
        pre_int = TRUE;
    } else {
        if (w < 0)
            w = 12 * pixel_per_char;
        nw = w ? (int)((w - 1) / pixel_per_char + 1) : 1;
        if (r) {
            Strcat_charp(tmp, "<pre_int>");
            pre_int = TRUE;
        }
        Strcat_charp(tmp, "<img_alt src=\"");
    }
    Strcat_charp(tmp, html_quote(p));
    Strcat_charp(tmp, "\"");
    if (t) {
        Strcat_charp(tmp, " title=\"");
        Strcat_charp(tmp, html_quote(t));
        Strcat_charp(tmp, "\"");
    }
    if (use_image) {
        if (w0 >= 0)
            Strcat(tmp, Sprintf(" width=%d", w0));
        if (i0 >= 0)
            Strcat(tmp, Sprintf(" height=%d", i0));
        switch (align) {
        case ALIGN_MIDDLE:
            if (!enable_inline_image) {
                top = ni / 2;
                bottom = top;
                if (top * 2 == ni)
                    yoffset = (int)(((ni + 1) * pixel_per_line - i) / 2);
                else
                    yoffset = (int)((ni * pixel_per_line - i) / 2);
                break;
            }
        case ALIGN_TOP:
            top = 0;
            bottom = ni - 1;
            yoffset = 0;
            break;
        case ALIGN_BOTTOM:
            top = ni - 1;
            bottom = 0;
            yoffset = (int)(ni * pixel_per_line - i);
            break;
        default:
            top = ni - 1;
            bottom = 0;
            if (ni == 1 && ni * pixel_per_line > i)
                yoffset = 0;
            else {
                yoffset = (int)(ni * pixel_per_line - i);
                if (yoffset <= -2)
                    yoffset++;
            }
            break;
        }

        if (enable_inline_image)
            xoffset = 0;
        else
            xoffset = (int)((nw * pixel_per_char - w) / 2);

        if (xoffset)
            Strcat(tmp, Sprintf(" xoffset=%d", xoffset));
        if (yoffset)
            Strcat(tmp, Sprintf(" yoffset=%d", yoffset));
        if (top)
            Strcat(tmp, Sprintf(" top_margin=%d", top));
        if (bottom)
            Strcat(tmp, Sprintf(" bottom_margin=%d", bottom));
        if (r) {
            Strcat_charp(tmp, " usemap=\"");
            Strcat_charp(tmp, html_quote((r2) ? r2 + 1 : r));
            Strcat_charp(tmp, "\"");
        }
        if (ismap)
            Strcat_charp(tmp, " ismap");
    }
    Strcat_charp(tmp, ">");
    if (q != NULL && *q == '\0' && ignore_null_img_alt)
        q = NULL;
    if (q != NULL) {
        n = get_strwidth(q);
        if (use_image) {
            if (n > nw) {
                char* r;
                for (r = q, n = 0; *r; r += get_mclen(r), n += get_mcwidth(r)) {
                    if (n + get_mcwidth(r) > nw)
                        break;
                }
                Strcat_charp(tmp, html_quote(Strnew_charp_n(q, r - q)->ptr));
            } else
                Strcat_charp(tmp, html_quote(q));
        } else
            Strcat_charp(tmp, html_quote(q));
        goto img_end;
    }
    if (w > 0 && i > 0) {
        /* guess what the image is! */
        if (w < 32 && i < 48) {
            /* must be an icon or space */
            n = 1;
            if (strcasestr(p, "space") || strcasestr(p, "blank"))
                Strcat_charp(tmp, "_");
            else {
                if (w * i < 8 * 16)
                    Strcat_charp(tmp, "*");
                else {
                    if (!pre_int) {
                        Strcat_charp(tmp, "<pre_int>");
                        pre_int = TRUE;
                    }
                    push_symbol(tmp, IMG_SYMBOL, symbol_width, 1);
                    n = symbol_width;
                }
            }
            goto img_end;
        }
        if (w > 200 && i < 13) {
            /* must be a horizontal line */
            if (!pre_int) {
                Strcat_charp(tmp, "<pre_int>");
                pre_int = TRUE;
            }
            w = w / pixel_per_char / symbol_width;
            if (w <= 0)
                w = 1;
            push_symbol(tmp, HR_SYMBOL, symbol_width, w);
            n = w * symbol_width;
            goto img_end;
        }
    }
    for (q = p; *q; q++)
        ;
    while (q > p && *q != '/')
        q--;
    if (*q == '/')
        q++;
    Strcat_char(tmp, '[');
    n = 1;
    p = q;
    for (; *q; q++) {
        if (!IS_ALNUM(*q) && *q != '_' && *q != '-') {
            break;
        }
        Strcat_char(tmp, *q);
        n++;
        if (n + 1 >= nw)
            break;
    }
    Strcat_char(tmp, ']');
    n++;
img_end:
    if (use_image) {
        for (; n < nw; n++)
            Strcat_char(tmp, ' ');
    }
    Strcat_charp(tmp, "</img_alt>");
    if (pre_int && !ext_pre_int)
        Strcat_charp(tmp, "</pre_int>");
    if (r) {
        Strcat_charp(tmp, "</input_alt>");
        process_n_form();
    }
    if (use_image) {
        switch (align) {
        case ALIGN_RIGHT:
        case ALIGN_CENTER:
        case ALIGN_LEFT:
            Strcat_charp(tmp, "</div_int>");
            break;
        }
    }
    return tmp;
}

Str process_anchor(struct HtmlTagParsed* tag, char* tagbuf)
{
    if (tag->need_reconstruct) {
        parsedtag_set_value(tag, ATTR_HSEQ, Sprintf("%d", cur_hseq++)->ptr);
        return parsedtag2str(tag);
    } else {
        Str tmp = Sprintf("<a hseq=\"%d\"", cur_hseq++);
        Strcat_charp(tmp, tagbuf + 2);
        return tmp;
    }
}

Str process_input(struct HtmlTagParsed* tag)
{
    int i = 20, v, x, y, z, iw, ih, size = 20;
    char *q, *p, *r, *p2, *s;
    Str tmp = NULL;
    char* qq = "";
    int qlen = 0;

    if (cur_form_id < 0) {
        char* s = "<form_int method=internal action=none>";
        tmp = process_form(parse_tag(&s, TRUE));
    }
    if (tmp == NULL)
        tmp = Strnew();

    p = "text";
    parsedtag_get_value(tag, ATTR_TYPE, &p);
    q = NULL;
    parsedtag_get_value(tag, ATTR_VALUE, &q);
    r = "";
    parsedtag_get_value(tag, ATTR_NAME, &r);
    parsedtag_get_value(tag, ATTR_SIZE, &size);
    if (size > MAX_INPUT_SIZE)
        size = MAX_INPUT_SIZE;
    parsedtag_get_value(tag, ATTR_MAXLENGTH, &i);
    p2 = NULL;
    parsedtag_get_value(tag, ATTR_ALT, &p2);
    x = parsedtag_exists(tag, ATTR_CHECKED);
    y = parsedtag_exists(tag, ATTR_ACCEPT);
    z = parsedtag_exists(tag, ATTR_READONLY);

    v = formtype(p);
    if (v == FORM_UNKNOWN)
        return NULL;

    if (!q) {
        switch (v) {
        case FORM_INPUT_IMAGE:
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
            q = "SUBMIT";
            break;
        case FORM_INPUT_RESET:
            q = "RESET";
            break;
            /* if no VALUE attribute is specified in
             * <INPUT TYPE=CHECKBOX> tag, then the value "on" is used
             * as a default value. It is not a part of HTML4.0
             * specification, but an imitation of Netscape behaviour.
             */
        case FORM_INPUT_CHECKBOX:
            q = "on";
        }
    }
    /* VALUE attribute is not allowed in <INPUT TYPE=FILE> tag. */
    if (v == FORM_INPUT_FILE)
        q = NULL;
    if (q) {
        qq = html_quote(q);
        qlen = get_strwidth(q);
    }

    Strcat_charp(tmp, "<pre_int>");
    switch (v) {
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
    case FORM_INPUT_CHECKBOX:
        if (displayLinkNumber)
            Strcat(tmp, getLinkNumberStr(0));
        Strcat_char(tmp, '[');
        break;
    case FORM_INPUT_RADIO:
        if (displayLinkNumber)
            Strcat(tmp, getLinkNumberStr(0));
        Strcat_char(tmp, '(');
    }
    Strcat(tmp, Sprintf("<input_alt hseq=\"%d\" fid=\"%d\" type=\"%s\" "
                        "name=\"%s\" width=%d maxlength=%d value=\"%s\"",
                    cur_hseq++, cur_form_id, html_quote(p), html_quote(r), size, i, qq));
    if (x)
        Strcat_charp(tmp, " checked");
    if (y)
        Strcat_charp(tmp, " accept");
    if (z)
        Strcat_charp(tmp, " readonly");
    Strcat_char(tmp, '>');

    if (v == FORM_INPUT_HIDDEN)
        Strcat_charp(tmp, "</input_alt></pre_int>");
    else {
        switch (v) {
        case FORM_INPUT_PASSWORD:
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
            Strcat_charp(tmp, "<u>");
            break;
        case FORM_INPUT_IMAGE:
            s = NULL;
            parsedtag_get_value(tag, ATTR_SRC, &s);
            if (s) {
                Strcat(tmp, Sprintf("<img src=\"%s\"", html_quote(s)));
                if (p2)
                    Strcat(tmp, Sprintf(" alt=\"%s\"", html_quote(p2)));
                if (parsedtag_get_value(tag, ATTR_WIDTH, &iw))
                    Strcat(tmp, Sprintf(" width=\"%d\"", iw));
                if (parsedtag_get_value(tag, ATTR_HEIGHT, &ih))
                    Strcat(tmp, Sprintf(" height=\"%d\"", ih));
                Strcat_charp(tmp, " pre_int>");
                Strcat_charp(tmp, "</input_alt></pre_int>");
                return tmp;
            }
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
        case FORM_INPUT_RESET:
            if (displayLinkNumber)
                Strcat(tmp, getLinkNumberStr(-1));
            Strcat_charp(tmp, "[");
            break;
        }
        switch (v) {
        case FORM_INPUT_PASSWORD:
            i = 0;
            if (q) {
                for (; i < qlen && i < size; i++)
                    Strcat_char(tmp, '*');
            }
            for (; i < size; i++)
                Strcat_char(tmp, ' ');
            break;
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
            if (q)
                Strcat(tmp, textfieldrep(Strnew_charp(q), size));
            else {
                for (i = 0; i < size; i++)
                    Strcat_char(tmp, ' ');
            }
            break;
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
            if (p2)
                Strcat_charp(tmp, html_quote(p2));
            else
                Strcat_charp(tmp, qq);
            break;
        case FORM_INPUT_RESET:
            Strcat_charp(tmp, qq);
            break;
        case FORM_INPUT_RADIO:
        case FORM_INPUT_CHECKBOX:
            if (x)
                Strcat_char(tmp, '*');
            else
                Strcat_char(tmp, ' ');
            break;
        }
        switch (v) {
        case FORM_INPUT_PASSWORD:
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
            Strcat_charp(tmp, "</u>");
            break;
        case FORM_INPUT_IMAGE:
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
        case FORM_INPUT_RESET:
            Strcat_charp(tmp, "]");
        }
        Strcat_charp(tmp, "</input_alt>");
        switch (v) {
        case FORM_INPUT_PASSWORD:
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
        case FORM_INPUT_CHECKBOX:
            Strcat_char(tmp, ']');
            break;
        case FORM_INPUT_RADIO:
            Strcat_char(tmp, ')');
        }
        Strcat_charp(tmp, "</pre_int>");
    }
    return tmp;
}

Str process_button(struct HtmlTagParsed* tag)
{
    Str tmp = NULL;
    char *p, *q, *r, *qq = "";
    int v;

    if (cur_form_id < 0) {
        char* s = "<form_int method=internal action=none>";
        tmp = process_form(parse_tag(&s, TRUE));
    }
    if (tmp == NULL)
        tmp = Strnew();

    p = "submit";
    parsedtag_get_value(tag, ATTR_TYPE, &p);
    q = NULL;
    parsedtag_get_value(tag, ATTR_VALUE, &q);
    r = "";
    parsedtag_get_value(tag, ATTR_NAME, &r);

    v = formtype(p);
    if (v == FORM_UNKNOWN)
        return NULL;

    switch (v) {
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
    case FORM_INPUT_RESET:
        break;
    default:
        p = "submit";
        v = FORM_INPUT_SUBMIT;
        break;
    }

    if (!q) {
        switch (v) {
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
            q = "SUBMIT";
            break;
        case FORM_INPUT_RESET:
            q = "RESET";
            break;
        }
    }
    if (q) {
        qq = html_quote(q);
    }

    /*    Strcat_charp(tmp, "<pre_int>"); */
    Strcat(tmp, Sprintf("<input_alt hseq=\"%d\" fid=\"%d\" type=\"%s\" "
                        "name=\"%s\" value=\"%s\">",
                    cur_hseq++, cur_form_id, html_quote(p), html_quote(r), qq));
    return tmp;
}

Str process_n_button(void)
{
    Str tmp = Strnew();
    Strcat_charp(tmp, "</input_alt>");
    /*    Strcat_charp(tmp, "</pre_int>"); */
    return tmp;
}

Str process_select(struct HtmlTagParsed* tag)
{
    Str tmp = NULL;
    char* p;

    if (cur_form_id < 0) {
        char* s = "<form_int method=internal action=none>";
        tmp = process_form(parse_tag(&s, TRUE));
    }

    p = "";
    parsedtag_get_value(tag, ATTR_NAME, &p);
    cur_select = Strnew_charp(p);
    select_is_multiple = parsedtag_exists(tag, ATTR_MULTIPLE);

    if (!select_is_multiple) {
        select_str = Strnew_charp("<pre_int>");
        if (displayLinkNumber)
            Strcat(select_str, getLinkNumberStr(0));
        Strcat(select_str, Sprintf("[<input_alt hseq=\"%d\" "
                                   "fid=\"%d\" type=select name=\"%s\" selectnumber=%d",
                               cur_hseq++, cur_form_id, html_quote(p), n_select));
        Strcat_charp(select_str, ">");
        if (n_select == max_select) {
            max_select *= 2;
            select_option = New_Reuse(FormSelectOption, select_option, max_select);
        }
        select_option[n_select].first = NULL;
        select_option[n_select].last = NULL;
        cur_option_maxwidth = 0;
    } else
        select_str = Strnew();
    cur_option = NULL;
    cur_status = R_ST_NORMAL;
    n_selectitem = 0;
    return tmp;
}

Str process_n_select(void)
{
    if (cur_select == NULL)
        return NULL;
    process_option();
    if (!select_is_multiple) {
        if (select_option[n_select].first) {
            FormItemList sitem;
            chooseSelectOption(&sitem, select_option[n_select].first);
            Strcat(select_str, textfieldrep(sitem.label, cur_option_maxwidth));
        }
        Strcat_charp(select_str, "</input_alt>]</pre_int>");
        n_select++;
    } else
        Strcat_charp(select_str, "<br>");
    cur_select = NULL;
    n_selectitem = 0;
    return select_str;
}

void feed_select(char* str)
{
    Str tmp = Strnew();
    int prev_status = cur_status;
    static int prev_spaces = -1;
    char* p;

    if (cur_select == NULL)
        return;
    while (read_token(tmp, &str, &cur_status, 0, 0)) {
        if (cur_status != R_ST_NORMAL || prev_status != R_ST_NORMAL)
            continue;
        p = tmp->ptr;
        if (tmp->ptr[0] == '<' && Strlastchar(tmp) == '>') {
            struct HtmlTagParsed* tag;
            char* q;
            if (!(tag = parse_tag(&p, FALSE)))
                continue;
            switch (tag->tagid) {
            case HTML_OPTION:
                process_option();
                cur_option = Strnew();
                if (parsedtag_get_value(tag, ATTR_VALUE, &q))
                    cur_option_value = Strnew_charp(q);
                else
                    cur_option_value = NULL;
                if (parsedtag_get_value(tag, ATTR_LABEL, &q))
                    cur_option_label = Strnew_charp(q);
                else
                    cur_option_label = NULL;
                cur_option_selected = parsedtag_exists(tag, ATTR_SELECTED);
                prev_spaces = -1;
                break;
            case HTML_N_OPTION:
                /* do nothing */
                break;
            default:
                /* never happen */
                break;
            }
        } else if (cur_option) {
            while (*p) {
                if (IS_SPACE(*p) && prev_spaces != 0) {
                    p++;
                    if (prev_spaces > 0)
                        prev_spaces++;
                } else {
                    if (IS_SPACE(*p))
                        prev_spaces = 1;
                    else
                        prev_spaces = 0;
                    if (*p == '&')
                        Strcat_charp(cur_option, getescapecmd(&p));
                    else
                        Strcat_char(cur_option, *(p++));
                }
            }
        }
    }
}

void process_option(void)
{
    char begin_char = '[', end_char = ']';

    if (cur_select == NULL || cur_option == NULL)
        return;
    while (cur_option->length > 0 && IS_SPACE(Strlastchar(cur_option)))
        Strshrink(cur_option, 1);
    if (cur_option_value == NULL)
        cur_option_value = cur_option;
    if (cur_option_label == NULL)
        cur_option_label = cur_option;
    int len;
    if (!select_is_multiple) {
        len = get_Str_strwidth(cur_option_label);
        if (len > cur_option_maxwidth)
            cur_option_maxwidth = len;
        addSelectOption(&select_option[n_select],
            cur_option_value,
            cur_option_label, cur_option_selected);
        return;
    }
    if (!select_is_multiple) {
        begin_char = '(';
        end_char = ')';
    }
    Strcat(select_str, Sprintf("<br><pre_int>%c<input_alt hseq=\"%d\" "
                               "fid=\"%d\" type=%s name=\"%s\" value=\"%s\"",
                           begin_char, cur_hseq++, cur_form_id, select_is_multiple ? "checkbox" : "radio", html_quote(cur_select->ptr), html_quote(cur_option_value->ptr)));
    if (cur_option_selected)
        Strcat_charp(select_str, " checked>*</input_alt>");
    else
        Strcat_charp(select_str, "> </input_alt>");
    Strcat_char(select_str, end_char);
    Strcat_charp(select_str, html_quote(cur_option_label->ptr));
    Strcat_charp(select_str, "</pre_int>");
    n_selectitem++;
}

Str process_textarea(struct HtmlTagParsed* tag, int width)
{
    Str tmp = NULL;
    char* p;
#define TEXTAREA_ATTR_COL_MAX 4096
#define TEXTAREA_ATTR_ROWS_MAX 4096

    if (cur_form_id < 0) {
        char* s = "<form_int method=internal action=none>";
        tmp = process_form(parse_tag(&s, TRUE));
    }

    p = "";
    parsedtag_get_value(tag, ATTR_NAME, &p);
    cur_textarea = Strnew_charp(p);
    cur_textarea_size = 20;
    if (parsedtag_get_value(tag, ATTR_COLS, &p)) {
        cur_textarea_size = atoi(p);
        if (strlen(p) > 0 && p[strlen(p) - 1] == '%')
            cur_textarea_size = width * cur_textarea_size / 100 - 2;
        if (cur_textarea_size <= 0) {
            cur_textarea_size = 20;
        } else if (cur_textarea_size > TEXTAREA_ATTR_COL_MAX) {
            cur_textarea_size = TEXTAREA_ATTR_COL_MAX;
        }
    }
    cur_textarea_rows = 1;
    if (parsedtag_get_value(tag, ATTR_ROWS, &p)) {
        cur_textarea_rows = atoi(p);
        if (cur_textarea_rows <= 0) {
            cur_textarea_rows = 1;
        } else if (cur_textarea_rows > TEXTAREA_ATTR_ROWS_MAX) {
            cur_textarea_rows = TEXTAREA_ATTR_ROWS_MAX;
        }
    }
    cur_textarea_readonly = parsedtag_exists(tag, ATTR_READONLY);
    if (n_textarea >= max_textarea) {
        max_textarea *= 2;
        textarea_str = New_Reuse(Str, textarea_str, max_textarea);
    }
    textarea_str[n_textarea] = Strnew();
    ignore_nl_textarea = TRUE;

    return tmp;
}

Str process_n_textarea(void)
{
    Str tmp;
    int i;

    if (cur_textarea == NULL)
        return NULL;

    tmp = Strnew();
    Strcat(tmp, Sprintf("<pre_int>[<input_alt hseq=\"%d\" fid=\"%d\" "
                        "type=textarea name=\"%s\" size=%d rows=%d "
                        "top_margin=%d textareanumber=%d",
                    cur_hseq, cur_form_id, html_quote(cur_textarea->ptr), cur_textarea_size, cur_textarea_rows, cur_textarea_rows - 1, n_textarea));
    if (cur_textarea_readonly)
        Strcat_charp(tmp, " readonly");
    Strcat_charp(tmp, "><u>");
    for (i = 0; i < cur_textarea_size; i++)
        Strcat_char(tmp, ' ');
    Strcat_charp(tmp, "</u></input_alt>]</pre_int>\n");
    cur_hseq++;
    n_textarea++;
    cur_textarea = NULL;

    return tmp;
}

void feed_textarea(char* str)
{
    if (cur_textarea == NULL)
        return;
    if (ignore_nl_textarea) {
        if (*str == '\r')
            str++;
        if (*str == '\n')
            str++;
    }
    ignore_nl_textarea = FALSE;
    while (*str) {
        if (*str == '&')
            Strcat_charp(textarea_str[n_textarea], getescapecmd(&str));
        else if (*str == '\n') {
            Strcat_charp(textarea_str[n_textarea], "\r\n");
            str++;
        } else if (*str == '\r')
            str++;
        else
            Strcat_char(textarea_str[n_textarea], *(str++));
    }
}

Str process_hr(struct HtmlTagParsed* tag, int width, int indent_width)
{
    Str tmp = Strnew_charp("<nobr>");
    int w = 0;
    int x = ALIGN_CENTER;
#define HR_ATTR_WIDTH_MAX 65535

    if (width > indent_width)
        width -= indent_width;
    if (parsedtag_get_value(tag, ATTR_WIDTH, &w)) {
        if (w > HR_ATTR_WIDTH_MAX) {
            w = HR_ATTR_WIDTH_MAX;
        }
        w = REAL_WIDTH(w, width);
    } else {
        w = width;
    }

    parsedtag_get_value(tag, ATTR_ALIGN, &x);
    switch (x) {
    case ALIGN_CENTER:
        Strcat_charp(tmp, "<div_int align=center>");
        break;
    case ALIGN_RIGHT:
        Strcat_charp(tmp, "<div_int align=right>");
        break;
    case ALIGN_LEFT:
        Strcat_charp(tmp, "<div_int align=left>");
        break;
    }
    w /= symbol_width;
    if (w <= 0)
        w = 1;
    push_symbol(tmp, HR_SYMBOL, symbol_width, w);
    Strcat_charp(tmp, "</div_int></nobr>");
    return tmp;
}

static char*
check_accept_charset(char* ac)
{
    char *s = ac, *e;

    while (*s) {
        while (*s && (IS_SPACE(*s) || *s == ','))
            s++;
        if (!*s)
            break;
        e = s;
        while (*e && !(IS_SPACE(*e) || *e == ','))
            e++;
        if (wc_guess_charset(Strnew_charp_n(s, e - s)->ptr, 0))
            return ac;
        s = e;
    }
    return NULL;
}

static char*
check_charset(char* p)
{
    return wc_guess_charset(p, 0) ? p : NULL;
}

Str process_form_int(struct HtmlTagParsed* tag, int fid)
{
    char *p, *q, *r, *s, *tg, *n;

    p = "get";
    parsedtag_get_value(tag, ATTR_METHOD, &p);
    q = "!CURRENT_URL!";
    parsedtag_get_value(tag, ATTR_ACTION, &q);
    q = url_encode(remove_space(q), cur_baseURL, cur_document_charset);
    r = NULL;
    if (parsedtag_get_value(tag, ATTR_ACCEPT_CHARSET, &r))
        r = check_accept_charset(r);
    if (!r && parsedtag_get_value(tag, ATTR_CHARSET, &r))
        r = check_charset(r);
    s = NULL;
    parsedtag_get_value(tag, ATTR_ENCTYPE, &s);
    tg = NULL;
    parsedtag_get_value(tag, ATTR_TARGET, &tg);
    n = NULL;
    parsedtag_get_value(tag, ATTR_NAME, &n);

    if (fid < 0) {
        form_max++;
        form_sp++;
        fid = form_max;
    } else { /* <form_int> */
        if (form_max < fid)
            form_max = fid;
        form_sp = fid;
    }
    if (forms_size == 0) {
        forms_size = INITIAL_FORM_SIZE;
        forms = New_N(FormList*, forms_size);
        form_stack = NewAtom_N(int, forms_size);
    }
    if (forms_size <= form_max) {
        forms_size += form_max;
        forms = New_Reuse(FormList*, forms, forms_size);
        form_stack = New_Reuse(int, form_stack, forms_size);
    }
    form_stack[form_sp] = fid;

    forms[fid] = newFormList(q, p, r, s, tg, n, NULL);
    return NULL;
}

Str process_form(struct HtmlTagParsed* tag)
{
    return process_form_int(tag, -1);
}

Str process_n_form(void)
{
    if (form_sp >= 0)
        form_sp--;
    return NULL;
}

void process_idattr(struct readbuffer* obuf, int cmd, struct HtmlTagParsed* tag)
{
    char *id = NULL, *framename = NULL;
    Str idtag = NULL;

    /*
     * HTML_TABLE is handled by the other process.
     */
    if (cmd == HTML_TABLE)
        return;

    parsedtag_get_value(tag, ATTR_ID, &id);
    parsedtag_get_value(tag, ATTR_FRAMENAME, &framename);
    if (id == NULL)
        return;
    if (framename)
        idtag = Sprintf("<_id id=\"%s\" framename=\"%s\">",
            html_quote(id), html_quote(framename));
    else
        idtag = Sprintf("<_id id=\"%s\">", html_quote(id));
    push_tag(obuf, idtag->ptr, HTML_NOP);
}
