#include "display.h"
#include "AnchorList.h"
#include "Anchor.h"
#include "alloc.h"
#include "screen_effects.h"
#include "image.h"
#include "maparea.h"
#include "ui.h"
#include "symbol.h"
#include "w3m.h"
#include "history.h"
#include "ctrlcode.h"
#include "buffer.h"
#include "screen.h"
#include "frame.h"
#include "putc.h"
#include <assert.h>
#include <math.h>
#include <string.h>

#include <wc.h>
#include <wtf.h>

int displayLink = (false);
int displayLineInfo = (false);
int FoldLine = (false);
int enable_inline_image = false;
int displayImage = (true);

double pixel_per_char = (DEFAULT_PIXEL_PER_CHAR);
int pixel_per_char_i = (DEFAULT_PIXEL_PER_CHAR);
int set_pixel_per_char = (false);
double pixel_per_line = (DEFAULT_PIXEL_PER_LINE);
int pixel_per_line_i = (DEFAULT_PIXEL_PER_LINE);
int set_pixel_per_line = (false);

static struct LineList* cline = NULL;
static int ccolumn = -1;
static int image_touch = 0;
static bool draw_image_flag = false;

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

static struct LineList* redrawLine(struct UI ui, struct Buffer* buf, struct LineList* l, int i)
{
    int j, pos, rcol, ncol, delta = 1;
    int column = buf->currentColumn;
    char* p;
    Lineprop* pr;
    Linecolor* pc;
    struct Anchor* a;
    struct Url url;
    int k, vpos = -1;

    if (l == NULL) {
        return NULL;
    }
    vt_move(ui.vt, i, 0);

    vt_move(ui.vt, i, ui.viewport.offset.x);
    if (l->l.width < 0)
        l->l.width = COLPOS(&l->l, l->l.len);
    if (l->l.len == 0 || l->l.width - 1 < column) {
        vt_clrtoeolx(ui.vt);
        return l;
    }
    /* need_clrtoeol(); */
    pos = columnPos(&l->l, column);
    p = &(l->l.lineBuf[pos]);
    pr = &(l->l.propBuf[pos]);
    if (useColor && l->l.colorBuf)
        pc = &(l->l.colorBuf[pos]);
    else
        pc = NULL;
    rcol = COLPOS(&l->l, pos);

    for (j = 0; rcol - column < buf->width && pos + j < l->l.len; j += delta) {
        if (useVisitedColor && vpos <= pos + j && !(pr[j] & PE_VISITED)) {
            a = retrieveAnchor(buf->href, (struct BufferPoint) { .line = l->linenumber, .pos = pos + j });
            if (a) {
                url = parseUrl(a->url, baseURL(buf));
                if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
                    for (k = a->start.pos; k < a->end.pos; k++)
                        pr[k - pos] |= PE_VISITED;
                }
                vpos = a->end.pos;
            }
        }
        delta = wtf_len((wc_uchar*)&p[j]);
        ncol = COLPOS(&l->l, pos + j + delta);
        if (ncol - column > buf->width)
            break;
        if (pc)
            vt_do_color(ui.vt, pc[j]);
        if (rcol < column) {
            for (rcol = column; rcol < ncol; rcol++)
                vt_addChar(ui.vt, ' ', 0, ui.use_graphic);
            continue;
        }
        if (p[j] == '\t') {
            for (; rcol < ncol; rcol++)
                vt_addChar(ui.vt, ' ', 0, ui.use_graphic);
        } else {
            vt_addMChar(ui.vt, &p[j], pr[j], delta, ui.use_graphic);
        }
        rcol = ncol;
    }

    vt_line_end(ui.vt);
    if (rcol - column < ui.viewport.size.x)
        vt_clrtoeolx(ui.vt);
    return l;
}

static struct LineList* redrawLineImage(struct UI ui, struct Buffer* buf, struct LineList* l, int i)
{
    int j, pos, rcol;
    int column = buf->currentColumn;
    struct Anchor* a;
    int x, y, sx, sy, w, h;

    if (l == NULL)
        return NULL;
    if (l->l.width < 0)
        l->l.width = COLPOS(&l->l, l->l.len);
    if (l->l.len == 0 || l->l.width - 1 < column)
        return l;
    pos = columnPos(&l->l, column);
    rcol = COLPOS(&l->l, pos);
    for (j = 0; rcol - column < ui.viewport.size.x && pos + j < l->l.len; j++) {
        if (rcol - column < 0) {
            rcol = COLPOS(&l->l, pos + j + 1);
            continue;
        }
        a = retrieveAnchor(buf->img, (struct BufferPoint) { .line = l->linenumber, .pos = pos + j });
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
                x = (int)((rcol - column + ui.viewport.offset.x) * pixel_per_char);
                y = (int)(i * pixel_per_line);
                sx = (int)((rcol - COLPOS(&l->l, a->start.pos)) * pixel_per_char);
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
                if (w > (int)((ui.viewport.offset.x + ui.viewport.size.x) * pixel_per_char - x))
                    w = (int)((ui.viewport.offset.x + ui.viewport.size.x) * pixel_per_char - x);
                if (h > (int)((ui.vt->ROWS - 1) * pixel_per_line - y))
                    h = (int)((ui.vt->ROWS - 1) * pixel_per_line - y);
                addImage(cache, x, y, sx, sy, w, h);
                image->touch = image_touch;
                draw_image_flag = true;
            }
        }
        rcol = COLPOS(&l->l, pos + j + 1);
    }
    return l;
}

static void redrawNLine(struct UI ui, struct Buffer* buf, int n)
{
    if (useColor) {
        EFFECT_ANCHOR_END_C(ui.vt);
        vt_setbcolor(ui.vt, bg_color);
    }

    struct LineList* l;
    int i;
    for (i = 0, l = topLine(buf); i < ui.viewport.size.y; i++, l = l->next) {
        if (i >= ui.viewport.size.y - n || i < -n)
            l = redrawLine(ui, buf, l, i + ui.viewport.offset.y);
        if (l == NULL)
            break;
    }
    if (n > 0) {
        vt_move(ui.vt, i + ui.viewport.offset.y, 0);
        vt_clrtobotx(ui.vt);
    }

    if (!(activeImage && displayImage && buf->img))
        return;

    // vt_move(ui.vt, buf->cursorY + ui.viewport.offset.y, buf->cursorX + ui.viewport.offset.x);
    for (i = 0, l = topLine(buf); i < ui.viewport.size.y && l; i++, l = l->next) {
        if (i >= ui.viewport.size.y - n || i < -n)
            redrawLineImage(ui, buf, l, i + ui.viewport.offset.y);
    }
    getAllImage(buf);
}

void bufToScreen(struct UI ui, struct Buffer* buf)
{
    if (buf->width == 0) {
        reshapeBuffer(buf, ui.viewport.size.x);
    }

    if (activeImage && (cline != topLine(buf) || ccolumn != buf->currentColumn)) {
        if (draw_image_flag) {
            vt_clear(getScreen());
            // termClear(ttyWriter());
        }
        clearImage();
        loadImage(buf, IMG_FLAG_STOP, false);
        image_touch++;
        draw_image_flag = false;
    }
    redrawNLine(ui, buf, getScreen()->ROWS - 1);
    cline = topLine(buf);
    ccolumn = buf->currentColumn;

    if (topLine(buf) == NULL)
        buf->topLineIndex = buf->firstLine->linenumber;
}

static int redrawLineRegion(struct UI ui, struct Buffer* buf, struct LineList* l, int i, int bpos, int epos)
{
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
    pos = columnPos(&l->l, column);
    p = &(l->l.lineBuf[pos]);
    pr = &(l->l.propBuf[pos]);
    if (useColor && l->l.colorBuf)
        pc = &(l->l.colorBuf[pos]);
    else
        pc = NULL;
    rcol = COLPOS(&l->l, pos);
    bcol = bpos - pos;
    ecol = epos - pos;

    for (j = 0; rcol - column < ui.viewport.size.x && pos + j < l->l.len; j += delta) {
        if (useVisitedColor && vpos <= pos + j && !(pr[j] & PE_VISITED)) {
            a = retrieveAnchor(buf->href, (struct BufferPoint) { .line = l->linenumber, .pos = pos + j });
            if (a) {
                url = parseUrl(a->url, baseURL(buf));
                if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
                    for (k = a->start.pos; k < a->end.pos; k++)
                        pr[k - pos] |= PE_VISITED;
                }
                vpos = a->end.pos;
            }
        }
        delta = wtf_len((wc_uchar*)&p[j]);
        ncol = COLPOS(&l->l, pos + j + delta);
        if (ncol - column > ui.viewport.size.x)
            break;
        if (pc)
            vt_do_color(ui.vt, pc[j]);
        if (j >= bcol && j < ecol) {
            if (rcol < column) {
                vt_move(ui.vt, i, ui.viewport.offset.x);
                for (rcol = column; rcol < ncol; rcol++)
                    vt_addChar(ui.vt, ' ', 0, ui.use_graphic);
                continue;
            }
            vt_move(ui.vt, i, rcol - column + ui.viewport.offset.x);
            if (p[j] == '\t') {
                for (; rcol < ncol; rcol++)
                    vt_addChar(ui.vt, ' ', 0, ui.use_graphic);
            } else
                vt_addMChar(ui.vt, &p[j], pr[j], delta, ui.use_graphic);
        }
        rcol = ncol;
    }

    vt_line_end(ui.vt);
    return rcol - column;
}

static void _drawAnchorCursor(struct UI ui, struct Buffer* buf,
    int hseq, int prevhseq,
    int tline, struct LineList* l, struct Anchor* an, bool active)
{
    if (hseq >= 0 && an->hseq == hseq) {
        int start_pos = an->start.pos;
        int end_pos = an->end.pos;
        for (int i = an->start.pos; i < an->end.pos; i++) {
            if (enable_inline_image && (l->l.propBuf[i] & PE_IMAGE)) {
                if (start_pos == i)
                    start_pos = i + 1;
                else if (end_pos == an->end.pos)
                    end_pos = i - 1;
            }
            if (l->l.propBuf[i] & (PE_IMAGE | PE_ANCHOR | PE_FORM)) {
                if (active)
                    l->l.propBuf[i] |= PE_ACTIVE;
                else
                    l->l.propBuf[i] &= ~PE_ACTIVE;
            }
        }
        if (active && start_pos < end_pos)
            redrawLineRegion(ui, buf, l, l->linenumber - tline + ui.viewport.offset.y, start_pos, end_pos);
    } else if (prevhseq >= 0 && an->hseq == prevhseq) {
        if (active)
            redrawLineRegion(ui, buf, l, l->linenumber - tline + ui.viewport.offset.y, an->start.pos, an->end.pos);
    }
}

static void
drawAnchorCursor0(struct UI ui, struct Buffer* buf,
    struct AnchorList* al, int hseq, int prevhseq, int tline, int eline, bool active)
{
    struct LineList* l = topLine(buf);
    for (int j = 0; j < al->nanchor; j++) {
        struct Anchor* an = &al->anchors[j];
        if (an->start.line < tline)
            continue;
        if (an->start.line >= eline)
            return;
        for (; l; l = l->next) {
            if (l->linenumber == an->start.line)
                break;
        }
        _drawAnchorCursor(ui, buf, hseq, prevhseq, tline, l, an, active);
    }
}

static int currentAnchorHseq(struct Buffer* buf)
{
    struct Anchor* an = retrieveCurrentAnchor(buf);
    if (!an)
        an = retrieveCurrentMap(buf);
    return an ? an->hseq : -1;
}

void drawAnchorCursor(struct UI ui, struct Buffer* buf)
{
    if (!buf->firstLine || !buf->hmarklist)
        return;
    if (!buf->href && !buf->formitem)
        return;

    int tline = topLine(buf)->linenumber;
    int eline = tline + ui.viewport.size.y;
    int hseq = currentAnchorHseq(buf);
    int prevhseq = buf->hmarklist->prevhseq;
    if (buf->href) {
        drawAnchorCursor0(ui, buf, buf->href, hseq, prevhseq, tline, eline, 1);
        drawAnchorCursor0(ui, buf, buf->href, hseq, -1, tline, eline, 0);
    }
    if (buf->formitem) {
        drawAnchorCursor0(ui, buf, buf->formitem, hseq, prevhseq, tline, eline, 1);
        drawAnchorCursor0(ui, buf, buf->formitem, hseq, -1, tline, eline, 0);
    }
    buf->hmarklist->prevhseq = hseq;
}
