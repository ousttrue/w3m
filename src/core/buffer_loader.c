#include "buffer_loader.h"
#include "HttpClient.h"
#include "mysignal.h"
#include "siteconf.h"
#include "screen.h"
#include "auth.h"
#include "local.h"
#include "tty.h"
#include "rc.h"
#include "etc.h"
#include "mailcap.h"
#include "alloc.h"
#include "file_copy.h"
#include "http.h"
#include "tmpfile.h"
#include "buffer.h"
#include "readbuffer.h"
// #include "mysignal.h"
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
#include <strings.h>
// #include <setjmp.h>
// #include <signal.h>

#define DOCUMENT_CHARSET WC_CES_UTF_8

// static sigjmp_buf AbortLoading;
// static MySignalHandler KeyAbort(int _dummy)
// {
//     siglongjmp(AbortLoading, 1);
// }

char UseContentCharset = (true);
wc_ces DocumentCharset = (DOCUMENT_CHARSET);
int autoImage = (true);
char MetaRefresh = (false);
char DecodeCTE = (false);
int label_topline = (false);
int UseExternalDirBuffer = (true);
char* DirBufferCommand = ("file:///$LIB/dirlist" CGI_EXTENSION);
char* DefaultType = (NULL);
int displayLinkNumber = (false);
char SimplePreserveSpace = (false);
int squeezeBlankLine = (false);

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

Buffer* page_loaded(ParsedURL pu, struct URLFile f,
    Str page, wc_ces charset, const char* real_type, TextList* document_header, bool do_download)
{
    if (page) {
        if (image_source)
            return NULL;
        Str tmp = tmpfname(TMPF_SRC, ".html");
        FILE* src = fopen(tmp->ptr, "w");
        if (src) {
            Str s = wc_Str_conv_strict(page, InnerCharset, charset);
            Strfputs(s, src);
            fclose(src);
        }
        if (do_download) {
            if (!src)
                return NULL;
            const char* file = guessFileName(pu.file);
            doFileMove(tmp->ptr, file);
            return NO_BUFFER;
        }
        Buffer* b = loadHTMLString(page);
        if (b) {
            copyParsedURL(&b->currentURL, &pu);
            b->real_scheme = pu.scheme;
            b->real_type = real_type;
            if (src)
                b->sourcefile = tmp->ptr;
            b->document_charset = charset;
        }
        return b;
    }

    current_content_length = 0;
    const char* p;
    if ((p = getHttpHeaderValue(document_header, "Content-Length:")) != NULL)
        current_content_length = strtoclen(p);
    if (do_download) {
        /* download only */
        if (DecodeCTE && IStype(f.stream) != IST_ENCODED)
            f.stream = newEncodedStream(f.stream, f.encoding);
        const char* file;
        if (pu.scheme == SCM_LOCAL) {
            struct stat st;
            if (PreserveTimestamp && !stat(pu.real_file, &st))
                f.modtime = st.st_mtime;
            file = conv_from_system(guessSaveName(NULL, pu.real_file));
        } else
            file = guessSaveName(document_header, pu.file);
        if (doFileSave(f, file, current_content_length) == 0)
            UFhalfclose(&f);
        else
            UFclose(&f);
        return NO_BUFFER;
    }

    Buffer* t_buf = newBuffer();
    if ((f.content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
        uncompress_stream(&f, &pu.real_file);
    } else if (f.compression != CMP_NOCOMPRESS) {
        if (is_text_type(real_type) || searchExtViewer(real_type)) {
            uncompress_stream(&f, &t_buf->sourcefile);
            uncompressed_file_type(pu.file, &f.ext);
        } else {
            real_type = compress_application_type(f.compression);
            f.compression = CMP_NOCOMPRESS;
        }
    }

    if (image_source) {
        Buffer* b = NULL;
        if (IStype(f.stream) != IST_ENCODED)
            f.stream = newEncodedStream(f.stream, f.encoding);
        if (save2tmp(f, image_source) == 0) {
            b = newBuffer();
            b->sourcefile = image_source;
            b->real_type = real_type;
        }
        UFclose(&f);
        return b;
    }

    if (t_buf == NULL)
        t_buf = newBuffer();
    copyParsedURL(&t_buf->currentURL, &pu);
    t_buf->filename = pu.real_file ? pu.real_file : pu.file ? conv_to_system(pu.file)
                                                            : NULL;
    t_buf->ssl_certificate = f.ssl_certificate;

    // Buffer* (*proc)(struct URLFile*, Buffer*) = loadBuffer;
    Buffer* b;
    if (is_html_type(real_type)) {
        b = loadHTMLBuffer(&f, t_buf);
        b->type = "text/html";
    } else {
        b = loadBuffer(&f, t_buf);
        b->type = "text/plain";
    }
    if (b) {
        if (b->buffername == NULL || b->buffername[0] == '\0') {
            b->buffername = getHttpHeaderValue(document_header, "Subject:");
            if (b->buffername == NULL && b->filename != NULL)
                b->buffername = conv_from_system(lastFileName(b->filename));
        }
        if (b->currentURL.scheme == SCM_UNKNOWN)
            b->currentURL.scheme = f.scheme;
        if (f.scheme == SCM_LOCAL && b->sourcefile == NULL)
            b->sourcefile = b->filename;
    }

    UFclose(&f);
    if (b && b != NO_BUFFER) {
        b->real_scheme = f.scheme;
        b->real_type = real_type;
        if (pu.label) {
            if (is_html_type(real_type)) {
                Anchor* a;
                a = searchURLLabel(b, pu.label);
                if (a != NULL) {
                    gotoLine(b, a->start.line);
                    if (label_topline)
                        b->topLine = lineSkip(b, b->topLine,
                            b->currentLine->linenumber
                                - b->topLine->linenumber,
                            false);
                    b->pos = a->start.pos;
                    arrangeCursor(b);
                }
            } else { /* plain text */
                int l = atoi(pu.label);
                gotoRealLine(b, l);
                b->pos = 0;
                arrangeCursor(b);
            }
        }
    }
    if (header_string)
        header_string = NULL;
    if (b && b != NO_BUFFER)
        preFormUpdateBuffer(b);
    return b;
}

/*
 * loadGeneralFile: load file to buffer
 */
Buffer*
loadGeneralFile(char* path, ParsedURL* current, const char* referer,
    int flag, FormList* request, bool do_download)
{
    ParsedURL pu;
    const char* tpath = path;
    const char* t = "text/plain";
    const char* p;
    const char* real_type = NULL;
    // int  searchHeader = SearchHeader;
    MySignalHandler (*prevtrap)(int _dummy) = NULL;
    TextList* extra_header = newTextList();
    Str uname = NULL;
    Str pwd = NULL;
    Str realm = NULL;
    bool add_auth_cookie_flag = 0;
    struct URLOption url_option;
    Str page = NULL;
    wc_ces charset = WC_CES_US_ASCII;
    struct HttpRequest hr;
    ParsedURL* auth_pu;

    struct HttpClient c;
    initHttpClient(&c);

load_doc: {
    parseURL2(tpath, &pu, current);
    const char* sc_redirect = query_SCONF_SUBSTITUTE_URL(&pu);
    if (sc_redirect && *sc_redirect && checkRedirection(&c, &pu)) {
        tpath = (char*)sc_redirect;
        request = NULL;
        add_auth_cookie_flag = 0;
        current = New(ParsedURL);
        *current = pu;
        c.status = HTST_NORMAL;
        goto load_doc;
    }
}

    term_raw();
    url_option.referer = referer;
    url_option.flag = flag;
    openURL(&c, tpath, &pu, current, &url_option, request, extra_header, &hr);
    content_charset = 0;
    if (!c.f.stream) {
        switch (c.f.scheme) {
        case SCM_LOCAL: {
            struct stat st;
            if (stat(pu.real_file, &st) < 0)
                return NULL;
            if (S_ISDIR(st.st_mode)) {
                if (UseExternalDirBuffer) {
                    Str cmd = Sprintf("%s?dir=%s#current",
                        DirBufferCommand, pu.file);
                    Buffer* b = loadGeneralFile(cmd->ptr, NULL, NO_REFERER, 0,
                        NULL, do_download);
                    if (b != NULL && b != NO_BUFFER) {
                        copyParsedURL(&b->currentURL, &pu);
                        b->filename = b->currentURL.real_file;
                    }
                    return b;
                } else {
                    page = loadLocalDir(pu.real_file);
                    t = "local:directory";
                    charset = SystemCharset;
                }
            }
        } break;
        case SCM_UNKNOWN:
            /* FIXME: gettextize? */
            message(getUI(), MSG_ERR, Sprintf("Unknown URI: %s", parsedURL2Str(&pu)->ptr)->ptr);
            break;

        default:
            break;
        }
        if (page && page->length > 0) {
            term_raw();
            return page_loaded(pu, c.f, page, charset, t, NULL, do_download);
        }
        return NULL;
    }

    if (c.status == HTST_MISSING) {
        term_raw();
        UFclose(&c.f);
        return NULL;
    }

    /* openURL() succeeded */
    // if (sigsetjmp(AbortLoading, 1) != 0) {
    //     /* transfer interrupted */
    //     term_raw();
    //     if (b)
    //         discardBuffer(b);
    //     UFclose(&f);
    //     return NULL;
    // }

    // Buffer *b = NULL;
    if (c.f.is_cgi) {
        /* local CGI */
        // searchHeader = true;
    }
    if (header_string)
        header_string = NULL;
    // TRAP_ON;
    if (pu.scheme == SCM_HTTP || pu.scheme == SCM_HTTPS) {

        term_cbreak();
        /* FIXME: gettextize? */
        message(getUI(), MSG_INFO, Sprintf("%s contacted. Waiting for reply...", pu.host)->ptr);
        // refresh(ttyWriter());

        struct HttpResponse response = readHttpResponse(&c.f, &pu);
        if (((response.status_code >= 301 && response.status_code <= 303)
                || response.status_code == 307)
            && (p = (char*)getHttpHeaderValue(response.headers, "Location:")) != NULL
            && checkRedirection(&c, &pu)) {
            /* document moved */
            /* 301: Moved Permanently */
            /* 302: Found */
            /* 303: See Other */
            /* 307: Temporary Redirect (HTTP/1.1) */
            tpath = url_encode(p, NULL, 0);
            request = NULL;
            UFclose(&c.f);
            current = New(ParsedURL);
            copyParsedURL(current, &pu);
            // t_buf->bufferprop |= BP_REDIRECTED;
            c.status = HTST_NORMAL;
            goto load_doc;
        }
        struct ContentTypeCharset cc = getContentType(response.headers);
        t = cc.content_type;
        if (t == NULL && pu.file != NULL) {
            if (!((response.status_code >= 400 && response.status_code <= 407) || (response.status_code >= 500 && response.status_code <= 505)))
                t = guessContentType(pu.file);
        }
        if (t == NULL)
            t = "text/plain";
        if (add_auth_cookie_flag && realm && uname && pwd) {
            /* If authorization is required and passed */
            add_auth_user_passwd(&pu, qstr_unquote(realm)->ptr, uname, pwd,
                0);
            add_auth_cookie_flag = 0;
        }
        if ((p = getHttpHeaderValue(response.headers, "WWW-Authenticate:")) != NULL && response.status_code == 401) {
            /* Authentication needed */
            struct http_auth hauth;
            if (findAuthentication(&hauth, response.headers, "WWW-Authenticate:") != NULL
                && (realm = get_auth_param(hauth.param, "realm")) != NULL) {
                auth_pu = &pu;
                getAuthCookie(&hauth, "Authorization:", extra_header,
                    auth_pu, &hr, request, &uname, &pwd);
                if (uname == NULL) {
                    /* abort */
                    term_raw();
                    return page_loaded(pu, c.f, page, charset, t, response.headers, do_download);
                }
                UFclose(&c.f);
                add_auth_cookie_flag = 1;
                c.status = HTST_NORMAL;
                goto load_doc;
            }
        }
        if ((p = getHttpHeaderValue(response.headers, "Proxy-Authenticate:")) != NULL && response.status_code == 407) {
            /* Authentication needed */
            struct http_auth hauth;
            if (findAuthentication(&hauth, response.headers, "Proxy-Authenticate:")
                    != NULL
                && (realm = get_auth_param(hauth.param, "realm")) != NULL) {
                auth_pu = schemeToProxy(pu.scheme);
                getAuthCookie(&hauth, "Proxy-Authorization:",
                    extra_header, auth_pu, &hr, request,
                    &uname, &pwd);
                if (uname == NULL) {
                    /* abort */
                    term_raw();
                    return page_loaded(pu, c.f, page, charset, t, response.headers, do_download);
                }
                UFclose(&c.f);
                add_auth_cookie_flag = 1;
                c.status = HTST_NORMAL;
                add_auth_user_passwd(auth_pu, qstr_unquote(realm)->ptr, uname, pwd, 1);
                goto load_doc;
            }
        }
        /* XXX: RFC2617 3.2.3 Authentication-Info: ? */

        if (c.status == HTST_CONNECT) {
            goto load_doc;
        }

        c.f.modtime = mymktime(getHttpHeaderValue(response.headers, "Last-Modified:"));
    } else if (pu.scheme == SCM_FTP) {
        check_compression(&c.f, path);
        if (c.f.compression != CMP_NOCOMPRESS) {
            char* t1 = (char*)uncompressed_file_type(pu.file, NULL);
            real_type = c.f.guess_type;
            if (t1)
                t = t1;
            else
                t = real_type;
        } else {
            real_type = guessContentType(pu.file);
            if (real_type == NULL)
                real_type = "text/plain";
            t = real_type;
        }
    } else if (pu.scheme == SCM_DATA) {
        t = c.f.guess_type;
    }
    //     else if (searchHeader) {
    //         searchHeader = SearchHeader = false;
    //         if (t_buf == NULL)
    //             t_buf = newBuffer();
    //         readHeader(&f, t_buf, searchHeader_through, &pu);
    //         if (f.is_cgi && (p = checkHeader(t_buf, "Location:")) != NULL && checkRedirection(&pu)) {
    //             /* document moved */
    //             tpath = url_encode(remove_space(p), NULL, 0);
    //             request = NULL;
    //             UFclose(&f);
    //             add_auth_cookie_flag = 0;
    //             current = New(ParsedURL);
    //             copyParsedURL(current, &pu);
    //             t_buf = newBuffer();
    //             t_buf->bufferprop |= BP_REDIRECTED;
    //             status = HTST_NORMAL;
    //             goto load_doc;
    //         }
    // #ifdef AUTH_DEBUG
    //         if ((p = checkHeader(t_buf, "WWW-Authenticate:")) != NULL) {
    //             /* Authentication needed */
    //             struct http_auth hauth;
    //             if (findAuthentication(&hauth, t_buf, "WWW-Authenticate:") != NULL
    //                 && (realm = get_auth_param(hauth.param, "realm")) != NULL) {
    //                 auth_pu = &pu;
    //                 getAuthCookie(&hauth, "Authorization:", extra_header,
    //                     auth_pu, &hr, request, &uname, &pwd);
    //                 if (uname == NULL) {
    //                     /* abort */
    //                     TRAP_OFF;
    //                     goto page_loaded;
    //                 }
    //                 UFclose(&f);
    //                 add_auth_cookie_flag = 1;
    //                 status = HTST_NORMAL;
    //                 goto load_doc;
    //             }
    //         }
    // #endif /* defined(AUTH_DEBUG) */
    //         t = checkContentType(t_buf);
    //         if (t == NULL)
    //             t = "text/plain";
    //     }
    else if (DefaultType) {
        t = DefaultType;
        DefaultType = NULL;
    } else {
        t = guessContentType(pu.file);
        if (t == NULL)
            t = "text/plain";
        real_type = t;
        if (c.f.guess_type) {
            t = c.f.guess_type;
        }
    }

    /* XXX: can we use guess_type to give the type to loadHTMLstream
     *      to support default utf8 encoding for XHTML here? */
    c.f.guess_type = t;

    term_raw();
    return page_loaded(pu, c.f, page, charset, t, NULL, do_download);
}

#define TAG_IS(s, tag, len) \
    (strncasecmp(s, tag, len) == 0 && (s[len] == '>' || IS_SPACE((int)s[len])))

static InputStream _file_lp2;

static void
proc_escape(struct readbuffer* obuf, char** str_return)
{
    char *str = *str_return, *estr;
    int ech = getescapechar(str_return);
    int width, n_add = *str_return - str;
    Lineprop mode = PC_ASCII;

    if (ech < 0) {
        *str_return = str;
        proc_mchar(obuf, obuf->flag & RB_SPECIAL, 1, str_return, PC_ASCII);
        return;
    }
    mode = IS_CNTRL(ech) ? PC_CTRL : PC_ASCII;

    estr = conv_entity(ech);
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

#ifndef min
#define min(a, b) ((a) > (b) ? (b) : (a))
#endif /* not min */

/* HTML processing first pass */
void HTMLlineproc0(char* line, struct html_feed_environ* h_env, bool internal)
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

#ifdef DEBUG
    if (w3m_debug) {
        FILE* f = fopen("zzzproc1", "a");
        fprintf(f, "%c%c%c%c",
            (obuf->flag & RB_PREMODE) ? 'P' : ' ',
            (obuf->table_level >= 0) ? 'T' : ' ',
            (obuf->flag & RB_INTXTA) ? 'X' : ' ',
            (obuf->flag & (RB_SCRIPT | RB_STYLE)) ? 'S' : ' ');
        fprintf(f, "HTMLlineproc1(\"%s\",%d,%lx)\n", line, h_env->limit,
            (unsigned long)h_env);
        fclose(f);
    }
#endif

    tokbuf = Strnew();

table_start:
    if (obuf->table_level >= 0) {
        int level = min(obuf->table_level, MAX_TABLE - 1);
        tbl = tables[level];
        tbl_mode = &table_mode[level];
        tbl_width = table_width(h_env, level);
    }

    while (*line != '\0') {
        char *str, *p;
        int is_tag = false;
        int pre_mode = (obuf->table_level >= 0 && tbl_mode) ? tbl_mode->pre_mode : obuf->flag;
        int end_tag = (obuf->table_level >= 0 && tbl_mode) ? tbl_mode->end_tag : obuf->end_tag;

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
                p = str;
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
                    char* p = str;
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
                    char* p = html_quote_char(*str);
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

/*
 * loadHTMLBuffer: read file and make new buffer
 */
Buffer*
loadHTMLBuffer(struct URLFile* f, Buffer* newBuf)
{
    FILE* src = NULL;
    Str tmp;

    if (newBuf == NULL)
        newBuf = newBuffer();
    if (newBuf->sourcefile == NULL && (f->scheme != SCM_LOCAL || newBuf->mailcap)) {
        tmp = tmpfname(TMPF_SRC, ".html");
        src = fopen(tmp->ptr, "w");
        if (src)
            newBuf->sourcefile = tmp->ptr;
    }

    loadHTMLstream(f, newBuf, src, false);

    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;
    if (n_textarea)
        formResetBuffer(newBuf, newBuf->formitem);
    if (src)
        fclose(src);

    return newBuf;
}

void loadHTMLstream(struct URLFile* f, Buffer* newBuf, FILE* src, int internal)
{
    Str html = Strnew();
    Str lineBuf2;

    // wc_ces doc_charset = DocumentCharset;
    // wc_ces charset = WC_CES_US_ASCII;
    // if (newBuf != NULL) {
    //     if (newBuf->document_charset)
    //         charset = /*doc_charset =*/newBuf->document_charset;
    // }
    // if (content_charset && UseContentCharset)
    //     doc_charset = content_charset;
    // else if (f->guess_type && !strcasecmp(f->guess_type, "application/xhtml+xml"))
    //     doc_charset = WC_CES_UTF_8;
    // meta_charset = 0;
    // cur_document_charset = charset;

    while ((lineBuf2 = StrmyUFgets(f)) && lineBuf2->length) {
        Strcat(html, lineBuf2);
    }

    struct UI ui = getUI();
    loadHTML(html, WC_CES_SHIFT_JIS /*WC_CES_US_ASCII*/, ui.vt->COLS, ui.use_graphic, internal, newBuf);
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
}

/*
 * loadHTMLString: read string and make new buffer
 */
Buffer*
loadHTMLString(Str page)
{
    struct URLFile f;
    MySignalHandler (*prevtrap)(int _dummy) = NULL;
    Buffer* newBuf;

    init_stream(&f, SCM_LOCAL, newStrStream(page));

    newBuf = newBuffer();
    // if (sigsetjmp(AbortLoading, 1) != 0) {
    //     term_raw();
    //     discardBuffer(newBuf);
    //     UFclose(&f);
    //     return NULL;
    // }
    // TRAP_ON;

    newBuf->document_charset = InnerCharset;
    loadHTMLstream(&f, newBuf, NULL, true);
    newBuf->document_charset = WC_CES_US_ASCII;

    term_raw();
    UFclose(&f);
    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;
    newBuf->type = "text/html";
    newBuf->real_type = newBuf->type;
    if (n_textarea)
        formResetBuffer(newBuf, newBuf->formitem);
    return newBuf;
}

/*
 * loadBuffer: read file and make new buffer
 */
Buffer*
loadBuffer(struct URLFile* uf, Buffer* newBuf)
{
    FILE* src = NULL;
    wc_ces charset = WC_CES_US_ASCII;
    wc_ces doc_charset = DocumentCharset;
    Str lineBuf2;
    char pre_lbuf = '\0';
    int nlines;
    Str tmpf;
    long long linelen = 0, trbyte = 0;
    Lineprop* propBuffer = NULL;
    Linecolor* colorBuffer = NULL;
    MySignalHandler (*prevtrap)(int _dummy) = NULL;

    if (newBuf == NULL)
        newBuf = newBuffer();

    // if (sigsetjmp(AbortLoading, 1) != 0) {
    //     goto _end;
    // }
    // TRAP_ON;

    if (newBuf->sourcefile == NULL && (uf->scheme != SCM_LOCAL || newBuf->mailcap)) {
        tmpf = tmpfname(TMPF_SRC, NULL);
        src = fopen(tmpf->ptr, "w");
        if (src)
            newBuf->sourcefile = tmpf->ptr;
    }
    if (newBuf->document_charset)
        charset = doc_charset = newBuf->document_charset;
    if (content_charset && UseContentCharset)
        doc_charset = content_charset;

    nlines = 0;
    if (IStype(uf->stream) != IST_ENCODED)
        uf->stream = newEncodedStream(uf->stream, uf->encoding);
    while ((lineBuf2 = StrmyISgets(uf->stream)) && lineBuf2->length) {
        if (src)
            Strfputs(lineBuf2, src);
        linelen += lineBuf2->length;
        showProgress(current_content_length, &linelen, &trbyte);
        lineBuf2 = convertLine(uf, lineBuf2, HEADER_MODE, &charset, doc_charset);
        if (squeezeBlankLine) {
            if (lineBuf2->ptr[0] == '\n' && pre_lbuf == '\n') {
                ++nlines;
                continue;
            }
            pre_lbuf = lineBuf2->ptr[0];
        }
        ++nlines;
        Strchop(lineBuf2);
        lineBuf2 = checkType(lineBuf2, &propBuffer, NULL);
        addnewline(newBuf, lineBuf2->ptr, propBuffer, colorBuffer,
            lineBuf2->length, -1, nlines);
    }
_end:
    term_raw();
    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;
    newBuf->trbyte = trbyte + linelen;
    newBuf->document_charset = charset;
    if (src)
        fclose(src);

    return newBuf;
}
