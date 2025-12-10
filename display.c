#include "display.h"
#include "Line.h"
#include "Url.h"
#include <gcstr.h>
#include <stdbool.h>
#include "tui.h"
#include "screen.h"
#include "history.h"
#include "mailcap.h"
#include "terms.h"
#include "image.h"
#include "map.h"
#include "fm.h"
#include "w3m_runtime.h"
#include "buffer.h"
#include <math.h>

extern unsigned char last_key;

static struct Buffer* save_current_buf = 0;
static struct Line* cline = 0;
static int ccolumn = -1;
static int image_touch = 0;
static bool draw_image_flag = false;

static struct Line*
redrawLineImage(struct Buffer* buf, struct Line* l, int i)
{
    int j, pos, rcol;
    int column = buf->currentColumn;
    Anchor* a;
    int x, y, sx, sy, w, h;

    if (l == 0)
        return 0;
    if (l->width < 0)
        l->width = COLPOS(l, l->len);
    if (l->len == 0 || l->width - 1 < column)
        return l;
    pos = columnPos(l, column);
    rcol = COLPOS(l, pos);
    for (j = 0; rcol - column < buf->cols && pos + j < l->len; j++) {
        if (rcol - column < 0) {
            rcol = COLPOS(l, pos + j + 1);
            continue;
        }
        a = retrieveAnchor(buf->img, l->linenumber, pos + j);
        if (a && a->image && a->image->touch < image_touch) {
            struct Image* image = a->image;
            struct ImageCache* cache = image->cache = getImage(image, baseURL(buf),
                buf->image_flag);
            if (cache) {
                if ((image->width < 0 && cache->width > 0) || (image->height < 0 && cache->height > 0)) {
                    image->width = cache->width;
                    image->height = cache->height;
                    buf->need_reshape = true;
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
                if (w > (int)((buf->rootX + buf->cols) * pixel_per_char - x))
                    w = (int)((buf->rootX + buf->cols) * pixel_per_char - x);
                if (h > (int)(LINES-1 * pixel_per_line - y))
                    h = (int)(LINES-1 * pixel_per_line - y);
                addImage(cache, x, y, sx, sy, w, h);
                image->touch = image_touch;
                draw_image_flag = true;
            }
        }
        rcol = COLPOS(l, pos + j + 1);
    }
    return l;
}

static struct Line*
redrawLine(struct Buffer* buf, struct Line* l, int i)
{
    int j, pos, rcol, ncol, delta = 1;
    int column = buf->currentColumn;
    char* p;
    Lineprop* pr;
    Linecolor* pc;
    Anchor* a;
    struct Url url;
    int k, vpos = -1;

    if (l == 0) {
        if (buf->pagerSource) {
            l = getNextPage(buf, buf->lines + buf->rootY - i);
            if (l == 0)
                return 0;
        } else
            return 0;
    }
    scr_move(i, 0);
    if (showLineNum) {
        char tmp[16];
        if (!buf->rootX) {
            if (buf->lastLine->real_linenumber > 0)
                buf->rootX = (int)(log(buf->lastLine->real_linenumber + 0.1)
                                 / log(10))
                    + 2;
            if (buf->rootX < 5)
                buf->rootX = 5;
            if (buf->rootX > COLS)
                buf->rootX = COLS;
            buf->cols = COLS - buf->rootX;
        }
        if (l->real_linenumber && !l->bpos)
            sprintf(tmp, "%*ld:", buf->rootX - 1, l->real_linenumber);
        else
            sprintf(tmp, "%*s ", buf->rootX - 1, "");
        scr_addstr(tmp);
    }
    scr_move(i, buf->rootX);
    if (l->width < 0)
        l->width = COLPOS(l, l->len);
    if (l->len == 0 || l->width - 1 < column) {
        scr_clrtoeolx();
        return l;
    }
    /* need_clrtoeol(); */
    pos = columnPos(l, column);
    p = &(l->lineBuf[pos]);
    pr = &(l->propBuf[pos]);
    if (useColor && l->colorBuf)
        pc = &(l->colorBuf[pos]);
    else
        pc = 0;
    rcol = COLPOS(l, pos);

    for (j = 0; rcol - column < buf->cols && pos + j < l->len; j += delta) {
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
        if (ncol - column > buf->cols)
            break;
        if (pc)
            do_color(pc[j]);
        if (rcol < column) {
            for (rcol = column; rcol < ncol; rcol++)
                scr_addChar(' ', 0);
            continue;
        }
        if (p[j] == '\t') {
            for (; rcol < ncol; rcol++)
                scr_addChar(' ', 0);
        } else {
            scr_addMChar(&p[j], pr[j], delta);
        }
        rcol = ncol;
    }
    scr_line_finalize();
    if (rcol - column < buf->cols)
        scr_clrtoeolx();
    return l;
}

static void redrawNLine(struct Buffer* buf, int n)
{
    struct Line* l;
    int i;

    scr_init_color();
    if (nTab > 1) {
        TabBuffer* t;
        int l;

        scr_move(0, 0);
        scr_clrtoeolx();
        for (t = FirstTab; t; t = t->nextTab) {
            scr_move(t->y, t->x1);
            if (t == CurrentTab)
                scr_bold();
            scr_addch('[');
            l = t->x2 - t->x1 - 1 - get_strwidth(t->currentBuffer->buffername);
            if (l < 0)
                l = 0;
            if (l / 2 > 0)
                scr_addnstr_sup(" ", l / 2);
            if (t == CurrentTab)
                scr_active_start();
            scr_addnstr(t->currentBuffer->buffername, t->x2 - t->x1 - l);
            if (t == CurrentTab)
                scr_active_end();
            if ((l + 1) / 2 > 0)
                scr_addnstr_sup(" ", (l + 1) / 2);
            scr_move(t->y, t->x2);
            scr_addch(']');
            if (t == CurrentTab)
                scr_boldend();
        }
        scr_move(LastTab->y + 1, 0);
        for (i = 0; i < COLS; i++)
            scr_addch('~');
    }
    for (i = 0, l = buf->topLine; i < buf->lines; i++, l = l->next) {
        if (i >= buf->lines - n || i < -n)
            l = redrawLine(buf, l, i + buf->rootY);
        if (l == 0)
            break;
    }
    if (n > 0) {
        scr_move(i + buf->rootY, 0);
        scr_clrtobotx();
    }

    if (!(activeImage && displayImage && buf->img))
        return;
    scr_move(buf->cursorY + buf->rootY, buf->cursorX + buf->rootX);
    for (i = 0, l = buf->topLine; i < buf->lines && l; i++, l = l->next) {
        if (i >= buf->lines - n || i < -n)
            redrawLineImage(buf, l, i + buf->rootY);
    }
    getAllImage(buf);
}

static void redrawBuffer(struct Buffer* buf)
{
    redrawNLine(buf, LINES-1);
}

static Str
make_lastline_link(struct Buffer* buf, char* title, char* url)
{
    Str s = 0, u;
    Lineprop* pr;
    struct Url pu;
    char* p;
    int l = COLS - 1, i;

    if (title && *title) {
        s = Strnew_m_charp("[", title, "]", 0);
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
    if (w3m_config.DecodeURL)
        u = Strnew_charp(url_decode2(u->ptr, buf));
    u = checkType(u, &pr, 0);
    if (l <= 4 || l >= get_Str_strwidth(u)) {
        if (!s)
            return u;
        Strcat(s, u);
        return s;
    }
    if (!s)
        s = Strnew_size(COLS);
    i = (l - 2) / 2;
    while (i && pr[i] & PC_WCHAR2)
        i--;
    Strcat_charp_n(s, u->ptr, i);
    Strcat_charp(s, "..");
    i = get_Str_strwidth(u) - (COLS - 1 - get_Str_strwidth(s));
    while (i < u->length && pr[i] & PC_WCHAR2)
        i++;
    Strcat_charp(s, &u->ptr[i]);
    return s;
}

static Str
make_lastline_message(struct Buffer* buf)
{
    Str msg, s = 0;
    int sl = 0;

    if (displayLink) {
        struct MapArea* a = retrieveCurrentMapArea(buf);
        if (a)
            s = make_lastline_link(buf, a->alt, a->url);
        else {
            Anchor* a = retrieveCurrentAnchor(buf);
            char* p = 0;
            if (a && a->title && *a->title)
                p = a->title;
            else {
                Anchor* a_img = retrieveCurrentImg(buf);
                if (a_img && a_img->title && *a_img->title)
                    p = a_img->title;
            }
            if (p || a)
                s = make_lastline_link(buf, p, a ? a->url : 0);
        }
        if (s) {
            sl = get_Str_strwidth(s);
            if (sl >= COLS - 3)
                return s;
        }
    }

    msg = Strnew();
    if (displayLineInfo && buf->currentLine != 0 && buf->lastLine != 0) {
        int cl = buf->currentLine->real_linenumber;
        int ll = buf->lastLine->real_linenumber;
        int r = (int)((double)cl * 100.0 / (double)(ll ? ll : 1) + 0.5);
        Strcat(msg, Sprintf("%d/%d (%d%%)", cl, ll, r));
    } else
        /* FIXME: gettextize? */
        msg = Sprintf("%s: code 0x%02x ", msg->ptr, last_key);
    Strcat_charp(msg, "Viewing");
    if (buf->ssl_certificate)
        Strcat_charp(msg, "[SSL]");
    Strcat_charp(msg, " <");
    Strcat_charp(msg, buf->buffername);

    if (s) {
        int l = COLS - 3 - sl;
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

static void calcTabPos(void)
{
    TabBuffer* tab;
    int lcol = 0, rcol = 0, col;
    int n1, n2, na, nx, ny, ix, iy;

    if (nTab <= 0)
        return;
    n1 = (COLS - rcol - lcol) / TabCols;
    if (n1 >= nTab) {
        n2 = 1;
        ny = 1;
    } else {
        if (n1 < 0)
            n1 = 0;
        n2 = COLS / TabCols;
        if (n2 == 0)
            n2 = 1;
        ny = (nTab - n1 - 1) / n2 + 2;
    }
    na = n1 + n2 * (ny - 1);
    n1 -= (na - nTab) / ny;
    if (n1 < 0)
        n1 = 0;
    na = n1 + n2 * (ny - 1);
    tab = FirstTab;
    for (iy = 0; iy < ny && tab; iy++) {
        if (iy == 0) {
            nx = n1;
            col = COLS - rcol - lcol;
        } else {
            nx = n2 - (na - nTab + (iy - 1)) / (ny - 1);
            col = COLS;
        }
        for (ix = 0; ix < nx && tab; ix++, tab = tab->nextTab) {
            tab->x1 = col * ix / nx;
            tab->x2 = col * (ix + 1) / nx - 1;
            tab->y = iy;
            if (iy == 0) {
                tab->x1 += lcol;
                tab->x2 += lcol;
            }
        }
    }
}

static int
redrawLineRegion(struct Buffer* buf, struct Line* l, int i, int bpos, int epos)
{
    int j, pos, rcol, ncol, delta = 1;
    int column = buf->currentColumn;
    char* p;
    Lineprop* pr;
    Linecolor* pc;
    int bcol, ecol;
    Anchor* a;
    struct Url url;
    int k, vpos = -1;

    if (l == 0)
        return 0;
    pos = columnPos(l, column);
    p = &(l->lineBuf[pos]);
    pr = &(l->propBuf[pos]);
    if (useColor && l->colorBuf)
        pc = &(l->colorBuf[pos]);
    else
        pc = 0;
    rcol = COLPOS(l, pos);
    bcol = bpos - pos;
    ecol = epos - pos;

    for (j = 0; rcol - column < buf->cols && pos + j < l->len; j += delta) {
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
        if (ncol - column > buf->cols)
            break;
        if (pc)
            do_color(pc[j]);
        if (j >= bcol && j < ecol) {
            if (rcol < column) {
                scr_move(i, buf->rootX);
                for (rcol = column; rcol < ncol; rcol++)
                    scr_addChar(' ', 0);
                continue;
            }
            scr_move(i, rcol - column + buf->rootX);
            if (p[j] == '\t') {
                for (; rcol < ncol; rcol++)
                    scr_addChar(' ', 0);
            } else
                scr_addMChar(&p[j], pr[j], delta);
        }
        rcol = ncol;
    }
    scr_line_finalize();
    return rcol - column;
}

static void
drawAnchorCursor0(struct Buffer* buf, AnchorList* al, int hseq, int prevhseq,
    int tline, int eline, int active)
{
    int i, j;
    struct Line* l;
    Anchor* an;

    l = buf->topLine;
    for (j = 0; j < al->nanchor; j++) {
        an = &al->anchors[j];
        if (an->start.line < tline)
            continue;
        if (an->start.line >= eline)
            return;
        for (;; l = l->next) {
            if (l == 0)
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
    eline = tline + buf->lines;
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

void displayBuffer(struct Buffer* buf, enum DisplayMode mode)
{
    if (!buf)
        return;

    if (buf->topLine == 0 && !readBufferCache(buf)) { /* clear_buffer */
        mode = B_FORCE_REDRAW;
    }

    if (buf->width == 0)
        buf->width = INIT_BUFFER_WIDTH;
    if (buf->height == 0)
        buf->height = LINES-1 + 1;
    if ((buf->width != INIT_BUFFER_WIDTH && (is_html_type(buf->type) || FoldLine))
        || buf->need_reshape) {
        buf->need_reshape = true;
        reshapeBuffer(buf);
    }
    if (showLineNum) {
        if (buf->lastLine && buf->lastLine->real_linenumber > 0)
            buf->rootX = (int)(log(buf->lastLine->real_linenumber + 0.1)
                             / log(10))
                + 2;
        if (buf->rootX < 5)
            buf->rootX = 5;
        if (buf->rootX > COLS)
            buf->rootX = COLS;
    } else
        buf->rootX = 0;

    buf->cols = COLS - buf->rootX;
    int ny = 0;
    if (nTab > 1) {
        if (mode == B_FORCE_REDRAW || mode == B_REDRAW_IMAGE)
            calcTabPos();
        ny = LastTab->y + 2;
        if (ny > LINES-1)
            ny = LINES-1;
    }
    if (buf->rootY != ny || buf->lines != LINES-1 - ny) {
        buf->rootY = ny;
        buf->lines = LINES-1 - ny;
        arrangeCursor(buf);
        mode = B_REDRAW_IMAGE;
    }

    if (mode == B_FORCE_REDRAW || mode == B_SCROLL || mode == B_REDRAW_IMAGE || cline != buf->topLine || ccolumn != buf->currentColumn) {
        {
            if (activeImage && (mode == B_REDRAW_IMAGE || cline != buf->topLine || ccolumn != buf->currentColumn)) {
                if (draw_image_flag)
                    scr_clear();
                clearImage();
                loadImage(buf, IMG_FLAG_STOP);
                image_touch++;
                draw_image_flag = false;
            }
            redrawBuffer(buf);
        }
        cline = buf->topLine;
        ccolumn = buf->currentColumn;
    }
    if (buf->topLine == 0)
        buf->topLine = buf->firstLine;

    if (buf->need_reshape) {
        displayBuffer(buf, B_FORCE_REDRAW);
        return;
    }

    drawAnchorCursor(buf);

    Str msg = make_lastline_message(buf);
    if (buf->firstLine == 0) {
        Strcat_charp(msg, "\tNo Line");
    }
    tui_render_delayed_msg();
    scr_standout();
    tui_message(msg->ptr);
    scr_move(buf->cursorY + buf->rootY, buf->cursorX + buf->rootX);
    scr_standend();
    tty_set_title(conv_to_system(buf->buffername));
    tui_render_screen();
    if (activeImage && displayImage && buf->img && buf->image_loaded) {
        drawImage();
    }
    if (buf != save_current_buf) {
        saveBufferInfo();
        save_current_buf = buf;
    }
    if (mode == B_FORCE_REDRAW && (buf->check_url & CHK_URL)) {
        chkURLBuffer(buf);
        displayBuffer(buf, B_NORMAL);
    }
}
