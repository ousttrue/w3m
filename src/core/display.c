#include "display.h"
#include "alloc.h"
#include "screen_effects.h"
#include "image.h"
#include "etc.h"
#include "map.h"
#include "ui.h"
#include "symbol.h"
#include "file.h"
#include "w3m.h"
#include "history.h"
#include "ctrlcode.h"
#include "buffer.h"
#include "screen.h"
#include "frame.h"
#include "putc.h"
#include <assert.h>
#include <math.h>

int displayLink = (false);
int displayLineInfo = (false);
int FoldLine = (false);
int showLineNum = (false);
int enable_inline_image = false;
int displayImage = (true);

double pixel_per_char = (DEFAULT_PIXEL_PER_CHAR);
int pixel_per_char_i = (DEFAULT_PIXEL_PER_CHAR);
int set_pixel_per_char = (false);
double pixel_per_line = (DEFAULT_PIXEL_PER_LINE);
int pixel_per_line_i = (DEFAULT_PIXEL_PER_LINE);
int set_pixel_per_line = (false);

static Line* cline = NULL;
static int ccolumn = -1;
static int image_touch = 0;
static bool draw_image_flag = false;

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

Str make_lastline_message(Buffer* buf)
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

void bufToScreen(struct VirtualTerm* vt, Buffer* buf, bool use_graphic)
{
    if (buf->width == 0) {
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
            vt_clear(getScreen());
            // termClear(ttyWriter());
        }
        clearImage();
        loadImage(buf, IMG_FLAG_STOP);
        image_touch++;
        draw_image_flag = false;
    }
    redrawNLine(buf, getScreen()->ROWS - 1, use_graphic);
    cline = buf->topLine;
    ccolumn = buf->currentColumn;
    // }
    if (buf->topLine == NULL)
        buf->topLine = buf->firstLine;
}

static void
drawAnchorCursor0(Buffer* buf, AnchorList* al, int hseq, int prevhseq,
    int tline, int eline, int active, bool use_graphic)
{
    int i;

    Line* l = buf->topLine;
    for (int j = 0; j < al->nanchor; j++) {
        Anchor* an = &al->anchors[j];
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
                    start_pos, end_pos, use_graphic);
        } else if (prevhseq >= 0 && an->hseq == prevhseq) {
            if (active)
                redrawLineRegion(buf, l, l->linenumber - tline + buf->rootY,
                    an->start.pos, an->end.pos, use_graphic);
        }
    }
}

void drawAnchorCursor(Buffer* buf, bool use_graphic)
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
        drawAnchorCursor0(buf, buf->href, hseq, prevhseq, tline, eline, 1, use_graphic);
        drawAnchorCursor0(buf, buf->href, hseq, -1, tline, eline, 0, use_graphic);
    }
    if (buf->formitem) {
        drawAnchorCursor0(buf, buf->formitem, hseq, prevhseq, tline, eline, 1, use_graphic);
        drawAnchorCursor0(buf, buf->formitem, hseq, -1, tline, eline, 0, use_graphic);
    }
    buf->hmarklist->prevhseq = hseq;
}

void redrawNLine(Buffer* buf, int n, bool use_graphic)
{
    struct VirtualTerm* vt = getScreen();
    Line* l;
    int i;

    if (useColor) {
        EFFECT_ANCHOR_END_C(vt);
        vt_setbcolor(vt, bg_color);
    }

    for (i = 0, l = buf->topLine; i < buf->LINES; i++, l = l->next) {
        if (i >= buf->LINES - n || i < -n)
            l = redrawLine(buf, l, i + buf->rootY, use_graphic);
        if (l == NULL)
            break;
    }
    if (n > 0) {
        vt_move(vt, i + buf->rootY, 0);
        vt_clrtobotx(vt);
    }

    if (!(activeImage && displayImage && buf->img))
        return;
    vt_move(vt, buf->cursorY + buf->rootY, buf->cursorX + buf->rootX);
    for (i = 0, l = buf->topLine; i < buf->LINES && l; i++, l = l->next) {
        if (i >= buf->LINES - n || i < -n)
            redrawLineImage(buf, l, i + buf->rootY);
    }
    getAllImage(buf);
}

Line* redrawLine(Buffer* buf, Line* l, int i, bool use_graphic)
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
    vt_move(vt, i, 0);
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
        vt_addstr(vt, tmp);
    }
    vt_move(vt, i, buf->rootX);
    if (l->width < 0)
        l->width = COLPOS(l, l->len);
    if (l->len == 0 || l->width - 1 < column) {
        vt_clrtoeolx(vt);
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
            vt_do_color(vt, pc[j]);
        if (rcol < column) {
            for (rcol = column; rcol < ncol; rcol++)
                vt_addChar(vt, ' ', 0, use_graphic);
            continue;
        }
        if (p[j] == '\t') {
            for (; rcol < ncol; rcol++)
                vt_addChar(vt, ' ', 0, use_graphic);
        } else {
            vt_addMChar(vt, &p[j], pr[j], delta, use_graphic);
        }
        rcol = ncol;
    }

    vt_line_end(vt);
    if (rcol - column < buf->COLS)
        vt_clrtoeolx(vt);
    return l;
}

Line* redrawLineImage(Buffer* buf, Line* l, int i)
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
                draw_image_flag = true;
            }
        }
        rcol = COLPOS(l, pos + j + 1);
    }
    return l;
}

int redrawLineRegion(Buffer* buf, Line* l, int i, int bpos, int epos, bool use_graphic)
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
            vt_do_color(vt, pc[j]);
        if (j >= bcol && j < ecol) {
            if (rcol < column) {
                vt_move(vt, i, buf->rootX);
                for (rcol = column; rcol < ncol; rcol++)
                    vt_addChar(vt, ' ', 0, use_graphic);
                continue;
            }
            vt_move(vt, i, rcol - column + buf->rootX);
            if (p[j] == '\t') {
                for (; rcol < ncol; rcol++)
                    vt_addChar(vt, ' ', 0, use_graphic);
            } else
                vt_addMChar(vt, &p[j], pr[j], delta, use_graphic);
        }
        rcol = ncol;
    }

    vt_line_end(vt);
    return rcol - column;
}
