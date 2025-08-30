#include "display.h"
#include "image.h"
#include "ui.h"
#include "symbol.h"
#include "file.h"
#include "w3m.h"
#include "history.h"
#include "ctrlcode.h"
#include "buffer.h"
#include "TermEntry.h"
#include "term_renderer.h"
#include "graphicchar.h"
#include "screen.h"
#include "frame.h"
#include "tty.h"
#include "putc.h"
#include "fm.h"
#include <assert.h>
#include <wtf.h>

/* *INDENT-OFF* */

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

#define EFFECT_ANCHOR_START_C setfcolor(vt, anchor_color)
#define EFFECT_IMAGE_START_C setfcolor(vt, image_color)
#define EFFECT_FORM_START_C setfcolor(vt, form_color)
#define EFFECT_ACTIVE_START_C (setfcolor(vt, active_color), underline(vt))
#define EFFECT_VISITED_START_C setfcolor(vt, visited_color)
#define EFFECT_MARK_START_C setbcolor(vt, mark_color)

#define EFFECT_IMAGE_END_C setfcolor(vt, basic_color)
#define EFFECT_ANCHOR_END_C setfcolor(vt, basic_color)
#define EFFECT_FORM_END_C setfcolor(vt, basic_color)
#define EFFECT_ACTIVE_END_C (setfcolor(vt, basic_color), underlineend(vt))
#define EFFECT_VISITED_END_C setfcolor(vt, basic_color)
#define EFFECT_MARK_END_C setbcolor(vt, bg_color)

#define EFFECT_ANCHOR_START_M underline(vt)
#define EFFECT_ANCHOR_END_M underlineend(vt)
#define EFFECT_IMAGE_START_M standout(vt)
#define EFFECT_IMAGE_END_M standend(vt)
#define EFFECT_FORM_START_M standout(vt)
#define EFFECT_FORM_END_M standend(vt)
#define EFFECT_ACTIVE_START_NC underline(vt)
#define EFFECT_ACTIVE_END_NC underlineend(vt)
#define EFFECT_ACTIVE_START_M bold(vt)
#define EFFECT_ACTIVE_END_M boldend(vt)
#define EFFECT_VISITED_START_M /**/
#define EFFECT_VISITED_END_M /**/
#define EFFECT_MARK_START_M standout(vt)
#define EFFECT_MARK_END_M standend(vt)
#define define_effect(name_start, name_end, color_start, color_end, mono_start, mono_end) \
    static void name_start                                                                \
    {                                                                                     \
        struct VirtualTerm* vt = getScreen();                                             \
        if (useColor) {                                                                   \
            color_start;                                                                  \
        } else {                                                                          \
            mono_start;                                                                   \
        }                                                                                 \
    }                                                                                     \
    static void name_end                                                                  \
    {                                                                                     \
        struct VirtualTerm* vt = getScreen();                                             \
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
    struct VirtualTerm* vt = getScreen();
    if (useColor) {
        if (useActiveColor) {
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
    struct VirtualTerm* vt = getScreen();
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
    struct VirtualTerm* vt = getScreen();
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
    struct VirtualTerm* vt = getScreen();
    if (useVisitedColor) {
        if (useColor) {
            EFFECT_VISITED_END_C;
        } else {
            EFFECT_VISITED_END_M;
        }
    }
}

/*
 * Display some lines.
 */
static Line* cline = NULL;
static int ccolumn = -1;

static int ulmode = 0, somode = 0, bomode = 0;
static int anch_mode = 0, emph_mode = 0, imag_mode = 0, form_mode = 0,
           active_mode = 0, visited_mode = 0, mark_mode = 0, graph_mode = 0;
static Linecolor color_mode = 0;

static Buffer* save_current_buf = NULL;

static char* delayed_msg = NULL;

static void drawAnchorCursor(Buffer* buf);
static void redrawNLine(Buffer* buf, int n);
static Line* redrawLine(Buffer* buf, Line* l, int i);
static int image_touch = 0;
static int draw_image_flag = FALSE;
static Line* redrawLineImage(Buffer* buf, Line* l, int i);
static int redrawLineRegion(Buffer* buf, Line* l, int i, int bpos, int epos);
static void do_effects(Lineprop m);
static void do_color(Linecolor c);

static Str
make_lastline_link(Buffer* buf, char* title, char* url)
{
    Str s = NULL, u;
    Lineprop* pr;
    ParsedURL pu;
    char* p;
    int l = getScreen()->COLS - 1, i;

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
        s = Strnew_size(getScreen()->COLS);
    i = (l - 2) / 2;
    while (i && pr[i] & PC_WCHAR2)
        i--;
    Strcat_charp_n(s, u->ptr, i);
    Strcat_charp(s, "..");
    i = get_Str_strwidth(u) - (getScreen()->COLS - 1 - get_Str_strwidth(s));
    while (i < u->length && pr[i] & PC_WCHAR2)
        i++;
    Strcat_charp(s, &u->ptr[i]);
    return s;
}

static Str
make_lastline_message(Buffer* buf)
{
    Str msg, s = NULL;
    int sl = 0;

    if (displayLink) {
        MapArea* a = retrieveCurrentMapArea(buf);
        if (a)
            s = make_lastline_link(buf, a->alt, a->url);
        else {
            Anchor* a = retrieveCurrentAnchor(buf);
            char* p = NULL;
            if (a && a->title && *a->title)
                p = a->title;
            else {
                Anchor* a_img = retrieveCurrentImg(buf);
                if (a_img && a_img->title && *a_img->title)
                    p = a_img->title;
            }
            if (p || a)
                s = make_lastline_link(buf, p, a ? a->url : NULL);
        }
        if (s) {
            sl = get_Str_strwidth(s);
            if (sl >= getScreen()->COLS - 3)
                return s;
        }
    }

    msg = Strnew();
    if (displayLineInfo && buf->currentLine != NULL && buf->lastLine != NULL) {
        int cl = buf->currentLine->real_linenumber;
        int ll = buf->lastLine->real_linenumber;
        int r = (int)((double)cl * 100.0 / (double)(ll ? ll : 1) + 0.5);
        Strcat(msg, Sprintf("%d/%d (%d%%)", cl, ll, r));
    } else
        /* FIXME: gettextize? */
        Strcat_charp(msg, "Viewing");
    if (buf->ssl_certificate)
        Strcat_charp(msg, "[SSL]");
    Strcat_charp(msg, " <");
    Strcat_charp(msg, buf->buffername);

    if (s) {
        int l = getScreen()->COLS - 3 - sl;
        if (get_Str_strwidth(msg) > l) {
            char* p;
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

struct Frame* screenToFrame(const struct VirtualTerm* vt)
{
    // struct VirtualTerm* vt = getScreen();
    struct Frame* frame = New(struct Frame);
    frame->rows = getScreen()->ROWS;
    frame->cols = getScreen()->COLS;
    frame->cells = New_N(struct Cell, frame->rows * frame->cols);
    struct Cell* cell = frame->cells;
    for (int y = 0; y < frame->rows; ++y) {
        Screen* l = vt->ScreenImage[y];
        for (int x = 0; x < frame->cols; ++x, ++cell) {
            cell->prop = l->lineprop[x];
            if (cell->prop & S_EOL || CHMODE(cell->prop) == C_WCHAR2) {
                memset(cell->str, 0, sizeof(cell->str));
            } else {
                struct ArrayInfo info = {
                    .buf = cell->str,
                    .len = sizeof(cell->str),
                    .pos = 0,
                };
                struct Writer w;
                makeArrayWriter(&w, &info);
                wc_putc_init(InnerCharset, DisplayCharset);
                const char* str = l->lineimage[x];
                if (str) {
                    wc_putc(&w, str);
                } else {
                    wc_putc(&w, " ");
                }
                wc_putc_end(&w);
            }
        }
    }

    return frame;
}

struct Frame* displayBuffer()
{
    struct VirtualTerm* vt = getScreen();
    Buffer* buf = Currentbuf;
    assert(buf);

    // if (buf->topLine == NULL && readBufferCache(buf) == 0) { /* clear_buffer */
    //     mode = B_FORCE_REDRAW;
    // }

    if (buf->width == 0)
        buf->width = getScreen()->COLS;
    if (buf->height == 0)
        buf->height = getScreen()->ROWS;
    if ((buf->width != getScreen()->COLS && (is_html_type(buf->type) || FoldLine))
        || buf->need_reshape) {
        buf->need_reshape = TRUE;
        reshapeBuffer(buf);
    }
    if (showLineNum) {
        if (buf->lastLine && buf->lastLine->real_linenumber > 0)
            buf->rootX = (int)(log(buf->lastLine->real_linenumber + 0.1)
                             / log(10))
                + 2;
        if (buf->rootX < 5)
            buf->rootX = 5;
        if (buf->rootX > getScreen()->COLS)
            buf->rootX = getScreen()->COLS;
    } else
        buf->rootX = 0;
    buf->COLS = getScreen()->COLS - buf->rootX;

    int ny = 0;
    if (buf->rootY != ny || buf->LINES != getScreen()->ROWS - 1 - ny) {
        buf->rootY = ny;
        buf->LINES = getScreen()->ROWS - 1 - ny;
        arrangeCursor(buf);
    }
    // if (cline != buf->topLine || ccolumn != buf->currentColumn) {
    if (activeImage && (cline != buf->topLine || ccolumn != buf->currentColumn)) {
        if (draw_image_flag) {
            clear(getScreen());
            termClear(ttyWriter());
        }
        clearImage();
        loadImage(buf, IMG_FLAG_STOP);
        image_touch++;
        draw_image_flag = FALSE;
    }
    redrawNLine(buf, getScreen()->ROWS - 1);
    cline = buf->topLine;
    ccolumn = buf->currentColumn;
    // }
    if (buf->topLine == NULL)
        buf->topLine = buf->firstLine;

    drawAnchorCursor(buf);

    Str msg;
    msg = make_lastline_message(buf);
    if (buf->firstLine == NULL) {
        /* FIXME: gettextize? */
        Strcat_charp(msg, "\tNo Line");
    }
    if (delayed_msg != NULL) {
        message(getUI(), MSG_INFO, delayed_msg);
        delayed_msg = NULL;
        // refresh(ttyWriter());
    }
    standout(vt);
    message(getUI(), MSG_INFO, msg->ptr);
    standend(vt);
    term_title(conv_to_system(buf->buffername));
    // refresh(ttyWriter());
    if (activeImage && displayImage && buf->img && buf->image_loaded) {
        drawImage();
    }
    if (buf != save_current_buf) {
        saveBufferInfo();
        save_current_buf = buf;
    }
    if (buf->check_url & CHK_URL) {
        chkURLBuffer(buf);
        displayBuffer();
    }

    return screenToFrame(getScreen());
}

static void
drawAnchorCursor0(Buffer* buf, AnchorList* al, int hseq, int prevhseq,
    int tline, int eline, int active)
{
    int i, j;
    Line* l;
    Anchor* an;

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
drawAnchorCursor(Buffer* buf)
{
    Anchor* an;
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
redrawNLine(Buffer* buf, int n)
{
    struct VirtualTerm* vt = getScreen();
    Line* l;
    int i;

    if (useColor) {
        EFFECT_ANCHOR_END_C;
        setbcolor(vt, bg_color);
    }

    for (i = 0, l = buf->topLine; i < buf->LINES; i++, l = l->next) {
        if (i >= buf->LINES - n || i < -n)
            l = redrawLine(buf, l, i + buf->rootY);
        if (l == NULL)
            break;
    }
    if (n > 0) {
        move(vt, i + buf->rootY, 0);
        clrtobotx(vt);
    }

    if (!(activeImage && displayImage && buf->img))
        return;
    move(vt, buf->cursorY + buf->rootY, buf->cursorX + buf->rootX);
    for (i = 0, l = buf->topLine; i < buf->LINES && l; i++, l = l->next) {
        if (i >= buf->LINES - n || i < -n)
            redrawLineImage(buf, l, i + buf->rootY);
    }
    getAllImage(buf);
}

static Line*
redrawLine(Buffer* buf, Line* l, int i)
{
    struct VirtualTerm* vt = getScreen();
    int j, pos, rcol, ncol, delta = 1;
    int column = buf->currentColumn;
    char* p;
    Lineprop* pr;
    Linecolor* pc;
    Anchor* a;
    ParsedURL url;
    int k, vpos = -1;

    if (l == NULL) {
        return NULL;
    }
    move(vt, i, 0);
    if (showLineNum) {
        char tmp[16];
        if (!buf->rootX) {
            if (buf->lastLine->real_linenumber > 0)
                buf->rootX = (int)(log(buf->lastLine->real_linenumber + 0.1)
                                 / log(10))
                    + 2;
            if (buf->rootX < 5)
                buf->rootX = 5;
            if (buf->rootX > getScreen()->COLS)
                buf->rootX = getScreen()->COLS;
            buf->COLS = getScreen()->COLS - buf->rootX;
        }
        if (l->real_linenumber && !l->bpos)
            sprintf(tmp, "%*ld:", buf->rootX - 1, l->real_linenumber);
        else
            sprintf(tmp, "%*s ", buf->rootX - 1, "");
        addstr(vt, tmp);
    }
    move(vt, i, buf->rootX);
    if (l->width < 0)
        l->width = COLPOS(l, l->len);
    if (l->len == 0 || l->width - 1 < column) {
        clrtoeolx(vt);
        return l;
    }
    /* need_clrtoeol(); */
    pos = columnPos(l, column);
    p = &(l->lineBuf[pos]);
    pr = &(l->propBuf[pos]);
    if (useColor && l->colorBuf)
        pc = &(l->colorBuf[pos]);
    else
        pc = NULL;
    rcol = COLPOS(l, pos);

    for (j = 0; rcol - column < buf->COLS && pos + j < l->len; j += delta) {
        if (useVisitedColor && vpos <= pos + j && !(pr[j] & PE_VISITED)) {
            a = retrieveAnchor(buf->href, l->linenumber, pos + j);
            if (a) {
                parseURL2(a->url, &url, baseURL(buf));
                if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
                    for (k = a->start.pos; k < a->end.pos; k++)
                        pr[k - pos] |= PE_VISITED;
                }
                vpos = a->end.pos;
            }
        }
        delta = wtf_len((wc_uchar*)&p[j]);
        ncol = COLPOS(l, pos + j + delta);
        if (ncol - column > buf->COLS)
            break;
        if (pc)
            do_color(pc[j]);
        if (rcol < column) {
            for (rcol = column; rcol < ncol; rcol++)
                addChar(' ', 0);
            continue;
        }
        if (p[j] == '\t') {
            for (; rcol < ncol; rcol++)
                addChar(' ', 0);
        } else {
            addMChar(&p[j], pr[j], delta);
        }
        rcol = ncol;
    }
    if (somode) {
        somode = FALSE;
        standend(vt);
    }
    if (ulmode) {
        ulmode = FALSE;
        underlineend(vt);
    }
    if (bomode) {
        bomode = FALSE;
        boldend(vt);
    }
    if (emph_mode) {
        emph_mode = FALSE;
        boldend(vt);
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
        graphend(vt);
    }
    if (color_mode)
        do_color(0);
    if (rcol - column < buf->COLS)
        clrtoeolx(vt);
    return l;
}

static Line*
redrawLineImage(Buffer* buf, Line* l, int i)
{
    int j, pos, rcol;
    int column = buf->currentColumn;
    Anchor* a;
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
            Image* image = a->image;
            ImageCache* cache;

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
                if (h > (int)((getScreen()->ROWS - 1) * pixel_per_line - y))
                    h = (int)((getScreen()->ROWS - 1) * pixel_per_line - y);
                addImage(cache, x, y, sx, sy, w, h);
                image->touch = image_touch;
                draw_image_flag = TRUE;
            }
        }
        rcol = COLPOS(l, pos + j + 1);
    }
    return l;
}

static int
redrawLineRegion(Buffer* buf, Line* l, int i, int bpos, int epos)
{
    struct VirtualTerm* vt = getScreen();
    int j, pos, rcol, ncol, delta = 1;
    int column = buf->currentColumn;
    char* p;
    Lineprop* pr;
    Linecolor* pc;
    int bcol, ecol;
    Anchor* a;
    ParsedURL url;
    int k, vpos = -1;

    if (l == NULL)
        return 0;
    pos = columnPos(l, column);
    p = &(l->lineBuf[pos]);
    pr = &(l->propBuf[pos]);
    if (useColor && l->colorBuf)
        pc = &(l->colorBuf[pos]);
    else
        pc = NULL;
    rcol = COLPOS(l, pos);
    bcol = bpos - pos;
    ecol = epos - pos;

    for (j = 0; rcol - column < buf->COLS && pos + j < l->len; j += delta) {
        if (useVisitedColor && vpos <= pos + j && !(pr[j] & PE_VISITED)) {
            a = retrieveAnchor(buf->href, l->linenumber, pos + j);
            if (a) {
                parseURL2(a->url, &url, baseURL(buf));
                if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
                    for (k = a->start.pos; k < a->end.pos; k++)
                        pr[k - pos] |= PE_VISITED;
                }
                vpos = a->end.pos;
            }
        }
        delta = wtf_len((wc_uchar*)&p[j]);
        ncol = COLPOS(l, pos + j + delta);
        if (ncol - column > buf->COLS)
            break;
        if (pc)
            do_color(pc[j]);
        if (j >= bcol && j < ecol) {
            if (rcol < column) {
                move(vt, i, buf->rootX);
                for (rcol = column; rcol < ncol; rcol++)
                    addChar(' ', 0);
                continue;
            }
            move(vt, i, rcol - column + buf->rootX);
            if (p[j] == '\t') {
                for (; rcol < ncol; rcol++)
                    addChar(' ', 0);
            } else
                addMChar(&p[j], pr[j], delta);
        }
        rcol = ncol;
    }
    if (somode) {
        somode = FALSE;
        standend(vt);
    }
    if (ulmode) {
        ulmode = FALSE;
        underlineend(vt);
    }
    if (bomode) {
        bomode = FALSE;
        boldend(vt);
    }
    if (emph_mode) {
        emph_mode = FALSE;
        boldend(vt);
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
        graphend(vt);
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
    struct VirtualTerm* vt = getScreen();
    /* effect end */
    do_effect2(PE_UNDER, ulmode, underline(vt), underlineend(vt));
    do_effect2(PE_STAND, somode, standout(vt), standend(vt));
    do_effect2(PE_BOLD, bomode, bold(vt), boldend(vt));
    do_effect2(PE_EMPH, emph_mode, bold(vt), boldend(vt));
    do_effect2(PE_ANCHOR, anch_mode, EFFECT_ANCHOR_START, EFFECT_ANCHOR_END);
    do_effect2(PE_IMAGE, imag_mode, EFFECT_IMAGE_START, EFFECT_IMAGE_END);
    do_effect2(PE_FORM, form_mode, EFFECT_FORM_START, EFFECT_FORM_END);
    do_effect2(PE_VISITED, visited_mode, EFFECT_VISITED_START,
        EFFECT_VISITED_END);
    do_effect2(PE_ACTIVE, active_mode, EFFECT_ACTIVE_START, EFFECT_ACTIVE_END);
    do_effect2(PE_MARK, mark_mode, EFFECT_MARK_START, EFFECT_MARK_END);
    if (graph_mode) {
        graphend(vt);
        graph_mode = FALSE;
    }

    /* effect start */
    do_effect1(PE_UNDER, ulmode, underline(vt), underlineend(vt));
    do_effect1(PE_STAND, somode, standout(vt), standend(vt));
    do_effect1(PE_BOLD, bomode, bold(vt), boldend(vt));
    do_effect1(PE_EMPH, emph_mode, bold(vt), boldend(vt));
    do_effect1(PE_ANCHOR, anch_mode, EFFECT_ANCHOR_START, EFFECT_ANCHOR_END);
    do_effect1(PE_IMAGE, imag_mode, EFFECT_IMAGE_START, EFFECT_IMAGE_END);
    do_effect1(PE_FORM, form_mode, EFFECT_FORM_START, EFFECT_FORM_END);
    do_effect1(PE_VISITED, visited_mode, EFFECT_VISITED_START,
        EFFECT_VISITED_END);
    do_effect1(PE_ACTIVE, active_mode, EFFECT_ACTIVE_START, EFFECT_ACTIVE_END);
    do_effect1(PE_MARK, mark_mode, EFFECT_MARK_START, EFFECT_MARK_END);
}

static void
do_color(Linecolor c)
{
    struct VirtualTerm* vt = getScreen();
    if (c & 0x8)
        setfcolor(vt, c & 0x7);
    else if (color_mode & 0x8)
        setfcolor(vt, basic_color);
    if (c & 0x80)
        setbcolor(vt, (c >> 4) & 0x7);
    else if (color_mode & 0x80)
        setbcolor(vt, bg_color);
    color_mode = c;
}

void addChar(char c, Lineprop mode)
{
    addMChar(&c, mode, 1);
}

void addMChar(char* p, Lineprop mode, size_t len)
{
    struct VirtualTerm* vt = getScreen();
    struct TermEntry* t = getTermEntry();
    Lineprop m = CharEffect(mode);
    char c = *p;

    if (mode & PC_WCHAR2)
        return;
    do_effects(m);
    if (mode & PC_SYMBOL) {
        char** symbol;
        int w = (mode & PC_KANJI) ? 2 : 1;

        c = ((char)wtf_get_code((wc_uchar*)p) & 0x7f) - SYMBOL_BASE;
        if (graph_ok(t) && c < N_GRAPH_SYMBOL) {
            if (!graph_mode) {
                graphstart(vt);
                graph_mode = TRUE;
            }
            if (w == 2 && WcOption.use_wide)
                addstr(vt, graph2_symbol[(unsigned char)c % N_GRAPH_SYMBOL]);
            else
                addstr(vt, graph_symbol[(unsigned char)c % N_GRAPH_SYMBOL]);
        } else {
            symbol = get_symbol(DisplayCharset, &w);
            addstr(vt, symbol[(unsigned char)c % N_SYMBOL]);
        }
    } else if (mode & PC_CTRL) {
        switch (c) {
        case '\t':
            addch(vt, c);
            break;
        case '\n':
            addch(vt, ' ');
            break;
        case '\r':
            break;
        case DEL_CODE:
            addstr(vt, "^?");
            break;
        default:
            addch(vt, '^');
            addch(vt, c + '@');
            break;
        }
    } else if (mode & PC_UNKNOWN) {
        char buf[5];
        sprintf(buf, "[%.2X]",
            (unsigned char)wtf_get_code((wc_uchar*)p) | 0x80);
        addstr(vt, buf);
    } else
        addmch(vt, p, len);
}

/*
 * List of error messages
 */
Buffer*
message_list_panel(void)
{
    Str tmp = Strnew_size(getScreen()->ROWS * getScreen()->COLS);
    ListItem* p;

    /* FIXME: gettextize? */
    Strcat_charp(tmp,
        "<html><head><title>List of error messages</title></head><body>"
        "<h1>List of error messages</h1><table cellpadding=0>\n");

    concatMessageList(tmp);

    Strcat_charp(tmp, "</table></body></html>");
    return loadHTMLString(tmp);
}

void set_delayed_message(char* s)
{
    delayed_msg = allocStr(s, -1);
}

void cursorUp0(Buffer* buf, int n)
{
    if (buf->cursorY > 0)
        cursorUpDown(buf, -1);
    else {
        buf->topLine = lineSkip(buf, buf->topLine, -n, FALSE);
        if (buf->currentLine->prev != NULL)
            buf->currentLine = buf->currentLine->prev;
        arrangeLine(buf);
    }
}

void cursorUp(Buffer* buf, int n)
{
    Line* l = buf->currentLine;
    if (buf->firstLine == NULL)
        return;
    while (buf->currentLine->prev && buf->currentLine->bpos)
        cursorUp0(buf, n);
    if (buf->currentLine == buf->firstLine) {
        gotoLine(buf, l->linenumber);
        arrangeLine(buf);
        return;
    }
    cursorUp0(buf, n);
    while (buf->currentLine->prev && buf->currentLine->bpos && buf->currentLine->bwidth >= buf->currentColumn + buf->visualpos)
        cursorUp0(buf, n);
}

void cursorDown0(Buffer* buf, int n)
{
    if (buf->cursorY < buf->LINES - 1)
        cursorUpDown(buf, 1);
    else {
        buf->topLine = lineSkip(buf, buf->topLine, n, FALSE);
        if (buf->currentLine->next != NULL)
            buf->currentLine = buf->currentLine->next;
        arrangeLine(buf);
    }
}

void cursorDown(Buffer* buf, int n)
{
    Line* l = buf->currentLine;
    if (buf->firstLine == NULL)
        return;
    while (buf->currentLine->next && buf->currentLine->next->bpos)
        cursorDown0(buf, n);
    if (buf->currentLine == buf->lastLine) {
        gotoLine(buf, l->linenumber);
        arrangeLine(buf);
        return;
    }
    cursorDown0(buf, n);
    while (buf->currentLine->next && buf->currentLine->next->bpos && buf->currentLine->bwidth + buf->currentLine->width < buf->currentColumn + buf->visualpos)
        cursorDown0(buf, n);
}

void cursorUpDown(Buffer* buf, int n)
{
    Line* cl = buf->currentLine;

    if (buf->firstLine == NULL)
        return;
    if ((buf->currentLine = currentLineSkip(buf, cl, n, FALSE)) == cl)
        return;
    arrangeLine(buf);
}

void cursorRight(Buffer* buf, int n)
{
    int i, delta = 1, cpos, vpos2;
    Line* l = buf->currentLine;

    if (buf->firstLine == NULL)
        return;
    if (buf->pos == l->len && !(l->next && l->next->bpos))
        return;
    i = buf->pos;
    Lineprop* p = l->propBuf;
    while (i + delta < l->len && p[i + delta] & PC_WCHAR2)
        delta++;
    if (i + delta < l->len) {
        buf->pos = i + delta;
    } else if (l->len == 0) {
        buf->pos = 0;
    } else if (l->next && l->next->bpos) {
        cursorDown0(buf, 1);
        buf->pos = 0;
        arrangeCursor(buf);
        return;
    } else {
        buf->pos = l->len - 1;
        while (buf->pos && p[buf->pos] & PC_WCHAR2)
            buf->pos--;
    }
    cpos = COLPOS(l, buf->pos);
    buf->visualpos = l->bwidth + cpos - buf->currentColumn;
    delta = 1;
    while (buf->pos + delta < l->len && p[buf->pos + delta] & PC_WCHAR2)
        delta++;
    vpos2 = COLPOS(l, buf->pos + delta) - buf->currentColumn - 1;
    if (vpos2 >= buf->COLS && n) {
        columnSkip(buf, n + (vpos2 - buf->COLS) - (vpos2 - buf->COLS) % n);
        buf->visualpos = l->bwidth + cpos - buf->currentColumn;
    }
    buf->cursorX = buf->visualpos - l->bwidth;
}

void cursorLeft(Buffer* buf, int n)
{
    int i, delta = 1, cpos;
    Line* l = buf->currentLine;

    if (buf->firstLine == NULL)
        return;
    i = buf->pos;
    Lineprop* p = l->propBuf;
    while (i - delta > 0 && p[i - delta] & PC_WCHAR2)
        delta++;
    if (i >= delta)
        buf->pos = i - delta;
    else if (l->prev && l->bpos) {
        cursorUp0(buf, -1);
        buf->pos = buf->currentLine->len - 1;
        arrangeCursor(buf);
        return;
    } else
        buf->pos = 0;
    cpos = COLPOS(l, buf->pos);
    buf->visualpos = l->bwidth + cpos - buf->currentColumn;
    if (buf->visualpos - l->bwidth < 0 && n) {
        columnSkip(buf,
            -n + buf->visualpos - l->bwidth - (buf->visualpos - l->bwidth) % n);
        buf->visualpos = l->bwidth + cpos - buf->currentColumn;
    }
    buf->cursorX = buf->visualpos - l->bwidth;
}

void cursorHome(Buffer* buf)
{
    buf->visualpos = 0;
    buf->cursorX = buf->cursorY = 0;
}

/*
 * Arrange line,column and cursor position according to current line and
 * current position.
 */
void arrangeCursor(Buffer* buf)
{
    int col, col2, pos;
    int delta = 1;
    if (buf == NULL || buf->currentLine == NULL)
        return;
    /* Arrange line */
    if (buf->currentLine->linenumber - buf->topLine->linenumber >= buf->LINES
        || buf->currentLine->linenumber < buf->topLine->linenumber) {
        /*
         * buf->topLine = buf->currentLine;
         */
        buf->topLine = lineSkip(buf, buf->currentLine, 0, FALSE);
    }
    /* Arrange column */
    while (buf->pos < 0 && buf->currentLine->prev && buf->currentLine->bpos) {
        pos = buf->pos + buf->currentLine->prev->len;
        cursorUp0(buf, 1);
        buf->pos = pos;
    }
    while (buf->pos >= buf->currentLine->len && buf->currentLine->next && buf->currentLine->next->bpos) {
        pos = buf->pos - buf->currentLine->len;
        cursorDown0(buf, 1);
        buf->pos = pos;
    }
    if (buf->currentLine->len == 0 || buf->pos < 0)
        buf->pos = 0;
    else if (buf->pos >= buf->currentLine->len)
        buf->pos = buf->currentLine->len - 1;
    while (buf->pos > 0 && buf->currentLine->propBuf[buf->pos] & PC_WCHAR2)
        buf->pos--;
    col = COLPOS(buf->currentLine, buf->pos);
    while (buf->pos + delta < buf->currentLine->len && buf->currentLine->propBuf[buf->pos + delta] & PC_WCHAR2)
        delta++;
    col2 = COLPOS(buf->currentLine, buf->pos + delta);
    if (col < buf->currentColumn || col2 > buf->COLS + buf->currentColumn) {
        buf->currentColumn = 0;
        if (col2 > buf->COLS)
            columnSkip(buf, col);
    }
    /* Arrange cursor */
    buf->cursorY = buf->currentLine->linenumber - buf->topLine->linenumber;
    buf->visualpos = buf->currentLine->bwidth + COLPOS(buf->currentLine, buf->pos) - buf->currentColumn;
    buf->cursorX = buf->visualpos - buf->currentLine->bwidth;
#ifdef DISPLAY_DEBUG
    fprintf(stderr,
        "arrangeCursor: column=%d, cursorX=%d, visualpos=%d, pos=%d, len=%d\n",
        buf->currentColumn, buf->cursorX, buf->visualpos, buf->pos,
        buf->currentLine->len);
#endif
}

void arrangeLine(Buffer* buf)
{
    int i, cpos;

    if (buf->firstLine == NULL)
        return;
    buf->cursorY = buf->currentLine->linenumber - buf->topLine->linenumber;
    i = columnPos(buf->currentLine, buf->currentColumn + buf->visualpos - buf->currentLine->bwidth);
    cpos = COLPOS(buf->currentLine, i) - buf->currentColumn;
    if (cpos >= 0) {
        buf->cursorX = cpos;
        buf->pos = i;
    } else if (buf->currentLine->len > i) {
        buf->cursorX = 0;
        buf->pos = i + 1;
    } else {
        buf->cursorX = 0;
        buf->pos = 0;
    }
#ifdef DISPLAY_DEBUG
    fprintf(stderr,
        "arrangeLine: column=%d, cursorX=%d, visualpos=%d, pos=%d, len=%d\n",
        buf->currentColumn, buf->cursorX, buf->visualpos, buf->pos,
        buf->currentLine->len);
#endif
}

void cursorXY(Buffer* buf, int x, int y)
{
    int oldX;

    cursorUpDown(buf, y - buf->cursorY);

    if (buf->cursorX > x) {
        while (buf->cursorX > x)
            cursorLeft(buf, buf->COLS / 2);
    } else if (buf->cursorX < x) {
        while (buf->cursorX < x) {
            oldX = buf->cursorX;

            cursorRight(buf, buf->COLS / 2);

            if (oldX == buf->cursorX)
                break;
        }
        if (buf->cursorX > x)
            cursorLeft(buf, buf->COLS / 2);
    }
}

void restorePosition(Buffer* buf, Buffer* orig)
{
    buf->topLine = lineSkip(buf, buf->firstLine, TOP_LINENUMBER(orig) - 1,
        FALSE);
    gotoLine(buf, CUR_LINENUMBER(orig));
    buf->pos = orig->pos;
    if (buf->currentLine && orig->currentLine)
        buf->pos += orig->currentLine->bpos - buf->currentLine->bpos;
    buf->currentColumn = orig->currentColumn;
    arrangeCursor(buf);
}

/* Local Variables:    */
/* c-basic-offset: 4   */
/* tab-width: 8        */
/* End:                */
