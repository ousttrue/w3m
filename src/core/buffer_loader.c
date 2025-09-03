#include "buffer_loader.h"
#include "alloc.h"
#include "buffer.h"
#include "readbuffer.h"
#include "mysignal.h"
#include "table.h"
#include "symbol.h"
#include "ui.h"
#include "istream.h"
#include "image.h"
#include "display.h"
#include "progress.h"
#include "html_title.h"
#include "readbuffer.h"
#include "HtmlTagParsed.h"
#include "indep.h"
#include "form.h"
#include "map.h"
#include "w3m.h"
#include "funcname1.h"
#include <myctype.h>

#define DOCUMENT_CHARSET WC_CES_UTF_8

char UseContentCharset = (true);
wc_ces DocumentCharset = (DOCUMENT_CHARSET);
int autoImage = (true);
char MetaRefresh = (false);

long long current_content_length;
ParsedURL* cur_baseURL = NULL;

static TextLineListItem* _tl_lp2;

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

static int currentLn(Buffer* buf)
{
    if (buf->currentLine)
        /*     return buf->currentLine->real_linenumber + 1;      */
        return buf->currentLine->linenumber + 1;
    else
        return 1;
}

static void
addLink(Buffer* buf, struct HtmlTagParsed* tag)
{
    char *href = NULL, *title = NULL, *ctype = NULL, *rel = NULL, *rev = NULL;
    char type = LINK_TYPE_NONE;
    LinkList* l;

    parsedtag_get_value(tag, ATTR_HREF, &href);
    if (href)
        href = url_encode(remove_space(href), baseURL(buf),
            buf->document_charset);
    parsedtag_get_value(tag, ATTR_TITLE, &title);
    parsedtag_get_value(tag, ATTR_TYPE, &ctype);
    parsedtag_get_value(tag, ATTR_REL, &rel);
    if (rel != NULL) {
        /* forward link type */
        type = LINK_TYPE_REL;
        if (title == NULL)
            title = rel;
    }
    parsedtag_get_value(tag, ATTR_REV, &rev);
    if (rev != NULL) {
        /* reverse link type */
        type = LINK_TYPE_REV;
        if (title == NULL)
            title = rev;
    }

    l = New(LinkList);
    l->url = href;
    l->title = title;
    l->ctype = ctype;
    l->type = type;
    l->next = NULL;
    if (buf->linklist) {
        LinkList* i;
        for (i = buf->linklist; i->next; i = i->next)
            ;
        i->next = l;
    } else
        buf->linklist = l;
}

int getMetaRefreshParam(char* q, Str* refresh_uri)
{
    int refresh_interval;
    char* r;
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
            r = q;
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

static void
addnewline2(Buffer* buf, char* line, Lineprop* prop, Linecolor* color, int pos,
    int nlines)
{
    Line* l;
    l = New(Line);
    l->next = NULL;
    l->lineBuf = line;
    l->propBuf = prop;
    l->colorBuf = color;
    l->len = pos;
    l->width = -1;
    l->size = pos;
    l->bpos = 0;
    l->bwidth = 0;
    l->prev = buf->currentLine;
    if (buf->currentLine) {
        l->next = buf->currentLine->next;
        buf->currentLine->next = l;
    } else
        l->next = NULL;
    if (buf->lastLine == NULL || buf->lastLine == buf->currentLine)
        buf->lastLine = l;
    buf->currentLine = l;
    if (buf->firstLine == NULL)
        buf->firstLine = l;
    l->linenumber = ++buf->allLine;
    if (nlines < 0) {
        /*     l->real_linenumber = l->linenumber;     */
        l->real_linenumber = 0;
    } else {
        l->real_linenumber = nlines;
    }
    l = NULL;
}

void addnewline(Buffer* buf, char* line, Lineprop* prop, Linecolor* color, int pos,
    int width, int nlines)
{
    char* s;
    Lineprop* p;
    Linecolor* c;
    Line* l;
    int i, bpos, bwidth;

    if (pos > 0) {
        s = allocStr(line, pos);
        p = NewAtom_N(Lineprop, pos);
        memcpy(p, prop, pos * sizeof(Lineprop));
    } else {
        s = NullLine;
        p = NullProp;
    }
    if (pos > 0 && color) {
        c = NewAtom_N(Linecolor, pos);
        memcpy(c, color, pos * sizeof(Linecolor));
    } else {
        c = NULL;
    }
    addnewline2(buf, s, p, c, pos, nlines);
    if (pos <= 0 || width <= 0)
        return;
    bpos = 0;
    bwidth = 0;
    while (1) {
        l = buf->currentLine;
        l->bpos = bpos;
        l->bwidth = bwidth;
        i = columnLen(l, width);
        if (i == 0) {
            i++;
            while (i < l->len && p[i] & PC_WCHAR2)
                i++;
        }
        l->len = i;
        l->width = COLPOS(l, l->len);
        if (pos <= i)
            return;
        bpos += l->len;
        bwidth += l->width;
        s += i;
        p += i;
        if (c)
            c += i;
        pos -= i;
        addnewline2(buf, s, p, c, pos, nlines);
    }
}

static void
HTMLlineproc2body(Buffer* buf, Str (*feed)(), int llimit)
{
    static char* outc = NULL;
    static Lineprop* outp = NULL;
    static int out_size = 0;
    Anchor *a_href = NULL, *a_img = NULL, *a_form = NULL;
    char *p, *q, *r, *s, *t, *str;
    Lineprop mode, effect, ex_effect;
    int pos;
    int nlines;
#ifdef DEBUG
    FILE* debug = NULL;
#endif
    char* id = NULL;
    int hseq, form_id;
    Str line;
    char* endp;
    char symbol = '\0';
    int internal = 0;
    Anchor** a_textarea = NULL;
    Anchor** a_select = NULL;
#if defined(USE_M17N) || defined(USE_IMAGE)
    ParsedURL* base = baseURL(buf);
#endif
    wc_ces name_charset = url_to_charset(NULL, &buf->currentURL,
        buf->document_charset);

    if (out_size == 0) {
        out_size = LINELEN;
        outc = NewAtom_N(char, out_size);
        outp = NewAtom_N(Lineprop, out_size);
    }

    int max_textarea;
    int max_select;
    initParser(&max_textarea, &max_select);
    if (!max_textarea) { /* halfload */
        a_textarea = New_N(Anchor*, max_textarea);
    }
    if (!max_select) { /* halfload */
        a_select = New_N(Anchor*, max_select);
    }

#ifdef DEBUG
    if (w3m_debug)
        debug = fopen("zzzerr", "a");
#endif

    effect = 0;
    ex_effect = 0;
    nlines = 0;
    while ((line = feed()) != NULL) {
#ifdef DEBUG
        if (w3m_debug) {
            Strfputs(line, debug);
            fputc('\n', debug);
        }
#endif
        if (n_textarea >= 0 && *(line->ptr) != '<') { /* halfload */
            Strcat(textarea_str[n_textarea], line);
            continue;
        }
    proc_again:
        if (++nlines == llimit)
            break;
        pos = 0;
#ifdef ENABLE_REMOVE_TRAILINGSPACES
        Strremovetrailingspaces(line);
#endif
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
                if (!(tag = parse_tag(&str, TRUE)))
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
                    q = buf->baseTarget;
                    t = "";
                    hseq = 0;
                    id = NULL;
                    if (parsedtag_get_value(tag, ATTR_NAME, &id)) {
                        id = url_quote_conv(id, name_charset);
                        registerName(buf, id, currentLn(buf), pos);
                    }
                    if (parsedtag_get_value(tag, ATTR_HREF, &p))
                        p = url_encode(remove_space(p), base,
                            buf->document_charset);
                    if (parsedtag_get_value(tag, ATTR_TARGET, &q))
                        q = url_quote_conv(q, buf->document_charset);
                    if (parsedtag_get_value(tag, ATTR_REFERER, &r))
                        r = url_encode(r, base,
                            buf->document_charset);
                    parsedtag_get_value(tag, ATTR_TITLE, &s);
                    parsedtag_get_value(tag, ATTR_ACCESSKEY, &t);
                    parsedtag_get_value(tag, ATTR_HSEQ, &hseq);
                    if (hseq > 0)
                        buf->hmarklist = putHmarker(buf->hmarklist, currentLn(buf),
                            pos, hseq - 1);
                    else if (hseq < 0) {
                        int h = -hseq - 1;
                        if (buf->hmarklist && h < buf->hmarklist->nmark && buf->hmarklist->marks[h].invalid) {
                            buf->hmarklist->marks[h].pos = pos;
                            buf->hmarklist->marks[h].line = currentLn(buf);
                            buf->hmarklist->marks[h].invalid = 0;
                            hseq = -hseq;
                        }
                    }
                    if (p) {
                        effect |= PE_ANCHOR;
                        a_href = registerHref(buf, p, q, r, s,
                            *t, currentLn(buf), pos);
                        a_href->hseq = ((hseq > 0) ? hseq : -hseq) - 1;
                        a_href->slave = (hseq > 0) ? FALSE : TRUE;
                    }
                    break;
                case HTML_N_A:
                    effect &= ~PE_ANCHOR;
                    if (a_href) {
                        a_href->end.line = currentLn(buf);
                        a_href->end.pos = pos;
                        if (a_href->start.line == a_href->end.line && a_href->start.pos == a_href->end.pos) {
                            if (buf->hmarklist && a_href->hseq >= 0 && a_href->hseq < buf->hmarklist->nmark)
                                buf->hmarklist->marks[a_href->hseq].invalid = 1;
                            a_href->hseq = -1;
                        }
                        a_href = NULL;
                    }
                    break;

                case HTML_LINK:
                    addLink(buf, tag);
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
                            buf->imarklist = putHmarker(buf->imarklist,
                                currentLn(buf), pos,
                                iseq - 1);
                        }
                        s = NULL;
                        parsedtag_get_value(tag, ATTR_TITLE, &s);
                        p = url_quote_conv(remove_space(p),
                            buf->document_charset);
                        a_img = registerImg(buf, p, s, currentLn(buf), pos);
                        a_img->hseq = iseq;
                        a_img->image = NULL;
                        if (iseq > 0) {
                            ParsedURL u;
                            Image* image;

                            parseURL2(a_img->url, &u, base);
                            a_img->image = image = New(Image);
                            image->url = parsedURL2Str(&u)->ptr;
                            if (!uncompressed_file_type(u.file, &image->ext))
                                image->ext = filename_extension(u.file, TRUE);
                            image->cache = NULL;
                            image->width = (w > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : w;
                            image->height = (h > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : h;
                            image->xoffset = xoffset;
                            image->yoffset = yoffset;
                            image->y = currentLn(buf) - top;
                            if (image->xoffset < 0 && pos == 0)
                                image->xoffset = 0;
                            if (image->yoffset < 0 && image->y == 1)
                                image->yoffset = 0;
                            image->rows = 1 + top + bottom;
                            image->map = q;
                            image->ismap = ismap;
                            image->touch = 0;
                            image->cache = getImage(image, base,
                                IMG_FLAG_SKIP);
                        } else if (iseq < 0) {
                            BufferPoint* po = buf->imarklist->marks - iseq - 1;
                            Anchor* a = retrieveAnchor(buf->img,
                                po->line, po->pos);
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
                        a_img->end.line = currentLn(buf);
                        a_img->end.pos = pos;
                    }
                    a_img = NULL;
                    break;
                case HTML_INPUT_ALT: {
                    FormList* form;
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
                        buf->hmarklist = putHmarker(buf->hmarklist, currentLn(buf),
                            hpos, hseq - 1);
                    } else if (hseq < 0) {
                        int h = -hseq - 1;
                        int hpos = pos;
                        if (*str == '[')
                            hpos++;
                        if (buf->hmarklist && h < buf->hmarklist->nmark && buf->hmarklist->marks[h].invalid) {
                            buf->hmarklist->marks[h].pos = hpos;
                            buf->hmarklist->marks[h].line = currentLn(buf);
                            buf->hmarklist->marks[h].invalid = 0;
                            hseq = -hseq;
                        }
                    }

                    if (!form->target)
                        form->target = buf->baseTarget;
                    if (a_textarea && parsedtag_get_value(tag, ATTR_TEXTAREANUMBER, &textareanumber)) {
                        if (textareanumber >= max_textarea) {
                            max_textarea = 2 * textareanumber;
                            textarea_str = New_Reuse(Str, textarea_str,
                                max_textarea);
                            a_textarea = New_Reuse(Anchor*, a_textarea,
                                max_textarea);
                        }
                    }
                    if (a_select && parsedtag_get_value(tag, ATTR_SELECTNUMBER, &selectnumber)) {
                        if (selectnumber >= max_select) {
                            max_select = 2 * selectnumber;
                            select_option = New_Reuse(FormSelectOption,
                                select_option,
                                max_select);
                            a_select = New_Reuse(Anchor*, a_select,
                                max_select);
                        }
                    }
                    a_form = registerForm(buf, form, tag, currentLn(buf), pos);
                    if (a_textarea && textareanumber >= 0)
                        a_textarea[textareanumber] = a_form;
                    if (a_select && selectnumber >= 0)
                        a_select[selectnumber] = a_form;
                    if (a_form) {
                        a_form->hseq = hseq - 1;
                        a_form->y = currentLn(buf) - top;
                        a_form->rows = 1 + top + bottom;
                        if (!parsedtag_exists(tag, ATTR_NO_EFFECT))
                            effect |= PE_FORM;
                        break;
                    }
                }
                case HTML_N_INPUT_ALT:
                    effect &= ~PE_FORM;
                    if (a_form) {
                        a_form->end.line = currentLn(buf);
                        a_form->end.pos = pos;
                        if (a_form->start.line == a_form->end.line && a_form->start.pos == a_form->end.pos)
                            a_form->hseq = -1;
                    }
                    a_form = NULL;
                    break;
                case HTML_MAP:
                    if (parsedtag_get_value(tag, ATTR_NAME, &p)) {
                        MapList* m = New(MapList);
                        m->name = Strnew_charp(p);
                        m->area = newGeneralList();
                        m->next = buf->maplist;
                        buf->maplist = m;
                    }
                    break;
                case HTML_N_MAP:
                    /* nothing to do */
                    break;
                case HTML_AREA:
                    if (buf->maplist == NULL) /* outside of <map>..</map> */
                        break;
                    if (parsedtag_get_value(tag, ATTR_HREF, &p)) {
                        MapArea* a;
                        p = url_encode(remove_space(p), base,
                            buf->document_charset);
                        t = NULL;
                        parsedtag_get_value(tag, ATTR_TARGET, &t);
                        q = "";
                        parsedtag_get_value(tag, ATTR_ALT, &q);
                        r = NULL;
                        s = NULL;
                        parsedtag_get_value(tag, ATTR_SHAPE, &r);
                        parsedtag_get_value(tag, ATTR_COORDS, &s);
                        a = newMapArea(p, t, q, r, s);
                        pushValue(buf->maplist->area, (void*)a);
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
                        p = url_encode(remove_space(p), NULL,
                            buf->document_charset);
                        if (!buf->baseURL)
                            buf->baseURL = New(ParsedURL);
                        parseURL2(p, buf->baseURL, &buf->currentURL);
#if defined(USE_M17N) || defined(USE_IMAGE)
                        base = buf->baseURL;
#endif
                    }
                    if (parsedtag_get_value(tag, ATTR_TARGET, &p))
                        buf->baseTarget = url_quote_conv(p, buf->document_charset);
                    break;
                case HTML_META:
                    p = q = NULL;
                    parsedtag_get_value(tag, ATTR_HTTP_EQUIV, &p);
                    parsedtag_get_value(tag, ATTR_CONTENT, &q);
                    if (p && q && !strcasecmp(p, "refresh") && MetaRefresh) {
                        Str tmp = NULL;
                        int refresh_interval = getMetaRefreshParam(q, &tmp);
                        if (tmp) {
                            p = url_encode(remove_space(tmp->ptr), base,
                                buf->document_charset);
                            buf->event = setAlarmEvent(buf->event,
                                refresh_interval,
                                AL_IMPLICIT_ONCE,
                                FUNCNAME_gorURL, p);
                        } else if (refresh_interval > 0)
                            buf->event = setAlarmEvent(buf->event,
                                refresh_interval,
                                AL_IMPLICIT,
                                FUNCNAME_reload, NULL);
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
                        FormItemList* item = (FormItemList*)a_textarea[n_textarea]->url;
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
                        FormItemList* item = (FormItemList*)a_select[n_select]->url;
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
                        buf->buffername = html_unquote(p);
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
                    id = url_quote_conv(id, name_charset);
                    registerName(buf, id, currentLn(buf), pos);
                }
            }
        }
        /* end of processing for one line */
        if (!internal)
            addnewline(buf, outc, outp, NULL, pos, -1, nlines);
        if (internal == HTML_N_INTERNAL)
            internal = 0;
        if (str != endp) {
            line = Strsubstr(line, str - line->ptr, endp - str);
            goto proc_again;
        }
    }
#ifdef DEBUG
    if (w3m_debug)
        fclose(debug);
#endif
    for (form_id = 1; form_id <= form_max; form_id++)
        if (forms[form_id])
            forms[form_id]->next = forms[form_id - 1];
    buf->formlist = (form_max >= 0) ? forms[form_max] : NULL;
    if (n_textarea)
        addMultirowsForm(buf, buf->formitem);
    addMultirowsImg(buf, buf->img);
}

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

void HTMLlineproc2(Buffer* buf, TextLineList* tl)
{
    _tl_lp2 = tl->first;
    HTMLlineproc2body(buf, textlist_feed, -1);
}

void loadHTML(Str html, wc_ces doc_charset, int cols, bool use_graphic, bool internal, struct _Buffer* buf)
{
    struct environment envs[MAX_ENV_LEVEL];
    long long linelen = 0;
    long long trbyte = 0;
    Str lineBuf2 = Strnew();
    wc_ces charset = WC_CES_US_ASCII;
    // wc_ces doc_charset = DocumentCharset;
    struct html_feed_environ htmlenv1;
    struct readbuffer obuf;
    MySignalHandler (*volatile prevtrap)(int _dummy) = NULL;

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

    init_henv(&htmlenv1, &obuf, envs, MAX_ENV_LEVEL, NULL, cols, 0);

    htmlenv1.buf = newTextLineList();
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
    // if (content_charset && UseContentCharset)
    //     doc_charset = content_charset;

    struct URLFile f;
    init_stream(&f, SCM_LOCAL, newStrStream(html));
    meta_charset = 0;
    while ((lineBuf2 = StrmyUFgets(&f)) && lineBuf2->length) {
        // if (src)
        //     Strfputs(lineBuf2, src);
        linelen += lineBuf2->length;
        showProgress(current_content_length, &linelen, &trbyte);
        // if (meta_charset) { /* <META> */
        //     if (content_charset == 0 && UseContentCharset) {
        //         doc_charset = meta_charset;
        //         charset = WC_CES_US_ASCII;
        //     }
        //     meta_charset = 0;
        // }
        lineBuf2 = convertLine(&f, lineBuf2, HTML_MODE, &charset, doc_charset);
        // cur_document_charset = charset;
        HTMLlineproc0(lineBuf2->ptr, &htmlenv1, internal);
    }
    if (obuf.status != R_ST_NORMAL) {
        HTMLlineproc0("\n", &htmlenv1, internal);
    }
    obuf.status = R_ST_NORMAL;
    completeHTMLstream(&htmlenv1, &obuf);
    flushline(&htmlenv1, &obuf, 0, 2, htmlenv1.limit);
    cur_baseURL = NULL;
    cur_document_charset = 0;

    // Buffer* buf = newBuffer();
    if (htmlenv1.title)
        buf->buffername = htmlenv1.title;

phase2:
    buf->trbyte = trbyte + linelen;
    // TRAP_OFF;
    buf->document_charset = charset;
    buf->image_flag = image_flag;
    HTMLlineproc2(buf, htmlenv1.buf);
    // return buf;
}
