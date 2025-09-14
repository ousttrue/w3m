#include "buffer_loader.h"
#include "ui.h"
#include "AnchorList.h"
#include "LinkList.h"
#include "convertline.h"
#include "display.h"
#include "html_quote.h"
#include "html_title.h"
#include "runtime.h"
#include "tty.h"
#include "w3m.h"
#include "buffer.h"
#include "entity.h"
#include "html_form.h"
#include "image_loader.h"
#include "maparea.h"
#include "quote.h"
#include "Content.h"
#include "readbuffer.h"
#include "HtmlTagParsed.h"
#include "form.h"
#include "funcname1.h"
#include "alloc.h"
#include "symbol.h"
#include "table.h"
#include <myctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include <wc.h>
#include <wtf.h>

int autoImage = (true);
char MetaRefresh = (false);
char DecodeCTE = (false);
int UseExternalDirBuffer = (true);
const char* DirBufferCommand = ("file:///$LIB/dirlist" CGI_EXTENSION);
const char* DefaultType = (NULL);
int displayLinkNumber = (false);
char SimplePreserveSpace = (false);
int squeezeBlankLine = (false);

struct Url* cur_baseURL = NULL;

static TextLineListItem* _tl_lp2;
static Str
textlist_feed(void)
{
    TextLine* p;
    if (_tl_lp2 != NULL) {
        p = _tl_lp2->ptr;
        _tl_lp2 = _tl_lp2->next;
        return p->line;
    }
    return NULL;
}

#define PPUSH(p, c)      \
    {                    \
        outp[pos] = (p); \
        outc[pos] = (c); \
        pos++;           \
    }

#define PSIZE                                       \
    if (out_size <= pos + 1) {                      \
        out_size = pos * 3 / 2;                     \
        outc = New_Reuse(char, outc, out_size);     \
        outp = New_Reuse(Lineprop, outp, out_size); \
    }

static int
ex_efct(int ex)
{
    int effect = 0;

    if (!ex)
        return 0;

    if (ex & PE_EX_ITALIC)
        effect |= PE_EX_ITALIC_E;

    if (ex & PE_EX_INSERT)
        effect |= PE_EX_INSERT_E;

    if (ex & PE_EX_STRIKE)
        effect |= PE_EX_STRIKE_E;

    return effect;
}

int getMetaRefreshParam(const char* q, Str* refresh_uri)
{
    int refresh_interval;
    Str s_tmp = NULL;

    if (q == NULL || refresh_uri == NULL)
        return 0;

    refresh_interval = atoi(q);
    if (refresh_interval < 0)
        return 0;

    while (*q) {
        if (!strncasecmp(q, "url=", 4)) {
            q += 4;
            if (*q == '\"' || *q == '\'') /* " or ' */
                q++;
            const char* r = q;
            while (*r && !IS_SPACE(*r) && *r != ';')
                r++;
            s_tmp = Strnew_charp_n(q, r - q);

            if (s_tmp->length > 0 && (s_tmp->ptr[s_tmp->length - 1] == '\"' || /* " */
                    s_tmp->ptr[s_tmp->length - 1] == '\'')) { /* ' */
                s_tmp->length--;
                s_tmp->ptr[s_tmp->length] = '\0';
            }
            q = r;
        }
        while (*q && *q != ';')
            q++;
        if (*q == ';')
            q++;
        while (*q && *q == ' ')
            q++;
    }
    *refresh_uri = s_tmp;
    return refresh_interval;
}

char* NullLine = "";
Lineprop NullProp[] = { 0 };

// static void
// addnewline2(struct Buffer* buf, char* line, Lineprop* prop, Linecolor* color, int pos, int nlines)
// {
//     struct Line* l;
//     l = New(struct Line);
//     l->next = NULL;
//     l->lineBuf = line;
//     l->propBuf = prop;
//     l->colorBuf = color;
//     l->len = pos;
//     l->width = -1;
//     l->size = pos;
//     l->bpos = 0;
//     l->bwidth = 0;
//     l->prev = currentLine(&buf->document);
//     if (currentLine(&buf->document)) {
//         l->next = currentLine(&buf->document)->next;
//         currentLine(&buf->document)->next = l;
//     } else
//         l->next = NULL;
//     if (lastLine(buf) == NULL || lastLine(buf) == currentLine(&buf->document))
//         lastLine(buf) = l;
//     currentLine(&buf->document) = l;
//     if (buf->firstLine == NULL)
//         buf->firstLine = l;
//     l->linenumber = ++buf->allLine;
//     l->real_linenumber = nlines;
// }

//
// buf->CurrentLine
//  ^prev
// Line
//  vnext
// Null
//
static void addLine(struct LineList* prev, struct LineList* l)
{
    l->prev = prev;
    l->next = NULL;
    if (prev) {
        // prev => next
        //      ^ insert l
        // prev => l => next
        // l->next = prev->next;
        prev->next = l;
    }
}

static struct LineList* addNewline(struct LineList* prev, char* line, Lineprop* prop, Linecolor* color, int pos,
    int width, int index)
{
    char* s;
    Lineprop* p;
    if (pos > 0) {
        s = allocStr(line, pos);
        p = NewAtom_N(Lineprop, pos);
        memcpy(p, prop, pos * sizeof(Lineprop));
    } else {
        s = NullLine;
        p = NullProp;
    }

    Linecolor* c;
    if (pos > 0 && color) {
        c = NewAtom_N(Linecolor, pos);
        memcpy(c, color, pos * sizeof(Linecolor));
    } else {
        c = NULL;
    }

    struct LineList* l = newLine(s, p, c, pos, index);
    addLine(prev, l);
    // prev = l;
    // if (pos > 0 && width > 0) {
    //     int bpos = 0;
    //     int bwidth = 0;
    //     while (1) {
    //         l->bpos = bpos;
    //         l->bwidth = bwidth;
    //         int i = columnLen(l, width);
    //         if (i == 0) {
    //             i++;
    //             while (i < l->len && p[i] & PC_WCHAR2)
    //                 i++;
    //         }
    //         l->len = i;
    //         l->width = COLPOS(l, l->len);
    //         if (pos <= i)
    //             break;
    //         bpos += l->len;
    //         bwidth += l->width;
    //         s += i;
    //         p += i;
    //         if (c)
    //             c += i;
    //         pos -= i;
    //
    //         l = newLine(s, p, c, pos, nlines);
    //         // l->linenumber = ++buf->allLine;
    //         addLine(prev, l);
    //         prev = l;
    //     }
    // }
    return l;
}

typedef Str (*FeedFunc)();

static struct Document
HTMLlineproc2body(struct Url url, wc_ces charset, int cols, FeedFunc feed)
{
    static char* outc = NULL;
    static Lineprop* outp = NULL;
    static int out_size = 0;
    struct Anchor *a_href = NULL, *a_img = NULL, *a_form = NULL;
    const char* p;
    const char* q;
    const char *r, *s, *t;
    const char* str;
    Lineprop mode, effect, ex_effect;
    int pos;
    int nlines;
    const char* id = NULL;
    int hseq, form_id;
    Str line;
    const char* endp;
    char symbol = '\0';
    int internal = 0;
    struct Anchor** a_textarea = NULL;
    struct Anchor** a_select = NULL;

    if (out_size == 0) {
        out_size = LINELEN;
        outc = NewAtom_N(char, out_size);
        outp = NewAtom_N(Lineprop, out_size);
    }

    int max_textarea;
    int max_select;
    initParser(&max_textarea, &max_select);
    if (!max_textarea) { /* halfload */
        a_textarea = New_N(struct Anchor*, max_textarea);
    }
    if (!max_select) { /* halfload */
        a_select = New_N(struct Anchor*, max_select);
    }

    struct Document doc = {
        .charset = charset,
        .firstLine = 0,
        .allLine = 0,
        .cols = cols,
        .url = url,
    };
    struct Url* base = makeBaseUrl(&doc);

    effect = 0;
    ex_effect = 0;
    nlines = 0;
    while ((line = feed()) != NULL) {
        if (n_textarea >= 0 && *(line->ptr) != '<') { /* halfload */
            Strcat(textarea_str[n_textarea], line);
            continue;
        }
    proc_again:
        pos = 0;
        Strremovetrailingspaces(line);
        str = line->ptr;
        endp = str + line->length;
        while (str < endp) {
            PSIZE;
            mode = get_mctype(str);
            if ((effect | ex_efct(ex_effect)) & PC_SYMBOL && *str != '<') {
                char** buf = set_symbol(symbol_width0);
                int len;

                p = buf[(int)symbol];
                len = get_mclen(p);
                mode = get_mctype(p);
                PPUSH(mode | effect | ex_efct(ex_effect), *(p++));
                if (--len) {
                    mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                    while (len--) {
                        PSIZE;
                        PPUSH(mode | effect | ex_efct(ex_effect), *(p++));
                    }
                }
                str += symbol_width;
            } else if (mode == PC_CTRL || mode == PC_UNDEF) {
                PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                str++;
            } else if (mode & PC_UNKNOWN) {
                PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                str += get_mclen(str);
            } else if (*str != '<' && *str != '&') {
                int len = get_mclen(str);
                PPUSH(mode | effect | ex_efct(ex_effect), *(str++));
                if (--len) {
                    mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                    while (len--) {
                        PSIZE;
                        PPUSH(mode | effect | ex_efct(ex_effect), *(str++));
                    }
                }
            } else if (*str == '&') {
                /*
                 * & escape processing
                 */
                p = getescapecmd(&str);
                while (*p) {
                    PSIZE;
                    mode = get_mctype((unsigned char*)p);
                    if (mode == PC_CTRL || mode == PC_UNDEF) {
                        PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                        p++;
                    } else if (mode & PC_UNKNOWN) {
                        PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                        p += get_mclen(p);
                    } else {
                        int len = get_mclen(p);
                        PPUSH(mode | effect | ex_efct(ex_effect), *(p++));
                        if (--len) {
                            mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                            while (len--) {
                                PSIZE;
                                PPUSH(mode | effect | ex_efct(ex_effect), *(p++));
                            }
                        }
                    }
                }
            } else {
                /* tag processing */
                struct HtmlTagParsed* tag;
                if (!(tag = parse_tag(&str, true)))
                    continue;
                switch (tag->tagid) {
                case HTML_B:
                    effect |= PE_BOLD;
                    break;
                case HTML_N_B:
                    effect &= ~PE_BOLD;
                    break;
                case HTML_I:
                    ex_effect |= PE_EX_ITALIC;
                    break;
                case HTML_N_I:
                    ex_effect &= ~PE_EX_ITALIC;
                    break;
                case HTML_INS:
                    ex_effect |= PE_EX_INSERT;
                    break;
                case HTML_N_INS:
                    ex_effect &= ~PE_EX_INSERT;
                    break;
                case HTML_U:
                    effect |= PE_UNDER;
                    break;
                case HTML_N_U:
                    effect &= ~PE_UNDER;
                    break;
                case HTML_S:
                    ex_effect |= PE_EX_STRIKE;
                    break;
                case HTML_N_S:
                    ex_effect &= ~PE_EX_STRIKE;
                    break;
                case HTML_A:
                    p = r = s = NULL;
                    q = doc.baseTarget;
                    t = "";
                    hseq = 0;
                    id = NULL;
                    if (parsedtag_get_value(tag, ATTR_NAME, &id)) {
                        id = url_quote_conv(id, doc.charset);
                        registerName(&doc, id, (struct BufferPoint) { .line = doc.allLine, .pos = pos });
                    }
                    if (parsedtag_get_value(tag, ATTR_HREF, &p))
                        p = url_quote(remove_space(p));
                    if (parsedtag_get_value(tag, ATTR_TARGET, &q))
                        q = url_quote_conv(q, doc.charset);
                    if (parsedtag_get_value(tag, ATTR_REFERER, &r))
                        r = url_quote(r);
                    parsedtag_get_value(tag, ATTR_TITLE, &s);
                    parsedtag_get_value(tag, ATTR_ACCESSKEY, &t);
                    parsedtag_get_value(tag, ATTR_HSEQ, &hseq);
                    if (hseq > 0)
                        doc.hmarklist = putHmarker(doc.hmarklist, doc.allLine, pos, hseq - 1);
                    else if (hseq < 0) {
                        int h = -hseq - 1;
                        if (doc.hmarklist && h < doc.hmarklist->nmark && doc.hmarklist->marks[h].invalid) {
                            doc.hmarklist->marks[h].pos = pos;
                            doc.hmarklist->marks[h].line = doc.allLine;
                            doc.hmarklist->marks[h].invalid = 0;
                            hseq = -hseq;
                        }
                    }
                    if (p) {
                        effect |= PE_ANCHOR;
                        a_href = registerHref(&doc, p, q, r, s, *t,
                            (struct BufferPoint) { .line = doc.allLine, .pos = pos });
                        a_href->hseq = ((hseq > 0) ? hseq : -hseq) - 1;
                        a_href->slave = (hseq > 0) ? false : true;
                    }
                    break;
                case HTML_N_A:
                    effect &= ~PE_ANCHOR;
                    if (a_href) {
                        a_href->end.line = doc.allLine;
                        a_href->end.pos = pos;
                        if (a_href->start.line == a_href->end.line && a_href->start.pos == a_href->end.pos) {
                            if (doc.hmarklist && a_href->hseq >= 0 && a_href->hseq < doc.hmarklist->nmark)
                                doc.hmarklist->marks[a_href->hseq].invalid = 1;
                            a_href->hseq = -1;
                        }
                        a_href = NULL;
                    }
                    break;

                case HTML_LINK:
                    addLink(&doc, tag);
                    break;

                case HTML_IMG_ALT:
                    if (parsedtag_get_value(tag, ATTR_SRC, &p)) {
                        int w = -1, h = -1, iseq = 0, ismap = 0;
                        int xoffset = 0, yoffset = 0, top = 0, bottom = 0;
                        parsedtag_get_value(tag, ATTR_HSEQ, &iseq);
                        parsedtag_get_value(tag, ATTR_WIDTH, &w);
                        parsedtag_get_value(tag, ATTR_HEIGHT, &h);
                        parsedtag_get_value(tag, ATTR_XOFFSET, &xoffset);
                        parsedtag_get_value(tag, ATTR_YOFFSET, &yoffset);
                        parsedtag_get_value(tag, ATTR_TOP_MARGIN, &top);
                        parsedtag_get_value(tag, ATTR_BOTTOM_MARGIN, &bottom);
                        if (parsedtag_exists(tag, ATTR_ISMAP))
                            ismap = 1;
                        q = NULL;
                        parsedtag_get_value(tag, ATTR_USEMAP, &q);
                        if (iseq > 0) {
                            doc.imarklist = putHmarker(doc.imarklist,
                                doc.allLine, pos,
                                iseq - 1);
                        }
                        s = NULL;
                        parsedtag_get_value(tag, ATTR_TITLE, &s);
                        p = url_quote_conv(remove_space(p),
                            doc.charset);
                        a_img = registerImg(&doc, p, s,
                            (struct BufferPoint) { .line = doc.allLine, .pos = pos });
                        a_img->hseq = iseq;
                        a_img->image = NULL;
                        if (iseq > 0) {
                            struct Url u;
                            u = parseUrl(a_img->url, base);

                            struct Image* image;
                            a_img->image = image = New(struct Image);

                            image->url = parsedURL2Str(&u)->ptr;
                            if (!uncompressed_file_type(u.file, &image->ext))
                                image->ext = filename_extension(u.file, true);
                            image->cache = NULL;
                            image->width = (w > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : w;
                            image->height = (h > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : h;
                            image->xoffset = xoffset;
                            image->yoffset = yoffset;
                            image->y = doc.allLine - top;
                            if (image->xoffset < 0 && pos == 0)
                                image->xoffset = 0;
                            if (image->yoffset < 0 && image->y == 1)
                                image->yoffset = 0;
                            image->rows = 1 + top + bottom;
                            image->map = q;
                            image->ismap = ismap;
                            image->touch = 0;
                            image->cache = getImage(image, base, IMG_FLAG_SKIP);
                        } else if (iseq < 0) {
                            struct BufferPoint* po = doc.imarklist->marks - iseq - 1;
                            struct Anchor* a = retrieveAnchor(doc.img, *po);
                            if (a) {
                                a_img->url = a->url;
                                a_img->image = a->image;
                            }
                        }
                    }
                    effect |= PE_IMAGE;
                    break;
                case HTML_N_IMG_ALT:
                    effect &= ~PE_IMAGE;
                    if (a_img) {
                        a_img->end.line = doc.allLine;
                        a_img->end.pos = pos;
                    }
                    a_img = NULL;
                    break;
                case HTML_INPUT_ALT: {
                    struct Form* form;
                    int top = 0, bottom = 0;
                    int textareanumber = -1;
                    int selectnumber = -1;
                    hseq = 0;
                    form_id = -1;

                    parsedtag_get_value(tag, ATTR_HSEQ, &hseq);
                    parsedtag_get_value(tag, ATTR_FID, &form_id);
                    parsedtag_get_value(tag, ATTR_TOP_MARGIN, &top);
                    parsedtag_get_value(tag, ATTR_BOTTOM_MARGIN, &bottom);
                    if (form_id < 0 || form_id > form_max || forms == NULL || forms[form_id] == NULL)
                        break; /* outside of <form>..</form> */
                    form = forms[form_id];
                    if (hseq > 0) {
                        int hpos = pos;
                        if (*str == '[')
                            hpos++;
                        doc.hmarklist = putHmarker(doc.hmarklist, doc.allLine,
                            hpos, hseq - 1);
                    } else if (hseq < 0) {
                        int h = -hseq - 1;
                        int hpos = pos;
                        if (*str == '[')
                            hpos++;
                        if (doc.hmarklist && h < doc.hmarklist->nmark && doc.hmarklist->marks[h].invalid) {
                            doc.hmarklist->marks[h].pos = hpos;
                            doc.hmarklist->marks[h].line = doc.allLine;
                            doc.hmarklist->marks[h].invalid = 0;
                            hseq = -hseq;
                        }
                    }

                    if (!form->target)
                        form->target = doc.baseTarget;
                    if (a_textarea && parsedtag_get_value(tag, ATTR_TEXTAREANUMBER, &textareanumber)) {
                        if (textareanumber >= max_textarea) {
                            max_textarea = 2 * textareanumber;
                            textarea_str = New_Reuse(Str, textarea_str,
                                max_textarea);
                            a_textarea = New_Reuse(struct Anchor*, a_textarea,
                                max_textarea);
                        }
                    }
                    if (a_select && parsedtag_get_value(tag, ATTR_SELECTNUMBER, &selectnumber)) {
                        if (selectnumber >= max_select) {
                            max_select = 2 * selectnumber;
                            select_option = New_Reuse(struct FormSelectOption,
                                select_option,
                                max_select);
                            a_select = New_Reuse(struct Anchor*, a_select,
                                max_select);
                        }
                    }
                    a_form = registerForm(&doc, form, tag,
                        (struct BufferPoint) { .line = doc.allLine, .pos = pos });
                    if (a_textarea && textareanumber >= 0)
                        a_textarea[textareanumber] = a_form;
                    if (a_select && selectnumber >= 0)
                        a_select[selectnumber] = a_form;
                    if (a_form) {
                        a_form->hseq = hseq - 1;
                        a_form->y = doc.allLine - top;
                        a_form->rows = 1 + top + bottom;
                        if (!parsedtag_exists(tag, ATTR_NO_EFFECT))
                            effect |= PE_FORM;
                        break;
                    }
                }
                case HTML_N_INPUT_ALT:
                    effect &= ~PE_FORM;
                    if (a_form) {
                        a_form->end.line = doc.allLine;
                        a_form->end.pos = pos;
                        if (a_form->start.line == a_form->end.line && a_form->start.pos == a_form->end.pos)
                            a_form->hseq = -1;
                    }
                    a_form = NULL;
                    break;
                case HTML_MAP:
                    if (parsedtag_get_value(tag, ATTR_NAME, &p)) {
                        struct MapList* m = New(struct MapList);
                        m->name = Strnew_charp(p);
                        m->area = newGeneralList();
                        m->next = doc.maplist;
                        doc.maplist = m;
                    }
                    break;
                case HTML_N_MAP:
                    /* nothing to do */
                    break;
                case HTML_AREA:
                    if (doc.maplist == NULL) /* outside of <map>..</map> */
                        break;
                    if (parsedtag_get_value(tag, ATTR_HREF, &p)) {
                        struct MapArea* a;
                        p = url_quote(remove_space(p));
                        t = NULL;
                        parsedtag_get_value(tag, ATTR_TARGET, &t);
                        q = "";
                        parsedtag_get_value(tag, ATTR_ALT, &q);
                        r = NULL;
                        s = NULL;
                        parsedtag_get_value(tag, ATTR_SHAPE, &r);
                        parsedtag_get_value(tag, ATTR_COORDS, &s);
                        a = newMapArea(p, t, q, r, s);
                        pushValue(doc.maplist->area, (void*)a);
                    }
                    break;
                case HTML_FRAMESET:
                    break;
                case HTML_N_FRAMESET:
                    break;
                case HTML_FRAME:
                    break;
                case HTML_BASE:
                    if (parsedtag_get_value(tag, ATTR_HREF, &p)) {
                        p = url_quote(remove_space(p));
                        if (!doc.baseUrl)
                            doc.baseUrl = New(struct Url);
                        *doc.baseUrl = parseUrl(p, base);

                        base = doc.baseUrl;
                    }
                    if (parsedtag_get_value(tag, ATTR_TARGET, &p))
                        doc.baseTarget = url_quote_conv(p, doc.charset);
                    break;
                case HTML_META:
                    p = q = NULL;
                    parsedtag_get_value(tag, ATTR_HTTP_EQUIV, &p);
                    parsedtag_get_value(tag, ATTR_CONTENT, &q);
                    if (p && q && !strcasecmp(p, "refresh") && MetaRefresh) {
                        Str tmp = NULL;
                        int refresh_interval = getMetaRefreshParam(q, &tmp);
                        if (tmp) {
                            p = url_quote(remove_space(tmp->ptr));
                            // buf->event = setAlarmEvent(buf->event,
                            //     refresh_interval,
                            //     AL_IMPLICIT_ONCE,
                            //     FUNCNAME_gorURL, (void*)p);
                        } else if (refresh_interval > 0) {
                            // buf->event = setAlarmEvent(buf->event,
                            //     refresh_interval,
                            //     AL_IMPLICIT,
                            //     FUNCNAME_reload, NULL);
                        }
                    }
                    break;
                case HTML_INTERNAL:
                    internal = HTML_INTERNAL;
                    break;
                case HTML_N_INTERNAL:
                    internal = HTML_N_INTERNAL;
                    break;
                case HTML_FORM_INT:
                    if (parsedtag_get_value(tag, ATTR_FID, &form_id))
                        process_form_int(tag, form_id);
                    break;
                case HTML_TEXTAREA_INT:
                    if (parsedtag_get_value(tag, ATTR_TEXTAREANUMBER,
                            &n_textarea)
                        && n_textarea >= 0 && n_textarea < max_textarea) {
                        textarea_str[n_textarea] = Strnew();
                    } else
                        n_textarea = -1;
                    break;
                case HTML_N_TEXTAREA_INT:
                    if (a_textarea && n_textarea >= 0) {
                        struct FormItem* item = (struct FormItem*)a_textarea[n_textarea]->url;
                        item->init_value = item->value = textarea_str[n_textarea];
                    }
                    break;
                case HTML_SELECT_INT:
                    if (parsedtag_get_value(tag, ATTR_SELECTNUMBER, &n_select)
                        && n_select >= 0 && n_select < max_select) {
                        select_option[n_select].first = NULL;
                        select_option[n_select].last = NULL;
                    } else
                        n_select = -1;
                    break;
                case HTML_N_SELECT_INT:
                    if (a_select && n_select >= 0) {
                        struct FormItem* item = (struct FormItem*)a_select[n_select]->url;
                        item->select_option = select_option[n_select].first;
                        chooseSelectOption(item, item->select_option);
                        item->init_selected = item->selected;
                        item->init_value = item->value;
                        item->init_label = item->label;
                    }
                    break;
                case HTML_OPTION_INT:
                    if (n_select >= 0) {
                        int selected;
                        q = "";
                        parsedtag_get_value(tag, ATTR_LABEL, &q);
                        p = q;
                        parsedtag_get_value(tag, ATTR_VALUE, &p);
                        selected = parsedtag_exists(tag, ATTR_SELECTED);
                        addSelectOption(&select_option[n_select],
                            Strnew_charp(p), Strnew_charp(q),
                            selected);
                    }
                    break;
                case HTML_TITLE_ALT:
                    if (parsedtag_get_value(tag, ATTR_TITLE, &p))
                        doc.title = html_unquote(p);
                    break;
                case HTML_SYMBOL:
                    effect |= PC_SYMBOL;
                    if (parsedtag_get_value(tag, ATTR_TYPE, &p))
                        symbol = (char)atoi(p);
                    break;
                case HTML_N_SYMBOL:
                    effect &= ~PC_SYMBOL;
                    break;
                default:
                    break;
                }
                id = NULL;
                if (parsedtag_get_value(tag, ATTR_ID, &id)) {
                    id = url_quote_conv(id, doc.charset);
                    registerName(&doc, id,
                        (struct BufferPoint) { .line = doc.allLine, .pos = pos });
                }
            }
        }
        /* end of processing for one line */
        if (!internal) {
            struct LineList* l = addNewline(currentLine(&doc), outc, outp, NULL, pos, -1, doc.allLine++);
            doc.currentLineIndex = l->linenumber;
            if (doc.firstLine == NULL) {
                doc.firstLine = l;
            }
        }
        if (internal == HTML_N_INTERNAL)
            internal = 0;
        if (str != endp) {
            line = Strsubstr(line, str - line->ptr, endp - str);
            goto proc_again;
        }
    }
    for (form_id = 1; form_id <= form_max; form_id++)
        if (forms[form_id])
            forms[form_id]->next = forms[form_id - 1];
    doc.formlist = (form_max >= 0) ? forms[form_max] : NULL;
    if (n_textarea)
        addMultirowsForm(&doc, doc.formitem);
    addMultirowsImg(&doc, doc.img);

    doc.topLineIndex = doc.firstLine->linenumber;
    doc.currentLineIndex = doc.firstLine->linenumber;
    return doc;
}

static int loadHTML(struct html_feed_environ* htmlenv1,
    Str html, wc_ces* doc_charset, int cols, bool use_graphic, bool internal)
{
    struct environment envs[MAX_ENV_LEVEL];
    long long linelen = 0;
    long long trbyte = 0;
    Str lineBuf2 = Strnew();
    // wc_ces charset = WC_CES_US_ASCII;
    // wc_ces doc_charset = DocumentCharset;
    struct readbuffer obuf;
    // void (*volatile prevtrap)(int _dummy) = NULL;

    if (use_graphic) {
        symbol_width = symbol_width0 = 1;
    } else {
        symbol_width0 = 0;
        get_symbol(DisplayCharset, &symbol_width0);
        symbol_width = WcOption.use_wide ? symbol_width0 : 1;
    }

    init_title();
    init2();

    // if (newBuf->image_flag)
    //     image_flag = newBuf->image_flag;
    int image_flag;
    if (activeImage && displayImage && autoImage)
        image_flag = IMG_FLAG_AUTO;
    else
        image_flag = IMG_FLAG_SKIP;

    init_henv(htmlenv1, &obuf, envs, MAX_ENV_LEVEL, NULL, cols, 0);

    htmlenv1->buf = newTextLineList();
    cur_baseURL = NULL; // baseURL(newBuf);

    // if (sigsetjmp(AbortLoading, 1) != 0) {
    //     HTMLlineproc0("<br>Transfer Interrupted!<br>", &htmlenv1, true);
    //     goto phase2;
    // }
    // TRAP_ON;

    // if (newBuf != NULL) {
    //     if (newBuf->document_charset)
    //         charset = doc_charset = newBuf->document_charset;
    // }

    // wc_ces doc_charset = content_charset;
    // if (content_charset && UseContentCharset)
    //     doc_charset = content_charset;

    union input_stream* stream = newStrStream(html);
    meta_charset = 0;
    while ((lineBuf2 = StrmyISgets(stream)) && lineBuf2->length) {
        // if (src)
        //     Strfputs(lineBuf2, src);
        linelen += lineBuf2->length;
        // showProgress(current_content_length, &linelen, &trbyte);
        if (meta_charset) { /* <META> */
            //     if (content_charset == 0 && UseContentCharset) {
            *doc_charset = meta_charset;
            //         charset = WC_CES_US_ASCII;
            //     }
            meta_charset = 0;
        }
        wc_ces out_charset = 0;
        lineBuf2 = convertLine(lineBuf2, HTML_MODE, &out_charset, *doc_charset, InnerCharset);
        if (out_charset && out_charset != *doc_charset) {
            *doc_charset = out_charset;
        }
        // cur_document_charset = charset;
        HTMLlineproc0(lineBuf2->ptr, htmlenv1, internal);
    }
    if (obuf.status != R_ST_NORMAL) {
        HTMLlineproc0("\n", htmlenv1, internal);
    }
    obuf.status = R_ST_NORMAL;
    completeHTMLstream(htmlenv1, &obuf);
    flushline(htmlenv1, &obuf, 0, 2, htmlenv1->limit);
    cur_baseURL = NULL;
    cur_document_charset = 0;

    return trbyte + linelen;
}

// WC_CES_SHIFT_JIS /*WC_CES_US_ASCII*/
static struct Document loadHtmlDocument(struct Url url, Str html, wc_ces content_charset,
    int cols, bool use_graphic,
    bool internal)
{
    struct html_feed_environ htmlenv1;

    int trbyte = loadHTML(&htmlenv1, html, &content_charset, cols, use_graphic, internal);

    // phase2:
    // struct Buffer* buf = newBuffer();
    // TRAP_OFF;
    // buf->document_charset = charset;
    // buf->image_flag = image_flag;
    // HTMLlineproc2(buf, htmlenv1.buf);
    // static void HTMLlineproc2(struct Buffer* buf, TextLineList* tl)
    // {
    _tl_lp2 = htmlenv1.buf->first;
    struct Document doc = HTMLlineproc2body(url, content_charset, cols, textlist_feed);
    if (htmlenv1.title)
        doc.title = htmlenv1.title;
    // }

    // return buf;
    // }

    //     struct TermEntry* t = getTermEntry();
    //     struct environment envs[MAX_ENV_LEVEL];
    //     long long linelen = 0;
    //     long long trbyte = 0;
    //     Str lineBuf2 = Strnew();
    //     wc_ces charset = WC_CES_US_ASCII;
    //     wc_ces  doc_charset = DocumentCharset;
    //     struct html_feed_environ htmlenv1;
    //     struct readbuffer obuf;
    //     int  image_flag;
    //     MySignalHandler (* prevtrap)(int _dummy) = NULL;
    //
    //     if (graph_ok(t)) {
    //         symbol_width = symbol_width0 = 1;
    //     } else {
    //         symbol_width0 = 0;
    //         get_symbol(DisplayCharset, &symbol_width0);
    //         symbol_width = WcOption.use_wide ? symbol_width0 : 1;
    //     }
    //
    //     init_title();
    //     init2();
    //     if (newBuf->image_flag)
    //         image_flag = newBuf->image_flag;
    //     else if (activeImage && displayImage && autoImage)
    //         image_flag = IMG_FLAG_AUTO;
    //     else
    //         image_flag = IMG_FLAG_SKIP;
    //
    //     init_henv(&htmlenv1, &obuf, envs, MAX_ENV_LEVEL, NULL, newBuf->width, 0);
    //
    //     htmlenv1.buf = newTextLineList();
    // #if defined(USE_M17N) || defined(USE_IMAGE)
    //     cur_baseURL = baseURL(newBuf);
    // #endif
    //
    //     if (sigsetjmp(AbortLoading, 1) != 0) {
    //         HTMLlineproc0("<br>Transfer Interrupted!<br>", &htmlenv1, true);
    //         goto phase2;
    //     }
    //     TRAP_ON;
    //
    //     if (newBuf != NULL) {
    //         if (newBuf->document_charset)
    //             charset = doc_charset = newBuf->document_charset;
    //     }
    //     if (content_charset && UseContentCharset)
    //         doc_charset = content_charset;
    //     else if (f->guess_type && !strcasecmp(f->guess_type, "application/xhtml+xml"))
    //         doc_charset = WC_CES_UTF_8;
    //     meta_charset = 0;
    //     if (IStype(f->stream) != IST_ENCODED)
    //         f->stream = newEncodedStream(f->stream, f->encoding);
    //     while ((lineBuf2 = StrmyUFgets(f)) && lineBuf2->length) {
    //         if (src)
    //             Strfputs(lineBuf2, src);
    //         linelen += lineBuf2->length;
    //         showProgress(current_content_length, &linelen, &trbyte);
    //         if (meta_charset) { /* <META> */
    //             if (content_charset == 0 && UseContentCharset) {
    //                 doc_charset = meta_charset;
    //                 charset = WC_CES_US_ASCII;
    //             }
    //             meta_charset = 0;
    //         }
    //         lineBuf2 = convertLine(f, lineBuf2, HTML_MODE, &charset, doc_charset);
    //         cur_document_charset = charset;
    //         HTMLlineproc0(lineBuf2->ptr, &htmlenv1, internal);
    //     }
    //     if (obuf.status != R_ST_NORMAL) {
    //         HTMLlineproc0("\n", &htmlenv1, internal);
    //     }
    //     obuf.status = R_ST_NORMAL;
    //     completeHTMLstream(&htmlenv1, &obuf);
    //     flushline(&htmlenv1, &obuf, 0, 2, htmlenv1.limit);
    // #if defined(USE_M17N) || defined(USE_IMAGE)
    //     cur_baseURL = NULL;
    // #endif
    //     cur_document_charset = 0;
    //     if (htmlenv1.title)
    //         newBuf->buffername = htmlenv1.title;
    // phase2:
    //     newBuf->trbyte = trbyte + linelen;
    //     TRAP_OFF;
    //     newBuf->document_charset = charset;
    //     newBuf->image_flag = image_flag;
    //     HTMLlineproc2(newBuf, htmlenv1.buf);

    // if (n_textarea)
    //     formResetBuffer(buf, buf->document.formitem);

    return doc;
}

#define TAG_IS(s, tag, len) \
    (strncasecmp(s, tag, len) == 0 && (s[len] == '>' || IS_SPACE((int)s[len])))

static InputStream _file_lp2;

static void
proc_escape(struct readbuffer* obuf, const char** str_return)
{
    const char* str = *str_return;
    int ech = getescapechar(str_return);
    int width, n_add = *str_return - str;

    if (ech < 0) {
        *str_return = str;
        proc_mchar(obuf, obuf->flag & RB_SPECIAL, 1, str_return, PC_ASCII);
        return;
    }
    Lineprop mode = PC_ASCII;
    mode = IS_CNTRL(ech) ? PC_CTRL : PC_ASCII;

    const char* estr = conv_entity(ech);
    check_breakpoint(obuf, obuf->flag & RB_SPECIAL, estr);
    width = get_strwidth(estr);
    if (width == 1 && ech == (unsigned char)*estr && ech != '&' && ech != '<' && ech != '>') {
        if (IS_CNTRL(ech))
            mode = PC_CTRL;
        push_charp(obuf, width, estr, mode);
    } else
        push_nchars(obuf, width, str, n_add, mode);
    set_prevchar(obuf->prevchar, estr, strlen(estr));
    obuf->prev_ctype = mode;
}

static int
need_flushline(struct html_feed_environ* h_env, struct readbuffer* obuf,
    Lineprop mode)
{
    char ch;

    if (obuf->flag & RB_PRE_INT) {
        if (obuf->pos > h_env->limit)
            return 1;
        else
            return 0;
    }

    ch = Strlastchar(obuf->line);
    /* if (ch == ' ' && obuf->tag_sp > 0) */
    if (ch == ' ')
        return 0;

    if (obuf->pos > h_env->limit)
        return 1;

    return 0;
}

inline static int min(int a, int b)
{
    return ((a) > (b) ? (b) : (a));
}

/* HTML processing first pass */
void HTMLlineproc0(const char* line, struct html_feed_environ* h_env, bool internal)
{
    Lineprop mode;
    int cmd;
    struct readbuffer* obuf = h_env->obuf;
    int indent, delta;
    struct HtmlTagParsed* tag;
    Str tokbuf;
    struct table* tbl = NULL;
    struct table_mode* tbl_mode = NULL;
    int tbl_width = 0;
    int is_hangul, prev_is_hangul = 0;

    tokbuf = Strnew();

table_start:
    if (obuf->table_level >= 0) {
        int level = min(obuf->table_level, MAX_TABLE - 1);
        tbl = tables[level];
        tbl_mode = &table_mode[level];
        tbl_width = table_width(h_env, level);
    }

    while (*line != '\0') {
        int is_tag = false;
        int pre_mode = (obuf->table_level >= 0 && tbl_mode) ? tbl_mode->pre_mode : obuf->flag;
        int end_tag = (obuf->table_level >= 0 && tbl_mode) ? tbl_mode->end_tag : obuf->end_tag;
        const char* str;
        if (*line == '<' || obuf->status != R_ST_NORMAL) {
            /*
             * Tag processing
             */
            if (obuf->status == R_ST_EOL)
                obuf->status = R_ST_NORMAL;
            else {
                read_token(h_env->tagbuf, &line, &obuf->status,
                    pre_mode & RB_PREMODE, obuf->status != R_ST_NORMAL);
                if (obuf->status != R_ST_NORMAL)
                    return;
            }
            if (h_env->tagbuf->length == 0)
                continue;
            str = Strdup(h_env->tagbuf)->ptr;
            if (*str == '<') {
                if (str[1] && REALLY_THE_BEGINNING_OF_A_TAG(str))
                    is_tag = true;
                else if (!(pre_mode & (RB_PLAIN | RB_INTXTA | RB_INSELECT | RB_SCRIPT | RB_STYLE | RB_TITLE))) {
                    line = Strnew_m_charp(str + 1, line, NULL)->ptr;
                    str = "&lt;";
                }
            }
        } else {
            read_token(tokbuf, &line, &obuf->status, pre_mode & RB_PREMODE, 0);
            if (obuf->status != R_ST_NORMAL) /* R_ST_AMP ? */
                obuf->status = R_ST_NORMAL;
            str = tokbuf->ptr;
            if (need_number) {
                str = Strnew_m_charp(getLinkNumberStr(-1)->ptr, str, NULL)->ptr;
                need_number = 0;
            }
        }

        if (pre_mode & (RB_PLAIN | RB_INTXTA | RB_INSELECT | RB_SCRIPT | RB_STYLE | RB_TITLE)) {
            if (is_tag) {
                const char* p = str;
                if ((tag = parse_tag(&p, internal))) {
                    if (tag->tagid == end_tag || (pre_mode & RB_INSELECT && tag->tagid == HTML_N_FORM)
                        || (pre_mode & RB_TITLE
                            && (tag->tagid == HTML_N_HEAD
                                || tag->tagid == HTML_BODY)))
                        goto proc_normal;
                }
            }
            /* title */
            if (pre_mode & RB_TITLE) {
                feed_title(str);
                continue;
            }
            /* select */
            if (pre_mode & RB_INSELECT) {
                if (obuf->table_level >= 0)
                    goto proc_normal;
                feed_select(str);
                continue;
            }
            if (is_tag) {
                const char* p;
                if (strncmp(str, "<!--", 4) && (p = strchr(str + 1, '<'))) {
                    str = Strnew_charp_n(str, p - str)->ptr;
                    line = Strnew_m_charp(p, line, NULL)->ptr;
                }
                is_tag = false;
                continue;
            }
            if (obuf->table_level >= 0)
                goto proc_normal;
            /* textarea */
            if (pre_mode & RB_INTXTA) {
                feed_textarea(str);
                continue;
            }
            /* script */
            if (pre_mode & RB_SCRIPT)
                continue;
            /* style */
            if (pre_mode & RB_STYLE)
                continue;
        }

    proc_normal:
        if (obuf->table_level >= 0 && tbl && tbl_mode) {
            /*
             * within table: in <table>..</table>, all input tokens
             * are fed to the table renderer, and then the renderer
             * makes HTML output.
             */
            switch (feed_table(tbl, str, tbl_mode, tbl_width, internal)) {
            case 0:
                /* </table> tag */
                obuf->table_level--;
                if (obuf->table_level >= MAX_TABLE - 1)
                    continue;
                end_table(tbl);
                if (obuf->table_level >= 0) {
                    struct table* tbl0 = tables[obuf->table_level];
                    str = Sprintf("<table_alt tid=%d>", tbl0->ntable)->ptr;
                    if (tbl0->row < 0)
                        continue;
                    pushTable(tbl0, tbl);
                    tbl = tbl0;
                    tbl_mode = &table_mode[obuf->table_level];
                    tbl_width = table_width(h_env, obuf->table_level);
                    feed_table(tbl, str, tbl_mode, tbl_width, true);
                    continue;
                    /* continue to the next */
                }
                if (obuf->flag & RB_DEL)
                    continue;
                /* all tables have been read */
                if (tbl->vspace > 0 && !(obuf->flag & RB_IGNORE_P)) {
                    int indent = h_env->envs[h_env->envc].indent;
                    flushline(h_env, obuf, indent, 0, h_env->limit);
                    do_blankline(h_env, obuf, indent, 0, h_env->limit);
                }
                save_fonteffect(h_env, obuf);
                initRenderTable();
                renderTable(tbl, tbl_width, h_env);
                restore_fonteffect(h_env, obuf);
                obuf->flag &= ~RB_IGNORE_P;
                if (tbl->vspace > 0) {
                    int indent = h_env->envs[h_env->envc].indent;
                    do_blankline(h_env, obuf, indent, 0, h_env->limit);
                    obuf->flag |= RB_IGNORE_P;
                }
                set_space_to_prevchar(obuf->prevchar);
                continue;
            case 1:
                /* <table> tag */
                break;
            default:
                continue;
            }
        }

        if (is_tag) {
            /*** Beginning of a new tag ***/
            if ((tag = parse_tag(&str, internal)))
                cmd = tag->tagid;
            else
                continue;
            /* process tags */
            if (HTMLtagproc1(tag, h_env) == 0) {
                /* preserve the tag for second-stage processing */
                if (tag->need_reconstruct)
                    h_env->tagbuf = parsedtag2str(tag);
                push_tag(obuf, h_env->tagbuf->ptr, cmd);
            } else {
                process_idattr(obuf, cmd, tag);
            }
            obuf->bp.init_flag = 1;
            clear_ignore_p_flag(obuf, cmd);
            if (cmd == HTML_TABLE)
                goto table_start;
            else {
                if (displayLinkNumber && cmd == HTML_A && !internal)
                    if (h_env->obuf->anchor.url)
                        need_number = 1;
                continue;
            }
        }

        if (obuf->flag & (RB_DEL | RB_S))
            continue;
        while (*str) {
            mode = get_mctype(str);
            delta = get_mcwidth(str);
            if (obuf->flag & (RB_SPECIAL & ~RB_NOBR)) {
                char ch = *str;
                if (!(obuf->flag & RB_PLAIN) && (*str == '&')) {
                    const char* p = str;
                    int ech = getescapechar(&p);
                    if (ech == '\n' || ech == '\r') {
                        ch = '\n';
                        str = p - 1;
                    } else if (ech == '\t') {
                        ch = '\t';
                        str = p - 1;
                    }
                }
                if (ch != '\n')
                    obuf->flag &= ~RB_IGNORE_P;
                if (ch == '\n') {
                    str++;
                    if (obuf->flag & RB_IGNORE_P) {
                        obuf->flag &= ~RB_IGNORE_P;
                        continue;
                    }
                    if (obuf->flag & RB_PRE_INT)
                        PUSH(obuf, ' ');
                    else
                        flushline(h_env, obuf, h_env->envs[h_env->envc].indent,
                            1, h_env->limit);
                } else if (ch == '\t') {
                    do {
                        PUSH(obuf, ' ');
                    } while ((h_env->envs[h_env->envc].indent + obuf->pos)
                            % Tabstop
                        != 0);
                    str++;
                } else if (obuf->flag & RB_PLAIN) {
                    const char* p = html_quote_char(*str);
                    if (p) {
                        push_charp(obuf, 1, p, PC_ASCII);
                        str++;
                    } else {
                        proc_mchar(obuf, 1, delta, &str, mode);
                    }
                } else {
                    if (*str == '&')
                        proc_escape(obuf, &str);
                    else
                        proc_mchar(obuf, 1, delta, &str, mode);
                }
                if (obuf->flag & (RB_SPECIAL & ~RB_PRE_INT))
                    continue;
            } else {
                if (!IS_SPACE(*str))
                    obuf->flag &= ~RB_IGNORE_P;
                if ((mode == PC_ASCII || mode == PC_CTRL) && IS_SPACE(*str)) {
                    if (*obuf->prevchar->ptr != ' ') {
                        PUSH(obuf, ' ');
                    }
                    str++;
                } else {
                    if (mode == PC_KANJI1)
                        is_hangul = wtf_is_hangul((wc_uchar*)str);
                    else
                        is_hangul = 0;
                    if (!SimplePreserveSpace && mode == PC_KANJI1 && !is_hangul && !prev_is_hangul && obuf->pos > h_env->envs[h_env->envc].indent && Strlastchar(obuf->line) == ' ') {
                        while (obuf->line->length >= 2 && !strncmp(obuf->line->ptr + obuf->line->length - 2, "  ", 2)
                            && obuf->pos >= h_env->envs[h_env->envc].indent) {
                            Strshrink(obuf->line, 1);
                            obuf->pos--;
                        }
                        if (obuf->line->length >= 3 && obuf->prev_ctype == PC_KANJI1 && Strlastchar(obuf->line) == ' ' && obuf->pos >= h_env->envs[h_env->envc].indent) {
                            Strshrink(obuf->line, 1);
                            obuf->pos--;
                        }
                    }
                    prev_is_hangul = is_hangul;
                    if (*str == '&')
                        proc_escape(obuf, &str);
                    else
                        proc_mchar(obuf, obuf->flag & RB_SPECIAL, delta, &str,
                            mode);
                }
            }
            if (need_flushline(h_env, obuf, mode)) {
                char* bp = obuf->line->ptr + obuf->bp.len;
                char* tp = bp - obuf->bp.tlen;
                int i = 0;

                if (tp > obuf->line->ptr && tp[-1] == ' ')
                    i = 1;

                indent = h_env->envs[h_env->envc].indent;
                if (obuf->bp.pos - i > indent) {
                    Str line;
                    append_tags(obuf); /* may reallocate the buffer */
                    bp = obuf->line->ptr + obuf->bp.len;
                    line = Strnew_charp(bp);
                    Strshrink(obuf->line, obuf->line->length - obuf->bp.len);
                    if (obuf->pos - i > h_env->limit)
                        obuf->flag |= RB_FILL;
                    back_to_breakpoint(obuf);
                    flushline(h_env, obuf, indent, 0, h_env->limit);
                    obuf->flag &= ~RB_FILL;
                    HTMLlineproc0(line->ptr, h_env, true);
                }
            }
        }
    }
    if (!(obuf->flag & (RB_SPECIAL | RB_INTXTA | RB_INSELECT))) {
        char* tp;
        int i = 0;

        if (obuf->bp.pos == obuf->pos) {
            tp = &obuf->line->ptr[obuf->bp.len - obuf->bp.tlen];
        } else {
            tp = &obuf->line->ptr[obuf->line->length];
        }

        if (tp > obuf->line->ptr && tp[-1] == ' ')
            i = 1;
        indent = h_env->envs[h_env->envc].indent;
        if (obuf->pos - i > h_env->limit) {
            obuf->flag |= RB_FILL;
            flushline(h_env, obuf, indent, 0, h_env->limit);
            obuf->flag &= ~RB_FILL;
        }
    }
}

// struct Buffer*
// loadHTMLBuffer(struct UI ui, struct Url url, union input_stream* stream, wc_ces content_charset, struct Buffer* newBuf)
// {
//     Str html = readAll(stream);
//
//     if (newBuf == NULL)
//         newBuf = newBuffer();
//     // newBuf->document.charset = content_charset;
//
//     newBuf->document = loadHtmlDocument(html, baseURL(newBuf), content_charset, false);
//
//     newBuf->document.topLineIndex = newBuf->document.firstLine->linenumber;
//     newBuf->document.currentLineIndex = newBuf->document.firstLine->linenumber;
//     if (n_textarea)
//         formResetBuffer(newBuf, newBuf->document.formitem);
//
//     return newBuf;
// }

struct Document loadTextDocument(Str page, wc_ces charset)
{
    // FILE* src = NULL;
    // Str tmpf;
    // long long linelen = 0, trbyte = 0;
    // void (*prevtrap)(int _dummy) = NULL;

    // if (newBuf == NULL)
    //     newBuf = newBuffer();

    // if (sigsetjmp(AbortLoading, 1) != 0) {
    //     goto _end;
    // }
    // TRAP_ON;

    // if (newBuf->content.sourcefile == NULL && url.scheme != SCM_LOCAL) {
    //     tmpf = tmpfname(TMPF_SRC, NULL);
    //     src = fopen(tmpf->ptr, "w");
    //     if (src)
    //         newBuf->content.sourcefile = tmpf->ptr;
    // }

    // wc_ces charset = WC_CES_US_ASCII;
    // wc_ces doc_charset = DocumentCharset;
    // if (newBuf->document.charset)
    //     charset = doc_charset = newBuf->document.charset;

    // if (IStype(stream) != IST_ENCODED) {
    //     abort();
    //     // uf->stream = newEncodedStream(uf->stream, uf->encoding);
    // }

    struct Document doc = {
        .charset = charset,
        .firstLine = 0,
        .allLine = 0,
    };

    Lineprop* propBuffer = NULL;
    Linecolor* colorBuffer = NULL;
    char pre_lbuf = '\0';
    long long linelen = 0;
    union input_stream* stream = newStrStream(page);
    Str lineBuf2;
    while ((lineBuf2 = StrmyISgets(stream)) && lineBuf2->length) {
        // if (src)
        //     Strfputs(lineBuf2, src);
        linelen += lineBuf2->length;
        // showProgress(current_content_length, &linelen, &trbyte);
        lineBuf2 = convertLine(lineBuf2, HEADER_MODE, &doc.charset, doc.charset, InnerCharset);
        if (squeezeBlankLine) {
            if (lineBuf2->ptr[0] == '\n' && pre_lbuf == '\n') {
                continue;
            }
            pre_lbuf = lineBuf2->ptr[0];
        }
        Strchop(lineBuf2);
        lineBuf2 = checkType(lineBuf2, &propBuffer, NULL);
        {
            struct LineList* l = addNewline(currentLine(&doc),
                lineBuf2->ptr, propBuffer, colorBuffer, lineBuf2->length, -1, doc.allLine++);
            doc.currentLineIndex = l->linenumber;
            if (doc.firstLine == NULL) {
                doc.firstLine = l;
            }
        }
    }
_end:
    ISclose(stream);

    // term_raw();
    doc.topLineIndex = doc.firstLine->linenumber;
    doc.currentLineIndex = doc.firstLine->linenumber;
    // newBuf->trbyte = trbyte + linelen;
    // doc.charset = charset;
    // if (src)
    //     fclose(src);

    return doc;
}

struct Document loadContent(struct Content* content, int cols, bool use_graphic)
{
    if (content->cc.content_type == CONTENTTYPE_TEXT_HTML)
        return loadHtmlDocument(content->url, content->page, content->cc.charset,
            cols, use_graphic, false);
    else
        return loadTextDocument(content->page, content->cc.charset);
}

bool PermitSaveToPipe = (false);

bool canCopyFile(const char* path1, const char* path2)
{
    if (*path2 == '|' && PermitSaveToPipe)
        return true;

    struct stat st1, st2;
    if ((stat(path1, &st1) == 0) && (stat(path2, &st2) == 0))
        if (st1.st_ino == st2.st_ino)
            return false;

    return true;
}

#define SAVE_BUF_SIZE 1536

static int _MoveFile(const char* path1, const char* path2)
{
    InputStream f1;
    FILE* f2;
    int is_pipe;
    long long linelen = 0, trbyte = 0;
    char* buf = NULL;
    int count;

    f1 = openIS(path1);
    if (f1 == NULL)
        return -1;
    if (*path2 == '|' && PermitSaveToPipe) {
        is_pipe = true;
        f2 = popen(path2 + 1, "w");
    } else {
        is_pipe = false;
        f2 = fopen(path2, "wb");
    }
    if (f2 == NULL) {
        ISclose(f1);
        return -1;
    }

    int current_content_length = 0;
    buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
    while ((count = ISread_n(f1, buf, SAVE_BUF_SIZE)) > 0) {
        fwrite(buf, 1, count, f2);
        linelen += count;
        // showProgress(current_content_length, &linelen, &trbyte);
    }
    xfree(buf);
    ISclose(f1);
    if (is_pipe)
        pclose(f2);
    else
        fclose(f2);
    return 0;
}

int setModtime(const char* path, time_t modtime)
{
    struct utimbuf t;
    struct stat st;
    if (stat(path, &st) == 0)
        t.actime = st.st_atime;
    else
        t.actime = time(NULL);
    t.modtime = modtime;
    return utime(path, &t);
}
