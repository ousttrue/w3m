#include "display.h"
#include "buffer.h"
#include "anchor.h"
#include "maparea.h"
#include "tab.h"
#include "image.h"
#include "w3m_rc.h"
#include "ctrlcode.h"

#include <math.h>
#include "fm.h"

/* *INDENT-OFF* */
#ifdef USE_COLOR

#define EFFECT_ANCHOR_START effect_anchor_start()
#define EFFECT_ANCHOR_END effect_anchor_end()
#define EFFECT_IMAGE_START effect_image_start()
#define EFFECT_IMAGE_END effect_image_end()
#define EFFECT_FORM_START effect_form_start()
#define EFFECT_FORM_END effect_form_end()
#define EFFECT_ACTIVE_START effect_active_start()
#define EFFECT_ACTIVE_END effect_active_end()
#define EFFECT_VISITED_START effect_visited_start()
#define EFFECT_VISITED_END effect_visited_end()
#define EFFECT_MARK_START effect_mark_start()
#define EFFECT_MARK_END effect_mark_end()

/*-
 * color:
 *     0  black
 *     1  red
 *     2  green
 *     3  yellow
 *     4  blue
 *     5  magenta
 *     6  cyan
 *     7  white
 */

#define EFFECT_ANCHOR_START_C screen_setfcolor(anchor_color)
#define EFFECT_IMAGE_START_C screen_setfcolor(image_color)
#define EFFECT_FORM_START_C screen_setfcolor(form_color)
#define EFFECT_ACTIVE_START_C (screen_setfcolor(active_color), screen_underline())
#define EFFECT_VISITED_START_C screen_setfcolor(visited_color)
#define EFFECT_MARK_START_C screen_setbcolor(mark_color)

#define EFFECT_IMAGE_END_C screen_setfcolor(basic_color)
#define EFFECT_ANCHOR_END_C screen_setfcolor(basic_color)
#define EFFECT_FORM_END_C screen_setfcolor(basic_color)
#define EFFECT_ACTIVE_END_C (screen_setfcolor(basic_color), screen_underlineend())
#define EFFECT_VISITED_END_C screen_setfcolor(basic_color)
#define EFFECT_MARK_END_C screen_setbcolor(bg_color)

#define EFFECT_ANCHOR_START_M screen_underline()
#define EFFECT_ANCHOR_END_M screen_underlineend()
#define EFFECT_IMAGE_START_M screen_standout()
#define EFFECT_IMAGE_END_M screen_standend()
#define EFFECT_FORM_START_M screen_standout()
#define EFFECT_FORM_END_M screen_standend()
#define EFFECT_ACTIVE_START_NC screen_underline()
#define EFFECT_ACTIVE_END_NC screen_underlineend()
#define EFFECT_ACTIVE_START_M screen_bold()
#define EFFECT_ACTIVE_END_M screen_boldend()
#define EFFECT_VISITED_START_M /**/
#define EFFECT_VISITED_END_M /**/
#define EFFECT_MARK_START_M screen_standout()
#define EFFECT_MARK_END_M screen_standend()
#define define_effect(name_start, name_end, color_start, color_end, mono_start, mono_end) \
    static void name_start                                                                \
    {                                                                                     \
        if (useColor) {                                                                   \
            color_start;                                                                  \
        } else {                                                                          \
            mono_start;                                                                   \
        }                                                                                 \
    }                                                                                     \
    static void name_end                                                                  \
    {                                                                                     \
        if (useColor) {                                                                   \
            color_end;                                                                    \
        } else {                                                                          \
            mono_end;                                                                     \
        }                                                                                 \
    }

define_effect(EFFECT_ANCHOR_START, EFFECT_ANCHOR_END, EFFECT_ANCHOR_START_C,
    EFFECT_ANCHOR_END_C, EFFECT_ANCHOR_START_M, EFFECT_ANCHOR_END_M)
    define_effect(EFFECT_IMAGE_START, EFFECT_IMAGE_END, EFFECT_IMAGE_START_C,
        EFFECT_IMAGE_END_C, EFFECT_IMAGE_START_M, EFFECT_IMAGE_END_M)
        define_effect(EFFECT_FORM_START, EFFECT_FORM_END, EFFECT_FORM_START_C,
            EFFECT_FORM_END_C, EFFECT_FORM_START_M, EFFECT_FORM_END_M)
            define_effect(EFFECT_MARK_START, EFFECT_MARK_END, EFFECT_MARK_START_C,
                EFFECT_MARK_END_C, EFFECT_MARK_START_M, EFFECT_MARK_END_M)

    /*****************/
    static void EFFECT_ACTIVE_START
{
    if (useColor) {
        if (useActiveColor) {
#ifdef __EMX__
            if (!getenv("WINDOWID"))
                setfcolor(active_color);
            else
#endif
            {
                EFFECT_ACTIVE_START_C;
            }
        } else {
            EFFECT_ACTIVE_START_NC;
        }
    } else {
        EFFECT_ACTIVE_START_M;
    }
}

static void EFFECT_ACTIVE_END
{
    if (useColor) {
        if (useActiveColor) {
            EFFECT_ACTIVE_END_C;
        } else {
            EFFECT_ACTIVE_END_NC;
        }
    } else {
        EFFECT_ACTIVE_END_M;
    }
}

static void EFFECT_VISITED_START
{
    if (useVisitedColor) {
        if (useColor) {
            EFFECT_VISITED_START_C;
        } else {
            EFFECT_VISITED_START_M;
        }
    }
}

static void EFFECT_VISITED_END
{
    if (useVisitedColor) {
        if (useColor) {
            EFFECT_VISITED_END_C;
        } else {
            EFFECT_VISITED_END_M;
        }
    }
}

#else /* not USE_COLOR */

#define EFFECT_ANCHOR_START underline()
#define EFFECT_ANCHOR_END underlineend()
#define EFFECT_IMAGE_START standout()
#define EFFECT_IMAGE_END standend()
#define EFFECT_FORM_START standout()
#define EFFECT_FORM_END standend()
#define EFFECT_ACTIVE_START bold()
#define EFFECT_ACTIVE_END boldend()
#define EFFECT_VISITED_START /**/
#define EFFECT_VISITED_END /**/
#define EFFECT_MARK_START standout()
#define EFFECT_MARK_END standend()
#endif /* not USE_COLOR */
/* *INDENT-ON* */

/*
 * Display some lines.
 */
static struct Line* cline = NULL;
static int ccolumn = -1;

static int ulmode = 0, somode = 0, bomode = 0;
static int anch_mode = 0, emph_mode = 0, imag_mode = 0, form_mode = 0,
           active_mode = 0, visited_mode = 0, mark_mode = 0, graph_mode = 0;
static Linecolor color_mode = 0;

#ifdef USE_BUFINFO
static struct Buffer* save_current_buf = NULL;
#endif

static char* delayed_msg = NULL;

static void drawAnchorCursor(struct Buffer* buf);
#define redrawBuffer(buf) redrawNLine(buf, LASTLINE())
static void redrawNLine(struct Buffer* buf, int n);
static struct Line* redrawLine(struct Buffer* buf, struct Line* l, int i);

static int image_touch = 0;
static int draw_image_flag = FALSE;
static struct Line* redrawLineImage(struct Buffer* buf, struct Line* l, int i);

static int redrawLineRegion(struct Buffer* buf, struct Line* l, int i, int bpos, int epos);
static void do_effects(Lineprop m);

static void do_color(Linecolor c);

static Str
make_lastline_link(struct Buffer* buf, char* title, char* url)
{
    Str s = NULL, u;
    Lineprop* pr;
    struct Url pu;
    char* p;
    int l = TTY_COLS() - 1, i;

    if (title && *title) {
        s = Strnew_m_charp("[", title, "]", NULL);
        for (p = s->ptr; *p; p++) {
            if (IS_CNTRL(*p) || IS_SPACE(*p))
                *p = ' ';
        }
        if (url)
            Strcat_charp(s, " ");
        l -= get_Str_strwidth(s);
        if (l <= 0)
            return s;
    }
    if (!url)
        return s;
    parseURL2(url, &pu, baseURL(buf));
    u = parsedURL2Str(&pu);
    if (DecodeURL)
        u = Strnew_charp(url_decode2(u->ptr, buf));
    u = checkType(u, &pr, NULL);
    if (l <= 4 || l >= get_Str_strwidth(u)) {
        if (!s)
            return u;
        Strcat(s, u);
        return s;
    }
    if (!s)
        s = Strnew_size(TTY_COLS());
    i = (l - 2) / 2;
    while (i && pr[i] & PC_WCHAR2)
        i--;
    Strcat_charp_n(s, u->ptr, i);
    Strcat_charp(s, "..");
    i = get_Str_strwidth(u) - (TTY_COLS() - 1 - get_Str_strwidth(s));
    while (i < u->length && pr[i] & PC_WCHAR2)
        i++;
    Strcat_charp(s, &u->ptr[i]);
    return s;
}

static Str
make_lastline_message(struct Buffer* buf)
{
    Str msg, s = NULL;
    int sl = 0;

    if (displayLink) {
        struct MapArea* a = retrieveCurrentMapArea(buf);
        if (a)
            s = make_lastline_link(buf, a->alt, a->url);
        else {
            struct Anchor* a = retrieveCurrentAnchor(buf);
            char* p = NULL;
            if (a && a->title && *a->title)
                p = a->title;
            else {
                struct Anchor* a_img = retrieveCurrentImg(buf);
                if (a_img && a_img->title && *a_img->title)
                    p = a_img->title;
            }
            if (p || a)
                s = make_lastline_link(buf, p, a ? a->url : NULL);
        }
        if (s) {
            sl = get_Str_strwidth(s);
            if (sl >= TTY_COLS() - 3)
                return s;
        }
    }

    msg = Strnew();
    if (displayLineInfo && buf->currentLine != NULL && buf->lastLine != NULL) {
        int cl = buf->currentLine->real_linenumber;
        int ll = buf->lastLine->real_linenumber;
        int r = (int)((double)cl * 100.0 / (double)(ll ? ll : 1) + 0.5);
        Strcat(msg, Sprintf("%d/%d (%d%%)", cl, ll, r));
    } else {
        msg = Sprintf("%s", msg->ptr);
    }
    Strcat_charp(msg, "Viewing");
    if (buf->ssl_certificate)
        Strcat_charp(msg, "[SSL]");
    Strcat_charp(msg, " <");
    Strcat_charp(msg, buf->buffername);

    if (s) {
        int l = TTY_COLS() - 3 - sl;
        if (get_Str_strwidth(msg) > l) {

            const char* p;
            for (p = msg->ptr; *p; p += get_mclen(p)) {
                l -= get_mcwidth(p);
                if (l < 0)
                    break;
            }
            l = p - msg->ptr;

            Strtruncate(msg, l);
        }
        Strcat_charp(msg, "> ");
        Strcat(msg, s);
    } else {
        Strcat_charp(msg, ">");
    }
    return msg;
}

void displayBuffer(struct Buffer* buf, enum DisplayMode mode)
{
    if (!buf) {
        return;
    }

    if (buf->topLine == NULL && readBufferCache(buf)) {
        mode = B_FORCE_REDRAW;
    }

    if (buf->width == 0)
        buf->width = INIT_BUFFER_WIDTH;
    if (buf->height == 0)
        buf->height = LASTLINE() + 1;

    // reshape
    if ((
            buf->width != INIT_BUFFER_WIDTH //
            && (is_html_type(buf->type) || getRuntime()->FoldLine) //
            )
        || buf->need_reshape) {
        buf->need_reshape = TRUE;
        reshapeBuffer(buf);
    }

    // rootX
    if (getRuntime()->showLineNum) {
        if (buf->lastLine && buf->lastLine->real_linenumber > 0)
            buf->rootX = (int)(log(buf->lastLine->real_linenumber + 0.1)
                             / log(10))
                + 2;
        if (buf->rootX < 5)
            buf->rootX = 5;
        if (buf->rootX > TTY_COLS())
            buf->rootX = TTY_COLS();
    } else {
        buf->rootX = 0;
    }
    buf->COLS = TTY_COLS() - buf->rootX;

    // rootY
    int ny = 0;
    if (nTab() > 1) {
        if (mode == B_FORCE_REDRAW || mode == B_REDRAW_IMAGE)
            calcTabPos();
        ny = LastTab()->y + 2;
        if (ny > LASTLINE())
            ny = LASTLINE();
    }
    if (buf->rootY != ny || buf->LINES != LASTLINE() - ny) {
        buf->rootY = ny;
        buf->LINES = LASTLINE() - ny;
        arrangeCursor(buf);
        mode = B_REDRAW_IMAGE;
    }

    // check viewport ?
    if (mode == B_FORCE_REDRAW //
        || mode == B_SCROLL //
        || mode == B_REDRAW_IMAGE //
        || cline != buf->topLine //
        || ccolumn != buf->currentColumn) {

        if (activeImage && (mode == B_REDRAW_IMAGE || cline != buf->topLine || ccolumn != buf->currentColumn)) {
            if (draw_image_flag) {
                tty_clear();
                screen_clear();
            }
            clearImage();
            loadImage(buf, IMG_FLAG_STOP);
            image_touch++;
            draw_image_flag = FALSE;
        }
        redrawBuffer(buf);

        cline = buf->topLine;
        ccolumn = buf->currentColumn;
    }

    if (buf->topLine == NULL)
        buf->topLine = buf->firstLine;

    if (buf->need_reshape) {
        displayBuffer(buf, B_FORCE_REDRAW);
        return;
    }

    drawAnchorCursor(buf);

    Str msg = make_lastline_message(buf);
    if (buf->firstLine == NULL) {
        /* FIXME: gettextize? */
        Strcat_charp(msg, "\tNo Line");
    }
    if (delayed_msg != NULL) {
        disp_message(delayed_msg, FALSE);
        delayed_msg = NULL;
        tty_refresh();
    }
    screen_standout();
    message(msg->ptr, buf->cursorX + buf->rootX, buf->cursorY + buf->rootY);
    screen_standend();
    term_title(conv_to_system(buf->buffername));
    tty_refresh();

    if (activeImage && displayImage && buf->img) {
        if (buf->image_loaded) {
            drawImage(buf);
        }
    }

#ifdef USE_BUFINFO
    if (buf != save_current_buf) {
        saveBufferInfo();
        save_current_buf = buf;
    }
#endif
}

static void
drawAnchorCursor0(struct Buffer* buf, struct AnchorList* al, int hseq, int prevhseq,
    int tline, int eline, int active)
{
    int i, j;
    struct Line* l;
    struct Anchor* an;

    l = buf->topLine;
    for (j = 0; j < al->nanchor; j++) {
        an = &al->anchors[j];
        if (an->start.line < tline)
            continue;
        if (an->start.line >= eline)
            return;
        for (;; l = l->next) {
            if (l == NULL)
                return;
            if (l->linenumber == an->start.line)
                break;
        }
        if (hseq >= 0 && an->hseq == hseq) {
            int start_pos = an->start.pos;
            int end_pos = an->end.pos;
            for (i = an->start.pos; i < an->end.pos; i++) {
                if (enable_inline_image && (l->propBuf[i] & PE_IMAGE)) {
                    if (start_pos == i)
                        start_pos = i + 1;
                    else if (end_pos == an->end.pos)
                        end_pos = i - 1;
                }
                if (l->propBuf[i] & (PE_IMAGE | PE_ANCHOR | PE_FORM)) {
                    if (active)
                        l->propBuf[i] |= PE_ACTIVE;
                    else
                        l->propBuf[i] &= ~PE_ACTIVE;
                }
            }
            if (active && start_pos < end_pos)
                redrawLineRegion(buf, l, l->linenumber - tline + buf->rootY,
                    start_pos, end_pos);
        } else if (prevhseq >= 0 && an->hseq == prevhseq) {
            if (active)
                redrawLineRegion(buf, l, l->linenumber - tline + buf->rootY,
                    an->start.pos, an->end.pos);
        }
    }
}

static void
drawAnchorCursor(struct Buffer* buf)
{
    struct Anchor* an;
    int hseq, prevhseq;
    int tline, eline;

    if (!buf->firstLine || !buf->hmarklist)
        return;
    if (!buf->href && !buf->formitem)
        return;

    an = retrieveCurrentAnchor(buf);
    if (!an)
        an = retrieveCurrentMap(buf);
    if (an)
        hseq = an->hseq;
    else
        hseq = -1;
    tline = buf->topLine->linenumber;
    eline = tline + buf->LINES;
    prevhseq = buf->hmarklist->prevhseq;

    if (buf->href) {
        drawAnchorCursor0(buf, buf->href, hseq, prevhseq, tline, eline, 1);
        drawAnchorCursor0(buf, buf->href, hseq, -1, tline, eline, 0);
    }
    if (buf->formitem) {
        drawAnchorCursor0(buf, buf->formitem, hseq, prevhseq, tline, eline, 1);
        drawAnchorCursor0(buf, buf->formitem, hseq, -1, tline, eline, 0);
    }
    buf->hmarklist->prevhseq = hseq;
}

static void
redrawNLine(struct Buffer* buf, int n)
{
    struct Line* l;
    int i;

    if (useColor) {
        EFFECT_ANCHOR_END_C;
        screen_setbcolor(bg_color);
    }

    if (nTab() > 1) {
        screen_move(0, 0);
        screen_clrtoeolx();
        for (struct TabBuffer* t = FirstTab(); t; t = t->nextTab) {
            screen_move(t->y, t->x1);
            if (t == CurrentTab())
                screen_bold();
            screen_addch('[', 1);
            int l = t->x2 - t->x1 - 1 - get_strwidth(t->currentBuffer->buffername);
            if (l < 0)
                l = 0;
            if (l / 2 > 0)
                screen_wc_addnstr_sup(" ", l / 2);
            if (t == CurrentTab())
                EFFECT_ACTIVE_START;
            screen_wc_addstr_width(t->currentBuffer->buffername, t->x2 - t->x1 - l);
            if (t == CurrentTab())
                EFFECT_ACTIVE_END;
            if ((l + 1) / 2 > 0)
                screen_wc_addnstr_sup(" ", (l + 1) / 2);
            screen_move(t->y, t->x2);
            screen_addch(']', 1);
            if (t == CurrentTab())
                screen_boldend();
        }
        screen_move(LastTab()->y + 1, 0);
        for (i = 0; i < TTY_COLS(); i++)
            screen_addch('~', 1);
    }
    for (i = 0, l = buf->topLine; i < buf->LINES; i++, l = l->next) {
        if (i >= buf->LINES - n || i < -n)
            l = redrawLine(buf, l, i + buf->rootY);
        if (l == NULL)
            break;
    }
    if (n > 0) {
        screen_move(i + buf->rootY, 0);
        screen_clrtobotx();
    }

    if (!(activeImage && displayImage && buf->img))
        return;
    screen_move(buf->cursorY + buf->rootY, buf->cursorX + buf->rootX);
    for (i = 0, l = buf->topLine; i < buf->LINES && l; i++, l = l->next) {
        if (i >= buf->LINES - n || i < -n)
            redrawLineImage(buf, l, i + buf->rootY);
    }
    getAllImage(buf);
}

static struct Line*
redrawLine(struct Buffer* buf, struct Line* l, int i)
{
    int j, pos, rcol, ncol, delta = 1;
    int column = buf->currentColumn;
    char* p;
    Lineprop* pr;
#ifdef USE_ANSI_COLOR
    Linecolor* pc;
#endif
#ifdef USE_COLOR
    struct Anchor* a;
    struct Url url;
    int k, vpos = -1;
#endif

    if (l == NULL) {
        if (buf->pagerSource) {
            l = getNextPage(buf, buf->LINES + buf->rootY - i);
            if (l == NULL)
                return NULL;
        } else
            return NULL;
    }
    screen_move(i, 0);
    if (getRuntime()->showLineNum) {
        char tmp[16];
        if (!buf->rootX) {
            if (buf->lastLine->real_linenumber > 0)
                buf->rootX = (int)(log(buf->lastLine->real_linenumber + 0.1)
                                 / log(10))
                    + 2;
            if (buf->rootX < 5)
                buf->rootX = 5;
            if (buf->rootX > TTY_COLS())
                buf->rootX = TTY_COLS();
            buf->COLS = TTY_COLS() - buf->rootX;
        }
        if (l->real_linenumber && !l->bpos)
            sprintf(tmp, "%*ld:", buf->rootX - 1, l->real_linenumber);
        else
            sprintf(tmp, "%*s ", buf->rootX - 1, "");
        screen_wc_addstr(tmp);
    }
    screen_move(i, buf->rootX);
    if (l->width < 0)
        l->width = COLPOS(l, l->len);
    if (l->len == 0 || l->width - 1 < column) {
        screen_clrtoeolx();
        return l;
    }
    /* need_clrtoeol(); */
    pos = columnPos(l, column);
    p = &(l->lineBuf[pos]);
    pr = &(l->propBuf[pos]);
#ifdef USE_ANSI_COLOR
    if (useColor && l->colorBuf)
        pc = &(l->colorBuf[pos]);
    else
        pc = NULL;
#endif
    rcol = COLPOS(l, pos);

    for (j = 0; rcol - column < buf->COLS && pos + j < l->len; j += delta) {
#ifdef USE_COLOR
        if (useVisitedColor && vpos <= pos + j && !(pr[j] & PE_VISITED)) {
            a = retrieveAnchor(buf->href, l->linenumber, pos + j);
            if (a) {
                parseURL2(a->url, &url, baseURL(buf));
                if (getHashHist(getRuntime()->URLHist, parsedURL2Str(&url)->ptr)) {
                    for (k = a->start.pos; k < a->end.pos; k++)
                        pr[k - pos] |= PE_VISITED;
                }
                vpos = a->end.pos;
            }
        }
#endif
#ifdef USE_M17N
        delta = wtf_len((wc_uchar*)&p[j]);
#endif
        ncol = COLPOS(l, pos + j + delta);
        if (ncol - column > buf->COLS)
            break;
#ifdef USE_ANSI_COLOR
        if (pc)
            do_color(pc[j]);
#endif
        if (rcol < column) {
            for (rcol = column; rcol < ncol; rcol++)
                addChar(' ', 0);
            continue;
        }
        if (p[j] == '\t') {
            for (; rcol < ncol; rcol++)
                addChar(' ', 0);
        } else {
#ifdef USE_M17N
            addMChar(&p[j], pr[j], delta);
#else
            addChar(p[j], pr[j]);
#endif
        }
        rcol = ncol;
    }
    if (somode) {
        somode = FALSE;
        screen_standend();
    }
    if (ulmode) {
        ulmode = FALSE;
        screen_underlineend();
    }
    if (bomode) {
        bomode = FALSE;
        screen_boldend();
    }
    if (emph_mode) {
        emph_mode = FALSE;
        screen_boldend();
    }

    if (anch_mode) {
        anch_mode = FALSE;
        EFFECT_ANCHOR_END;
    }
    if (imag_mode) {
        imag_mode = FALSE;
        EFFECT_IMAGE_END;
    }
    if (form_mode) {
        form_mode = FALSE;
        EFFECT_FORM_END;
    }
    if (visited_mode) {
        visited_mode = FALSE;
        EFFECT_VISITED_END;
    }
    if (active_mode) {
        active_mode = FALSE;
        EFFECT_ACTIVE_END;
    }
    if (mark_mode) {
        mark_mode = FALSE;
        EFFECT_MARK_END;
    }
    if (graph_mode) {
        graph_mode = FALSE;
        screen_graphend();
    }
#ifdef USE_ANSI_COLOR
    if (color_mode)
        do_color(0);
#endif
    if (rcol - column < buf->COLS)
        screen_clrtoeolx();
    return l;
}

#ifdef USE_IMAGE
static struct Line*
redrawLineImage(struct Buffer* buf, struct Line* l, int i)
{
    int j, pos, rcol;
    int column = buf->currentColumn;
    struct Anchor* a;
    int x, y, sx, sy, w, h;

    if (l == NULL)
        return NULL;
    if (l->width < 0)
        l->width = COLPOS(l, l->len);
    if (l->len == 0 || l->width - 1 < column)
        return l;
    pos = columnPos(l, column);
    rcol = COLPOS(l, pos);
    for (j = 0; rcol - column < buf->COLS && pos + j < l->len; j++) {
        if (rcol - column < 0) {
            rcol = COLPOS(l, pos + j + 1);
            continue;
        }
        a = retrieveAnchor(buf->img, l->linenumber, pos + j);
        if (a && a->image && a->image->touch < image_touch) {
            struct Image* image = a->image;
            struct ImageCache* cache;

            cache = image->cache = getImage(image, baseURL(buf),
                buf->image_flag);
            if (cache) {
                if ((image->width < 0 && cache->width > 0) || (image->height < 0 && cache->height > 0)) {
                    image->width = cache->width;
                    image->height = cache->height;
                    buf->need_reshape = TRUE;
                }
                x = (int)((rcol - column + buf->rootX) * pixel_per_char);
                y = (int)(i * pixel_per_line);
                sx = (int)((rcol - COLPOS(l, a->start.pos)) * pixel_per_char);
                sy = (int)((l->linenumber - image->y) * pixel_per_line);
                if (!enable_inline_image) {
                    if (sx == 0 && x + image->xoffset >= 0)
                        x += image->xoffset;
                    else
                        sx -= image->xoffset;
                    if (sy == 0 && y + image->yoffset >= 0)
                        y += image->yoffset;
                    else
                        sy -= image->yoffset;
                }
                if (image->width > 0)
                    w = image->width - sx;
                else
                    w = (int)(8 * pixel_per_char - sx);
                if (image->height > 0)
                    h = image->height - sy;
                else
                    h = (int)(pixel_per_line - sy);
                if (w > (int)((buf->rootX + buf->COLS) * pixel_per_char - x))
                    w = (int)((buf->rootX + buf->COLS) * pixel_per_char - x);
                if (h > (int)(LASTLINE() * pixel_per_line - y))
                    h = (int)(LASTLINE() * pixel_per_line - y);
                addImage(cache, x, y, sx, sy, w, h);
                image->touch = image_touch;
                draw_image_flag = TRUE;
            }
        }
        rcol = COLPOS(l, pos + j + 1);
    }
    return l;
}
#endif

static int
redrawLineRegion(struct Buffer* buf, struct Line* l, int i, int bpos, int epos)
{
    int j, pos, rcol, ncol, delta = 1;
    int column = buf->currentColumn;
    char* p;
    Lineprop* pr;
#ifdef USE_ANSI_COLOR
    Linecolor* pc;
#endif
    int bcol, ecol;
#ifdef USE_COLOR
    struct Anchor* a;
    struct Url url;
    int k, vpos = -1;
#endif

    if (l == NULL)
        return 0;
    pos = columnPos(l, column);
    p = &(l->lineBuf[pos]);
    pr = &(l->propBuf[pos]);
#ifdef USE_ANSI_COLOR
    if (useColor && l->colorBuf)
        pc = &(l->colorBuf[pos]);
    else
        pc = NULL;
#endif
    rcol = COLPOS(l, pos);
    bcol = bpos - pos;
    ecol = epos - pos;

    for (j = 0; rcol - column < buf->COLS && pos + j < l->len; j += delta) {
#ifdef USE_COLOR
        if (useVisitedColor && vpos <= pos + j && !(pr[j] & PE_VISITED)) {
            a = retrieveAnchor(buf->href, l->linenumber, pos + j);
            if (a) {
                parseURL2(a->url, &url, baseURL(buf));
                if (getHashHist(getRuntime()->URLHist, parsedURL2Str(&url)->ptr)) {
                    for (k = a->start.pos; k < a->end.pos; k++)
                        pr[k - pos] |= PE_VISITED;
                }
                vpos = a->end.pos;
            }
        }
#endif
#ifdef USE_M17N
        delta = wtf_len((wc_uchar*)&p[j]);
#endif
        ncol = COLPOS(l, pos + j + delta);
        if (ncol - column > buf->COLS)
            break;
#ifdef USE_ANSI_COLOR
        if (pc)
            do_color(pc[j]);
#endif
        if (j >= bcol && j < ecol) {
            if (rcol < column) {
                screen_move(i, buf->rootX);
                for (rcol = column; rcol < ncol; rcol++)
                    addChar(' ', 0);
                continue;
            }
            screen_move(i, rcol - column + buf->rootX);
            if (p[j] == '\t') {
                for (; rcol < ncol; rcol++)
                    addChar(' ', 0);
            } else
#ifdef USE_M17N
                addMChar(&p[j], pr[j], delta);
#else
                addChar(p[j], pr[j]);
#endif
        }
        rcol = ncol;
    }
    if (somode) {
        somode = FALSE;
        screen_standend();
    }
    if (ulmode) {
        ulmode = FALSE;
        screen_underlineend();
    }
    if (bomode) {
        bomode = FALSE;
        screen_boldend();
    }
    if (emph_mode) {
        emph_mode = FALSE;
        screen_boldend();
    }

    if (anch_mode) {
        anch_mode = FALSE;
        EFFECT_ANCHOR_END;
    }
    if (imag_mode) {
        imag_mode = FALSE;
        EFFECT_IMAGE_END;
    }
    if (form_mode) {
        form_mode = FALSE;
        EFFECT_FORM_END;
    }
    if (visited_mode) {
        visited_mode = FALSE;
        EFFECT_VISITED_END;
    }
    if (active_mode) {
        active_mode = FALSE;
        EFFECT_ACTIVE_END;
    }
    if (mark_mode) {
        mark_mode = FALSE;
        EFFECT_MARK_END;
    }
    if (graph_mode) {
        graph_mode = FALSE;
        screen_graphend();
    }
    if (color_mode)
        do_color(0);
    return rcol - column;
}

#define do_effect1(effect, modeflag, action_start, action_end) \
    if (m & effect) {                                          \
        if (!modeflag) {                                       \
            action_start;                                      \
            modeflag = TRUE;                                   \
        }                                                      \
    }

#define do_effect2(effect, modeflag, action_start, action_end) \
    if (modeflag) {                                            \
        action_end;                                            \
        modeflag = FALSE;                                      \
    }

static void
do_effects(Lineprop m)
{
    /* effect end */
    do_effect2(PE_UNDER, ulmode, screen_underline(), screen_underlineend());
    do_effect2(PE_STAND, somode, screen_standout(), screen_standend());
    do_effect2(PE_BOLD, bomode, screen_bold(), screen_boldend());
    do_effect2(PE_EMPH, emph_mode, screen_bold(), screen_boldend());
    do_effect2(PE_ANCHOR, anch_mode, EFFECT_ANCHOR_START, EFFECT_ANCHOR_END);
    do_effect2(PE_IMAGE, imag_mode, EFFECT_IMAGE_START, EFFECT_IMAGE_END);
    do_effect2(PE_FORM, form_mode, EFFECT_FORM_START, EFFECT_FORM_END);
    do_effect2(PE_VISITED, visited_mode, EFFECT_VISITED_START,
        EFFECT_VISITED_END);
    do_effect2(PE_ACTIVE, active_mode, EFFECT_ACTIVE_START, EFFECT_ACTIVE_END);
    do_effect2(PE_MARK, mark_mode, EFFECT_MARK_START, EFFECT_MARK_END);
    if (graph_mode) {
        screen_graphend();
        graph_mode = FALSE;
    }

    /* effect start */
    do_effect1(PE_UNDER, ulmode, screen_underline(), screen_underlineend());
    do_effect1(PE_STAND, somode, screen_standout(), screen_standend());
    do_effect1(PE_BOLD, bomode, screen_bold(), screen_boldend());
    do_effect1(PE_EMPH, emph_mode, screen_bold(), screen_boldend());
    do_effect1(PE_ANCHOR, anch_mode, EFFECT_ANCHOR_START, EFFECT_ANCHOR_END);
    do_effect1(PE_IMAGE, imag_mode, EFFECT_IMAGE_START, EFFECT_IMAGE_END);
    do_effect1(PE_FORM, form_mode, EFFECT_FORM_START, EFFECT_FORM_END);
    do_effect1(PE_VISITED, visited_mode, EFFECT_VISITED_START,
        EFFECT_VISITED_END);
    do_effect1(PE_ACTIVE, active_mode, EFFECT_ACTIVE_START, EFFECT_ACTIVE_END);
    do_effect1(PE_MARK, mark_mode, EFFECT_MARK_START, EFFECT_MARK_END);
}

#ifdef USE_ANSI_COLOR
static void
do_color(Linecolor c)
{
    if (c & 0x8)
        screen_setfcolor(c & 0x7);
    else if (color_mode & 0x8)
        screen_setfcolor(basic_color);
#ifdef USE_BG_COLOR
    if (c & 0x80)
        screen_setbcolor((c >> 4) & 0x7);
    else if (color_mode & 0x80)
        screen_setbcolor(bg_color);
#endif
    color_mode = c;
}
#endif

void addChar(char c, Lineprop mode)
{
    addMChar(&c, mode, 1);
}

void addMChar(char* p, Lineprop mode, size_t len)
{
    Lineprop m = CharEffect(mode);

    char c = *p;

    if (mode & PC_WCHAR2)
        return;

    do_effects(m);
    if (mode & PC_SYMBOL) {
        char** symbol;

        int w = (mode & PC_KANJI) ? 2 : 1;

        c = ((char)wtf_get_code((wc_uchar*)p) & 0x7f) - SYMBOL_BASE;
        if (graph_ok() && c < N_GRAPH_SYMBOL) {
            if (!graph_mode) {
                screen_graphstart();
                graph_mode = TRUE;
            }

            if (w == 2 && WcOption.use_wide)
                screen_wc_addstr(graph2_symbol[(unsigned char)c % N_GRAPH_SYMBOL]);
            else
                screen_addch(*graph_symbol[(unsigned char)c % N_GRAPH_SYMBOL], 1);
        } else {
            symbol = get_symbol(getRuntime()->DisplayCharset, &w);
            screen_wc_addstr(symbol[(unsigned char)c % N_SYMBOL]);
        }
    } else if (mode & PC_CTRL) {
        switch (c) {
        case '\t':
            screen_add_tab();
            break;
        case '\n':
            screen_addch(' ', 1);
            break;
        case '\r':
            break;
        case DEL_CODE:
            screen_wc_addstr("^?");
            break;
        default:
            screen_addch('^', 1);
            screen_addch(c + '@', 1);
            break;
        }
    }

    else if (mode & PC_UNKNOWN) {
        char buf[5];
        sprintf(buf, "[%.2X]",
            (unsigned char)wtf_get_code((wc_uchar*)p) | 0x80);
        screen_wc_addstr(buf);
    } else {
        screen_addmch(p, len, wtf_width(p));
    }
}

static GeneralList* message_list = NULL;

void record_err_message(char* s)
{
    if (fmInitialized()) {
        if (!message_list)
            message_list = newGeneralList();
        if (message_list->nitem >= TTY_LINES())
            popValue(message_list);
        pushValue(message_list, allocStr(s, -1));
    }
}

/*
 * List of error messages
 */
struct Buffer*
message_list_panel(void)
{
    Str tmp = Strnew_size(TTY_LINES() * TTY_COLS());
    ListItem* p;

    /* FIXME: gettextize? */
    Strcat_charp(tmp,
        "<html><head><title>List of error messages</title></head><body>"
        "<h1>List of error messages</h1><table cellpadding=0>\n");
    if (message_list)
        for (p = message_list->last; p; p = p->prev)
            Strcat_m_charp(tmp, "<tr><td><pre>", html_quote(p->ptr),
                "</pre></td></tr>\n", NULL);
    else
        Strcat_charp(tmp, "<tr><td>(no message recorded)</td></tr>\n");
    Strcat_charp(tmp, "</table></body></html>");
    return loadHTMLString(tmp);
}

void message(char* s, int return_x, int return_y)
{
    if (!fmInitialized())
        return;
    screen_move(LASTLINE(), 0);
    screen_wc_addstr_width(s, TTY_COLS() - 1);
    screen_clrtoeolx();
    screen_move(return_y, return_x);
}

void disp_err_message(char* s, int redraw_current)
{
    record_err_message(s);
    disp_message(s, redraw_current);
}

void disp_message_nsec(char* s, int redraw_current, int sec, int purge, int mouse)
{
    if (QuietMessage)
        return;
    if (!fmInitialized()) {
        fprintf(stderr, "%s\n", conv_to_system(s));
        return;
    }
    if (CurrentTab() != NULL && Currentbuf != NULL)
        message(s, Currentbuf->cursorX + Currentbuf->rootX,
            Currentbuf->cursorY + Currentbuf->rootY);
    else
        message(s, LASTLINE(), 0);
    tty_refresh();
}

void disp_message(char* s, int redraw_current)
{
    disp_message_nsec(s, redraw_current, 10, FALSE, TRUE);
}

void set_delayed_message(char* s)
{
    delayed_msg = allocStr(s, -1);
}
