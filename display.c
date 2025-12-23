#include "display.h"
#include "file.h"
#include "terms.h"
#include "history.h"
#include "message.h"
#include "buffer.h"
#include "anchor.h"
#include "maparea.h"
#include "tab.h"
#include "image.h"
#include "w3m_rc.h"
#include "ctrlcode.h"
#include "LineWriter.h"
#include <math.h>

static int image_touch = 0;
static bool draw_image_flag = false;

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
    if (getRuntime()->DecodeURL)
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

    if (getRuntime()->displayLink) {
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
    if (getRuntime()->displayLineInfo && buf->currentLine != NULL && buf->lastLine != NULL) {
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

static int
redrawLineRegion(struct Buffer* buf, struct Line* l, int i, int bpos, int epos)
{
    struct LineWriter g = { 0 };

    int j, pos, rcol, ncol, delta = 1;
    int column = buf->currentColumn;
    char* p;
    Lineprop* pr;
    Linecolor* pc;
    int bcol, ecol;
    struct Anchor* a;
    struct Url url;
    int k, vpos = -1;

    if (l == NULL)
        return 0;
    pos = columnPos(l, column);
    p = &(l->lineBuf[pos]);
    pr = &(l->propBuf[pos]);
    if (getRuntime()->useColor && l->colorBuf)
        pc = &(l->colorBuf[pos]);
    else
        pc = NULL;
    rcol = COLPOS(l, pos);
    bcol = bpos - pos;
    ecol = epos - pos;

    for (j = 0; rcol - column < buf->COLS && pos + j < l->len; j += delta) {
        if (getRuntime()->useVisitedColor && vpos <= pos + j && !(pr[j] & PE_VISITED)) {
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
        delta = wtf_len((wc_uchar*)&p[j]);
        ncol = COLPOS(l, pos + j + delta);
        if (ncol - column > buf->COLS)
            break;
        if (pc)
            do_color(&g, pc[j]);
        if (j >= bcol && j < ecol) {
            if (rcol < column) {
                screen_move(i, buf->rootX);
                for (rcol = column; rcol < ncol; rcol++)
                    addChar(&g, ' ', 0);
                continue;
            }
            screen_move(i, rcol - column + buf->rootX);
            if (p[j] == '\t') {
                for (; rcol < ncol; rcol++)
                    addChar(&g, ' ', 0);
            } else
                addMChar(&g, &p[j], pr[j], delta);
        }
        rcol = ncol;
    }
    endLine(&g);
    return rcol - column;
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
                if (getRuntime()->enable_inline_image && (l->propBuf[i] & PE_IMAGE)) {
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
                }
                x = (int)((rcol - column + buf->rootX) * getRuntime()->pixel_per_char);
                y = (int)(i * getRuntime()->pixel_per_line);
                sx = (int)((rcol - COLPOS(l, a->start.pos)) * getRuntime()->pixel_per_char);
                sy = (int)((l->linenumber - image->y) * getRuntime()->pixel_per_line);
                if (!getRuntime()->enable_inline_image) {
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
                    w = (int)(8 * getRuntime()->pixel_per_char - sx);
                if (image->height > 0)
                    h = image->height - sy;
                else
                    h = (int)(getRuntime()->pixel_per_line - sy);
                if (w > (int)((buf->rootX + buf->COLS) * getRuntime()->pixel_per_char - x))
                    w = (int)((buf->rootX + buf->COLS) * getRuntime()->pixel_per_char - x);
                if (h > (int)(LASTLINE() * getRuntime()->pixel_per_line - y))
                    h = (int)(LASTLINE() * getRuntime()->pixel_per_line - y);
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
    struct LineWriter g = { 0 };

    int j, pos, rcol, ncol, delta = 1;
    int column = buf->currentColumn;
    char* p;
    Lineprop* pr;
    Linecolor* pc;
    struct Anchor* a;
    struct Url url;
    int k, vpos = -1;

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
    if (getRuntime()->useColor && l->colorBuf)
        pc = &(l->colorBuf[pos]);
    else
        pc = NULL;
    rcol = COLPOS(l, pos);

    for (j = 0; rcol - column < buf->COLS && pos + j < l->len; j += delta) {
        if (getRuntime()->useVisitedColor && vpos <= pos + j && !(pr[j] & PE_VISITED)) {
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
        delta = wtf_len((wc_uchar*)&p[j]);
        ncol = COLPOS(l, pos + j + delta);
        if (ncol - column > buf->COLS)
            break;
        if (pc)
            do_color(&g, pc[j]);
        if (rcol < column) {
            for (rcol = column; rcol < ncol; rcol++)
                addChar(&g, ' ', 0);
            continue;
        }
        if (p[j] == '\t') {
            for (; rcol < ncol; rcol++)
                addChar(&g, ' ', 0);
        } else {
            addMChar(&g, &p[j], pr[j], delta);
        }
        rcol = ncol;
    }
    endLine(&g);
    if (rcol - column < buf->COLS)
        screen_clrtoeolx();
    return l;
}

static void
redrawNLine(struct Buffer* buf, int n)
{
    struct Line* l;
    int i;

    beginLine();

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
            // if (t == CurrentTab())
            //     EFFECT_ACTIVE_START;
            screen_wc_addstr_width(t->currentBuffer->buffername, t->x2 - t->x1 - l);
            // if (t == CurrentTab())
            //     EFFECT_ACTIVE_END;
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

    if (!(getRuntime()->activeImage && getRuntime()->displayImage && buf->img))
        return;
    screen_move(buf->cursorY + buf->rootY, buf->cursorX + buf->rootX);
    for (i = 0, l = buf->topLine; i < buf->LINES && l; i++, l = l->next) {
        if (i >= buf->LINES - n || i < -n)
            redrawLineImage(buf, l, i + buf->rootY);
    }
    getAllImage(buf);
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

    // reshape
    // if (buf->width != INIT_BUFFER_WIDTH && (is_html_type(buf->type) || getRuntime()->FoldLine)) {
    //     buf->need_reshape = true;
    //     reshapeBuffer(buf);
    // }

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
    static struct Line* cline = NULL;
    static int ccolumn = -1;
    if (mode == B_FORCE_REDRAW //
        || mode == B_SCROLL //
        || mode == B_REDRAW_IMAGE //
        || cline != buf->topLine //
        || ccolumn != buf->currentColumn) {

        if (getRuntime()->activeImage && (mode == B_REDRAW_IMAGE || cline != buf->topLine || ccolumn != buf->currentColumn)) {
            if (draw_image_flag) {
                tty_clear();
                screen_clear();
            }
            clearImage();
            loadImage(buf, IMG_FLAG_STOP);
            image_touch++;
            draw_image_flag = false;
        }
        redrawNLine(buf, LASTLINE());

        cline = buf->topLine;
        ccolumn = buf->currentColumn;
    }

    if (buf->topLine == NULL)
        buf->topLine = buf->firstLine;

    drawAnchorCursor(buf);

    Str msg = make_lastline_message(buf);
    if (buf->firstLine == NULL) {
        Strcat_charp(msg, "\tNo Line");
    }
    displayDelayedMessage();
    screen_standout();
    message(msg->ptr, buf->cursorX + buf->rootX, buf->cursorY + buf->rootY);
    screen_standend();
    term_title(conv_to_system(buf->buffername));
    tty_refresh();

    if (getRuntime()->activeImage && getRuntime()->displayImage && buf->img) {
        if (buf->image_loaded) {
            drawImage(buf);
        }
    }
}
