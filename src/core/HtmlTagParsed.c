#include "HtmlTagParsed.h"
#include "fm.h"
#include "myctype.h"
#include "indep.h"
#include "Str.h"
#include "hash.h"
#include "table.h"
#include "html_tag_info.h"
#include "html_tag_attribute_info.h"

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
        tag->attrid = NewAtom_N(unsigned char, nattr);
        tag->value = New_N(char*, nattr);
        tag->map = NewAtom_N(unsigned char, MAX_TAGATTR);
        memset(tag->map, MAX_TAGATTR, MAX_TAGATTR);
        memset(tag->attrid, ATTR_UNKNOWN, nattr);
        for (int i = 0; i < nattr; i++)
            tag->map[TagMAP[tag_id].accept_attribute[i]] = i;
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
            if ((tag)->attrid[i] == ATTR_UNKNOWN && strcmp(AttrMAP[TagMAP[tag_id].accept_attribute[i]].name, attrname) == 0) {
                attr_id = TagMAP[tag_id].accept_attribute[i];
                break;
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
    int i;
    int tag_id = tag->tagid;
    int nattr = TagMAP[tag_id].max_attribute;
    Str tagstr = Strnew();
    Strcat_char(tagstr, '<');
    Strcat_charp(tagstr, TagMAP[tag_id].name);
    for (i = 0; i < nattr; i++) {
        if (tag->attrid[i] != ATTR_UNKNOWN) {
            Strcat_char(tagstr, ' ');
            Strcat_charp(tagstr, AttrMAP[tag->attrid[i]].name);
            if (tag->value[i])
                Strcat(tagstr, Sprintf("=\"%s\"", html_quote(tag->value[i])));
        }
    }
    Strcat_char(tagstr, '>');
    return tagstr;
}
