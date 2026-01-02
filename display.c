#include "display.h"
#include "document.h"
#include "file.h"
#include "screen.h"
#include "history.h"
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

static int
redrawLineRegion(struct Buffer* buf, struct Line* l, int i, int bpos, int epos)
{
    struct LineWriter g = { 0 };

    int j, pos, rcol, ncol, delta = 1;
    int column = buf->doc.currentColumn;
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

    for (j = 0; rcol - column < buf->doc.COLS && pos + j < l->len; j += delta) {
        if (getRuntime()->useVisitedColor && vpos <= pos + j && !(pr[j] & PE_VISITED)) {
            a = retrieveAnchor(buf->doc.href, l->linenumber, pos + j);
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
        if (ncol - column > buf->doc.COLS)
            break;
        if (pc)
            do_color(&g, pc[j]);
        if (j >= bcol && j < ecol) {
            if (rcol < column) {
                screen_move((struct Vec2) { .y = i, .x = buf->doc.rootX });
                for (rcol = column; rcol < ncol; rcol++)
                    addChar(&g, ' ', 0);
                continue;
            }
            screen_move((struct Vec2) { .y = i, .x = rcol - column + buf->doc.rootX });
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

void drawAnchorCursor0(struct Buffer* buf, struct AnchorList* al, int hseq, int prevhseq, int tline, int eline, int active)
{
    int i, j;
    struct Line* l;
    struct Anchor* an;

    l = buf->doc.topLine;
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
                redrawLineRegion(buf, l, l->linenumber - tline + buf->doc.rootY,
                    start_pos, end_pos);
        } else if (prevhseq >= 0 && an->hseq == prevhseq) {
            if (active)
                redrawLineRegion(buf, l, l->linenumber - tline + buf->doc.rootY,
                    an->start.pos, an->end.pos);
        }
    }
}

void drawAnchorCursor(struct Buffer* buf)
{
    struct Anchor* an;
    int hseq, prevhseq;
    int tline, eline;

    if (!buf->doc.firstLine || !buf->doc.hmarklist)
        return;
    if (!buf->doc.href && !buf->doc.formitem)
        return;

    an = retrieveCurrentAnchor(buf);
    if (!an)
        an = retrieveCurrentMap(buf);
    if (an)
        hseq = an->hseq;
    else
        hseq = -1;
    tline = buf->doc.topLine->linenumber;
    eline = tline + buf->doc.LINES;
    prevhseq = buf->doc.hmarklist->prevhseq;

    if (buf->doc.href) {
        drawAnchorCursor0(buf, buf->doc.href, hseq, prevhseq, tline, eline, 1);
        drawAnchorCursor0(buf, buf->doc.href, hseq, -1, tline, eline, 0);
    }
    if (buf->doc.formitem) {
        drawAnchorCursor0(buf, buf->doc.formitem, hseq, prevhseq, tline, eline, 1);
        drawAnchorCursor0(buf, buf->doc.formitem, hseq, -1, tline, eline, 0);
    }
    buf->doc.hmarklist->prevhseq = hseq;
}

static struct Line*
redrawLineImage(struct Document* doc, struct Line* l, int i, struct Url* base_url)
{
    int j, pos, rcol;
    int column = doc->currentColumn;
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
    for (j = 0; rcol - column < doc->COLS && pos + j < l->len; j++) {
        if (rcol - column < 0) {
            rcol = COLPOS(l, pos + j + 1);
            continue;
        }
        a = retrieveAnchor(doc->img, l->linenumber, pos + j);
        if (a && a->image && a->image->touch < image_touch) {
            struct Image* image = a->image;
            image->cache = getImage(image, base_url, doc->image_flag);
            struct ImageCache* cache = image->cache;
            if (cache) {
                if ((image->width < 0 && cache->width > 0) || (image->height < 0 && cache->height > 0)) {
                    image->width = cache->width;
                    image->height = cache->height;
                }
                x = (int)((rcol - column + doc->rootX) * getRuntime()->pixel_per_char);
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
                if (w > (int)((doc->rootX + doc->COLS) * getRuntime()->pixel_per_char - x))
                    w = (int)((doc->rootX + doc->COLS) * getRuntime()->pixel_per_char - x);
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

static void
redrawNLine(struct Document* doc, int n, struct Url* base_url)
{
    beginLine();

    if (nTab() > 1) {
        screen_move((struct Vec2) { 0 });
        screen_clrtoeolx();
        for (struct TabBuffer* t = FirstTab(); t; t = t->nextTab) {
            screen_move((struct Vec2) { .y = t->y, .x = t->x1 });
            if (t == CurrentTab())
                screen_bold();
            screen_addch('[', 1);
            int l = t->x2 - t->x1 - 1 - get_strwidth(t->currentBuffer->doc.title);
            if (l < 0)
                l = 0;
            if (l / 2 > 0)
                screen_wc_addnstr_sup(" ", l / 2);
            // if (t == CurrentTab())
            //     EFFECT_ACTIVE_START;
            screen_wc_addstr_width(t->currentBuffer->doc.title, t->x2 - t->x1 - l);
            // if (t == CurrentTab())
            //     EFFECT_ACTIVE_END;
            if ((l + 1) / 2 > 0)
                screen_wc_addnstr_sup(" ", (l + 1) / 2);
            screen_move((struct Vec2) { .y = t->y, .x = t->x2 });
            screen_addch(']', 1);
            if (t == CurrentTab())
                screen_boldend();
        }
        screen_move((struct Vec2) { .y = LastTab()->y + 1, .x = 0 });
        for (int i = 0; i < TTY_COLS(); i++)
            screen_addch('~', 1);
    }
    int i = 0;
    for (struct Line* l = doc->topLine; i < doc->LINES; i++, l = l->next) {
        if (i >= doc->LINES - n || i < -n)
            l = doc_redrawLine(doc, l, i + doc->rootY, base_url);
        if (l == NULL)
            break;
    }
    if (n > 0) {
        screen_move((struct Vec2) { .y = i + doc->rootY, .x = 0 });
        screen_clrtobotx();
    }

    if (!(getRuntime()->activeImage && getRuntime()->displayImage && doc->img))
        return;
    screen_move((struct Vec2) { .y = doc->cursorY + doc->rootY, .x = doc->cursorX + doc->rootX });

    i = 0;
    for (struct Line* l = doc->topLine; i < doc->LINES && l; i++, l = l->next) {
        if (i >= doc->LINES - n || i < -n)
            redrawLineImage(doc, l, i + doc->rootY, base_url);
    }
}

void screen_from_lines(struct Document* doc, struct Url* base_url)
{
    if (getRuntime()->activeImage) {
        if (draw_image_flag) {
            tty_clear();
            screen_clear();
        }
        clearImage();
        loadImage(IMG_FLAG_STOP);
        image_touch++;
        draw_image_flag = false;
    }
    redrawNLine(doc, LASTLINE(), base_url);
}

void bufferPosition(struct Buffer* buf)
{
    // doc.rootX
    if (getRuntime()->showLineNum) {
        if (buf->doc.lastLine && buf->doc.lastLine->real_linenumber > 0)
            buf->doc.rootX = (int)(log(buf->doc.lastLine->real_linenumber + 0.1)
                                 / log(10))
                + 2;
        if (buf->doc.rootX < 5)
            buf->doc.rootX = 5;
        if (buf->doc.rootX > TTY_COLS())
            buf->doc.rootX = TTY_COLS();
    } else {
        buf->doc.rootX = 0;
    }
    buf->doc.COLS = TTY_COLS() - buf->doc.rootX;

    // doc.rootY
    int ny = 0;
    if (nTab() > 1) {
        calcTabPos();
        ny = LastTab()->y + 2;
        if (ny > LASTLINE())
            ny = LASTLINE();
    }
    if (buf->doc.rootY != ny || buf->doc.LINES != LASTLINE() - ny) {
        buf->doc.rootY = ny;
        buf->doc.LINES = LASTLINE() - ny;
        doc_arrangeCursor(&buf->doc);
    }
}
