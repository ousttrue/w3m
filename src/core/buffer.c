#include "buffer.h"
#include "ui.h"
#include "buffer_list.h"
#include "line.h"
#include "regex.h"
#include "HttpRequest.h"
#include "maparea.h"
#include "runtime.h"
#include "html_quote.h"
#include "cookie.h"
#include "convertline.h"
#include "quote.h"
#include "screen_effects.h"
#include "display.h"
#include "form.h"
#include "ui.h"
#include "w3m.h"
#include "image.h"
#include "event_poller.h"
#include "screen.h"
#include "ctrlcode.h"
#include "istream.h"
#include "buffer_loader.h"
#include "alloc.h"
#include "Anchor.h"
#include "AnchorList.h"
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>

#include <wc.h>
#include <wtf.h>

int nextpage_topline = (false);

int REV_LB[MAX_LB] = {
    LB_N_INFO,
    LB_INFO,
    LB_N_SOURCE,
};

/*
 * struct Buffer creation
 */
struct Buffer*
newBuffer()
{
    struct Buffer* n = New(struct Buffer);
    memset(n, 0, sizeof(struct Buffer));
    n->width = 0;
    n->currentURL.scheme = SCM_UNKNOWN;
    n->document.baseURL = 0;
    n->document.baseTarget = 0;
    n->buffername = "";
    n->bufferprop = BP_NORMAL;
    n->clone = New(int);
    *n->clone = 1;
    n->trbyte = 0;
    n->ssl_certificate = 0;
    n->auto_detect = WcOption.auto_detect;
    n->check_url = MarkAllPages;
    return n;
}

/*
 * Create null buffer
 */
struct Buffer*
nullBuffer(void)
{
    struct Buffer* b;

    b = newBuffer();
    b->buffername = "*Null*";
    return b;
}

/*
 * clearBuffer: clear buffer content
 */
void clearBuffer(struct Buffer* buf)
{
    // charset !
    // buf->document = (struct Document) { 0 };
    buf->document.topLineIndex = 0;
    buf->document.currentLineIndex = 0;
}

/*
 * discardBuffer: free buffer structure
 */

void discardBuffer(struct Buffer* buf)
{
    deleteImage(buf);
    clearBuffer(buf);
    for (int i = 0; i < MAX_LB; i++) {
        struct Buffer* b = buf->linkBuffer[i];
        if (b == 0)
            continue;
        b->linkBuffer[REV_LB[i]] = 0;
    }
    if (buf->savecache)
        unlink(buf->savecache);
    if (--(*buf->clone))
        return;
    if (buf->sourcefile && (contentTypeIsImage(buf->content_type))) {
        if (buf->real_scheme != SCM_LOCAL)
            unlink(buf->sourcefile);
    }
    if (buf->mailcap_source)
        unlink(buf->mailcap_source);
}

/*
 * namedBuffer: Select buffer which have specified name
 */
struct Buffer*
namedBuffer(struct Buffer* first, char* name)
{
    struct Buffer* buf;

    if (!strcmp(first->buffername, name)) {
        return first;
    }
    for (buf = first; buf->nextBuffer != 0; buf = buf->nextBuffer) {
        if (!strcmp(buf->nextBuffer->buffername, name)) {
            return buf->nextBuffer;
        }
    }
    return 0;
}

/*
 * deleteBuffer: delete buffer
 */
struct Buffer*
deleteBuffer(struct Buffer* first, struct Buffer* delbuf)
{
    struct Buffer *buf, *b;

    if (first == delbuf && first->nextBuffer != 0) {
        buf = first->nextBuffer;
        discardBuffer(first);
        return buf;
    }
    if ((buf = prevBuffer(first, delbuf)) != 0) {
        b = buf->nextBuffer;
        buf->nextBuffer = b->nextBuffer;
        discardBuffer(b);
    }
    return first;
}

/*
 * replaceBuffer: replace buffer
 */
struct Buffer*
replaceBuffer(struct Buffer* first, struct Buffer* delbuf, struct Buffer* newbuf)
{
    struct Buffer* buf;

    if (delbuf == 0) {
        newbuf->nextBuffer = first;
        return newbuf;
    }
    if (first == delbuf) {
        newbuf->nextBuffer = delbuf->nextBuffer;
        discardBuffer(delbuf);
        return newbuf;
    }
    if (delbuf && (buf = prevBuffer(first, delbuf))) {
        buf->nextBuffer = newbuf;
        newbuf->nextBuffer = delbuf->nextBuffer;
        discardBuffer(delbuf);
        return first;
    }
    newbuf->nextBuffer = first;
    return newbuf;
}

struct Buffer*
nthBuffer(struct Buffer* firstbuf, int n)
{
    int i;
    struct Buffer* buf = firstbuf;

    if (n < 0)
        return firstbuf;
    for (i = 0; i < n; i++) {
        if (buf == 0)
            return 0;
        buf = buf->nextBuffer;
    }
    return buf;
}

static void
writeBufferName(struct Buffer* buf, int n)
{
    int all = buf->document.allLine;
    if (all == 0 && lastLine(&buf->document) != 0)
        all = lastLine(&buf->document)->linenumber;
    vt_move(getScreen(), n, 0);

    Str msg = Sprintf("<%s> [%d lines]", buf->buffername, all);
    if (buf->filename != 0) {
        switch (buf->currentURL.scheme) {
        case SCM_LOCAL:
        case SCM_LOCAL_CGI:
            // if (strcmp(buf->currentURL.file, "-")) {
            //     Strcat_char(msg, ' ');
            //     Strcat_charp(msg, conv_from_system(buf->currentURL.real_file));
            // }
            break;
        case SCM_UNKNOWN:
        case SCM_MISSING:
            break;
        default:
            Strcat_char(msg, ' ');
            Strcat(msg, parsedURL2Str(&buf->currentURL));
            break;
        }
    }
    vt_addnstr_sup(getScreen(), msg->ptr, getScreen()->COLS - 1);
}

/*
 * gotoLine: go to line number
 */
void gotoLine(struct Buffer* buf, int n)
{
    char msg[36];
    struct LineList* l = buf->document.firstLine;
    if (l == 0)
        return;
    if (l->linenumber > n) {
        /* FIXME: gettextize? */
        sprintf(msg, "First line is #%ld", l->linenumber);
        set_delayed_message(msg);
        buf->document.topLineIndex = buf->document.currentLineIndex = l->linenumber;
        return;
    }
    if (lastLine(&buf->document)->linenumber < n) {
        l = lastLine(&buf->document);
        /* FIXME: gettextize? */
        sprintf(msg, "Last line is #%ld", lastLine(&buf->document)->linenumber);
        set_delayed_message(msg);
        buf->document.currentLineIndex = l->linenumber;
        buf->document.topLineIndex = lineSkip(buf, currentLine(&buf->document), -(getScreen()->ROWS - 1), false)->linenumber;
        return;
    }
    for (; l != 0; l = l->next) {
        if (l->linenumber >= n) {
            buf->document.currentLineIndex = l->linenumber;
            if (n < topLine(&buf->document)->linenumber || topLine(&buf->document)->linenumber + getScreen()->ROWS <= n)
                buf->document.topLineIndex = lineSkip(buf, l, -(getScreen()->ROWS + 1) / 2, false)->linenumber;
            break;
        }
    }
}

// /*
//  * gotoRealLine: go to real line number
//  */
// void gotoRealLine(struct Buffer* buf, int n)
// {
//     char msg[36];
//     struct Line* l = buf->firstLine;
//
//     if (l == 0)
//         return;
//     if (l->real_linenumber > n) {
//         /* FIXME: gettextize? */
//         sprintf(msg, "First line is #%ld", l->real_linenumber);
//         set_delayed_message(msg);
//         topLine(&buf->document) = currentLine(&buf->document) = l;
//         return;
//     }
//     if (lastLine(&buf->document)->real_linenumber < n) {
//         l = lastLine(&buf->document);
//         /* FIXME: gettextize? */
//         sprintf(msg, "Last line is #%ld", lastLine(&buf->document)->real_linenumber);
//         set_delayed_message(msg);
//         currentLine(&buf->document) = l;
//         topLine(&buf->document) = lineSkip(buf, currentLine(&buf->document), -(getScreen()->ROWS - 1),
//             false);
//         return;
//     }
//     for (; l != 0; l = l->next) {
//         if (l->real_linenumber >= n) {
//             currentLine(&buf->document) = l;
//             if (n < topLine(&buf->document)->real_linenumber || topLine(&buf->document)->real_linenumber + getScreen()->ROWS <= n)
//                 topLine(&buf->document) = lineSkip(buf, l, -(getScreen()->ROWS + 1) / 2, false);
//             break;
//         }
//     }
// }

static struct Buffer*
listBuffer(struct Buffer* top, struct Buffer* current)
{
    struct VirtualTerm* vt = getScreen();
    int i, c = 0;
    struct Buffer* buf = top;

    vt_move(vt, 0, 0);
    if (useColor) {
        vt_setfcolor(vt, basic_color);
        vt_setbcolor(vt, bg_color);
    }
    vt_clrtobotx(vt);
    for (i = 0; i < getScreen()->ROWS - 1; i++) {
        if (buf == current) {
            c = i;
            vt_standout(vt);
        }
        writeBufferName(buf, i);
        if (buf == current) {
            vt_standend(vt);
            vt_clrtoeolx(vt);
            vt_move(vt, i, 0);
            vt_toggle_stand(vt);
        } else
            vt_clrtoeolx(vt);
        if (buf->nextBuffer == 0) {
            vt_move(vt, i + 1, 0);
            vt_clrtobotx(vt);
            break;
        }
        buf = buf->nextBuffer;
    }
    vt_standout(vt);
    /* FIXME: gettextize? */
    message(getUI(), MSG_INFO, "struct Buffer selection mode: SPC for select / D for delete buffer");
    vt_standend(vt);
    vt_move(vt, c, 0);
    // refresh(ttyWriter());
    return buf->nextBuffer;
}

/*
 * Select buffer visually
 */
struct Buffer*
selectBuffer(struct Buffer* firstbuf, struct Buffer* currentbuf, char* selectchar)
{
    struct VirtualTerm* vt = getScreen();
    int i, cpoint, /* Current struct Buffer Number */
        spoint, /* Current struct Line on Screen */
        maxbuf, sclimit = getScreen()->ROWS - 1; /* Upper limit of line * number in
                                                  * the * screen */
    struct Buffer *buf, *topbuf;
    char c;

    i = cpoint = 0;
    for (buf = firstbuf; buf != 0; buf = buf->nextBuffer) {
        if (buf == currentbuf)
            cpoint = i;
        i++;
    }
    maxbuf = i;

    if (cpoint >= sclimit) {
        spoint = sclimit / 2;
        topbuf = nthBuffer(firstbuf, cpoint - spoint);
    } else {
        topbuf = firstbuf;
        spoint = cpoint;
    }
    listBuffer(topbuf, currentbuf);

    GetChFunc getch = event_begin_input(-1);
    for (;;) {
        if ((c = getch()) == ESC_CODE) {
            if ((c = getch()) == '[' || c == 'O') {
                switch (c = getch()) {
                case 'A':
                    c = 'k';
                    break;
                case 'B':
                    c = 'j';
                    break;
                case 'C':
                    c = ' ';
                    break;
                case 'D':
                    c = 'B';
                    break;
                }
            }
        }
        switch (c) {
        case CTRL_N:
        case 'j':
            if (spoint < sclimit - 1) {
                if (currentbuf->nextBuffer == 0)
                    continue;
                writeBufferName(currentbuf, spoint);
                currentbuf = currentbuf->nextBuffer;
                cpoint++;
                spoint++;
                vt_standout(vt);
                writeBufferName(currentbuf, spoint);
                vt_standend(vt);
                vt_move(vt, spoint, 0);
                vt_toggle_stand(vt);
            } else if (cpoint < maxbuf - 1) {
                topbuf = currentbuf;
                currentbuf = currentbuf->nextBuffer;
                cpoint++;
                spoint = 1;
                listBuffer(topbuf, currentbuf);
            }
            break;
        case CTRL_P:
        case 'k':
            if (spoint > 0) {
                writeBufferName(currentbuf, spoint);
                currentbuf = nthBuffer(topbuf, --spoint);
                cpoint--;
                vt_standout(vt);
                writeBufferName(currentbuf, spoint);
                vt_standend(vt);
                vt_move(vt, spoint, 0);
                vt_toggle_stand(vt);
            } else if (cpoint > 0) {
                i = cpoint - sclimit;
                if (i < 0)
                    i = 0;
                cpoint--;
                spoint = cpoint - i;
                currentbuf = nthBuffer(firstbuf, cpoint);
                topbuf = nthBuffer(firstbuf, i);
                listBuffer(topbuf, currentbuf);
            }
            break;
        default:
            *selectchar = c;
            goto end;
        }
        vt_move(vt, spoint, 0);
        // refresh(ttyWriter());
    }
end:
    event_end_input(getch);
    return currentbuf;
}

/*
 * Reshape HTML buffer
 */
void reshapeBuffer(struct UI ui, struct Buffer* buf, int cols)
{
    buf->width = cols;
    if (buf->sourcefile == 0)
        return;

    union input_stream* stream = examineFile(buf->mailcap_source ? buf->mailcap_source : buf->sourcefile);
    if (stream == 0)
        return;

    struct Buffer sbuf;
    copyBuffer(&sbuf, buf);
    clearBuffer(buf);

    buf->document.href = 0;
    buf->document.name = 0;
    buf->document.img = 0;
    buf->document.formitem = 0;
    buf->document.formlist = 0;
    buf->document.linklist = 0;
    buf->document.maplist = 0;
    if (buf->document.hmarklist)
        buf->document.hmarklist->nmark = 0;
    if (buf->document.imarklist)
        buf->document.imarklist->nmark = 0;

    WcOption.auto_detect = WC_OPT_DETECT_OFF;
    if (buf->content_type == CONTENTTYPE_TEXT_HTML)
        loadHTMLBuffer(ui, buf->currentURL, stream, buf->document.charset, buf);
    else
        loadBuffer(buf->currentURL, stream, buf);
    ISclose(stream);
    wc_uint8 old_auto_detect = WcOption.auto_detect;
    WcOption.auto_detect = old_auto_detect;

    // buf->height = getScreen()->ROWS - 1 + 1;
    if (buf->document.firstLine && sbuf.document.firstLine) {
        struct LineList* cur = currentLine(&sbuf.document);
        int n;

        buf->pos = sbuf.pos + cur->bpos;
        while (cur->bpos && cur->prev)
            cur = cur->prev;
        // if (cur->real_linenumber > 0)
        //     gotoRealLine(buf, cur->real_linenumber);
        // else
        gotoLine(buf, cur->linenumber);
        n = (currentLine(&buf->document)->linenumber - topLine(&buf->document)->linenumber)
            - (cur->linenumber - sbuf.document.topLineIndex);
        if (n) {
            buf->document.topLineIndex = lineSkip(buf, topLine(&buf->document), n, false)->linenumber;
            // if (cur->real_linenumber > 0)
            //     gotoRealLine(buf, cur->real_linenumber);
            // else
            gotoLine(buf, cur->linenumber);
        }
        buf->pos -= currentLine(&buf->document)->bpos;
        if (FoldLine && buf->content_type != CONTENTTYPE_TEXT_HTML)
            buf->currentColumn = 0;
        else
            buf->currentColumn = sbuf.currentColumn;
        arrangeCursor(buf);
    }
    if (buf->check_url)
        chkURLBuffer(buf);
    formResetBuffer(ui, buf, sbuf.document.formitem);
}

/* shallow copy */
void copyBuffer(struct Buffer* a, struct Buffer* b)
{
    readBufferCache(b);
    memcpy(a, b, sizeof(struct Buffer));
}

struct Buffer*
prevBuffer(struct Buffer* first, struct Buffer* buf)
{
    struct Buffer* b;

    for (b = first; b != 0 && b->nextBuffer != buf; b = b->nextBuffer)
        ;
    return b;
}

#define fwrite1(d, f) (fwrite(&d, sizeof(d), 1, f) == 0)
#define fread1(d, f) (fread(&d, sizeof(d), 1, f) == 0)

int writeBufferCache(struct Buffer* buf)
{
    Str tmp;
    FILE* cache = 0;
    int colorflag;

    if (buf->savecache)
        return -1;

    if (buf->document.firstLine == 0)
        goto _error1;

    tmp = tmpfname(TMPF_CACHE, 0);
    buf->savecache = tmp->ptr;
    cache = fopen(buf->savecache, "w");
    if (!cache)
        goto _error1;

    if (fwrite1(currentLine(&buf->document)->linenumber, cache) || fwrite1(topLine(&buf->document)->linenumber, cache))
        goto _error;

    struct LineList* l;
    for (l = buf->document.firstLine; l; l = l->next) {
        if (fwrite1(l->l.usrflags, cache) || fwrite1(l->l.width, cache) || fwrite1(l->l.len, cache) || fwrite1(l->l.size, cache) || fwrite1(l->bpos, cache) || fwrite1(l->bwidth, cache))
            goto _error;
        if (l->bpos == 0) {
            if (fwrite(l->l.lineBuf, 1, l->l.size, cache) < l->l.size || fwrite(l->l.propBuf, sizeof(Lineprop), l->l.size, cache) < l->l.size)
                goto _error;
        }
        colorflag = l->l.colorBuf ? 1 : 0;
        if (fwrite1(colorflag, cache))
            goto _error;
        if (colorflag) {
            if (l->bpos == 0) {
                if (fwrite(l->l.colorBuf, sizeof(Linecolor), l->l.size, cache) < l->l.size)
                    goto _error;
            }
        }
    }

    fclose(cache);
    return 0;
_error:
    fclose(cache);
    unlink(buf->savecache);
_error1:
    buf->savecache = 0;
    return -1;
}

int readBufferCache(struct Buffer* buf)
{
    FILE* cache;
    long lnum = 0, clnum, tlnum;
    int colorflag;

    if (buf->savecache == 0)
        return -1;

    cache = fopen(buf->savecache, "r");
    if (cache == 0 || fread1(clnum, cache) || fread1(tlnum, cache)) {
        if (cache != 0)
            fclose(cache);
        buf->savecache = 0;
        return -1;
    }

    struct LineList *l = 0, *prevl = 0, *basel = 0;
    while (!feof(cache)) {
        lnum++;
        prevl = l;
        l = New(struct Line);
        l->prev = prevl;
        if (prevl)
            prevl->next = l;
        else
            buf->document.firstLine = l;
        l->linenumber = lnum;
        if (lnum == clnum)
            buf->document.currentLineIndex = l->linenumber;
        if (lnum == tlnum)
            buf->document.topLineIndex = l->linenumber;
        if (fread1(l->l.usrflags, cache) || fread1(l->l.width, cache) || fread1(l->l.len, cache) || fread1(l->l.size, cache) || fread1(l->bpos, cache) || fread1(l->bwidth, cache))
            break;
        if (l->bpos == 0) {
            basel = l;
            l->l.lineBuf = NewAtom_N(char, l->l.size + 1);
            fread(l->l.lineBuf, 1, l->l.size, cache);
            l->l.lineBuf[l->l.size] = '\0';
            l->l.propBuf = NewAtom_N(Lineprop, l->l.size);
            fread(l->l.propBuf, sizeof(Lineprop), l->l.size, cache);
        } else if (basel) {
            l->l.lineBuf = basel->l.lineBuf + l->bpos;
            l->l.propBuf = basel->l.propBuf + l->bpos;
        } else
            break;
        if (fread1(colorflag, cache))
            break;
        if (colorflag) {
            if (l->bpos == 0) {
                l->l.colorBuf = NewAtom_N(Linecolor, l->l.size);
                fread(l->l.colorBuf, sizeof(Linecolor), l->l.size, cache);
            } else
                l->l.colorBuf = basel->l.colorBuf + l->bpos;
        } else {
            l->l.colorBuf = 0;
        }
    }
    if (prevl) {
        lastLine(&buf->document)->next = 0;
    }
    fclose(cache);
    unlink(buf->savecache);
    buf->savecache = 0;
    return 0;
}

/*
 * Arrange line,column and cursor position according to current line and
 * current position.
 */
void arrangeCursor(struct Buffer* buf)
{
    int col, col2, pos;
    int delta = 1;
    if (buf == 0 || currentLine(&buf->document) == 0)
        return;
    /* Arrange line */
    if (currentLine(&buf->document)->linenumber - topLine(&buf->document)->linenumber >= getScreen()->ROWS
        || currentLine(&buf->document)->linenumber < topLine(&buf->document)->linenumber) {
        /*
         * topLine(&buf->document) = currentLine(&buf->document);
         */
        buf->document.topLineIndex = lineSkip(buf, currentLine(&buf->document), 0, false)->linenumber;
    }
    /* Arrange column */
    while (buf->pos < 0 && currentLine(&buf->document)->prev && currentLine(&buf->document)->bpos) {
        pos = buf->pos + currentLine(&buf->document)->prev->l.len;
        cursorUp(1);
        buf->pos = pos;
    }
    while (buf->pos >= currentLine(&buf->document)->l.len && currentLine(&buf->document)->next && currentLine(&buf->document)->next->bpos) {
        pos = buf->pos - currentLine(&buf->document)->l.len;
        cursorDown(1);
        buf->pos = pos;
    }
    if (currentLine(&buf->document)->l.len == 0 || buf->pos < 0)
        buf->pos = 0;
    else if (buf->pos >= currentLine(&buf->document)->l.len)
        buf->pos = currentLine(&buf->document)->l.len - 1;
    while (buf->pos > 0 && currentLine(&buf->document)->l.propBuf[buf->pos] & PC_WCHAR2)
        buf->pos--;
    col = COLPOS(&currentLine(&buf->document)->l, buf->pos);
    while (buf->pos + delta < currentLine(&buf->document)->l.len && currentLine(&buf->document)->l.propBuf[buf->pos + delta] & PC_WCHAR2)
        delta++;
    col2 = COLPOS(&currentLine(&buf->document)->l, buf->pos + delta);
    if (col < buf->currentColumn || col2 > getScreen()->COLS + buf->currentColumn) {
        buf->currentColumn = 0;
        if (col2 > getScreen()->COLS)
            columnSkip(buf, col);
    }
    /* Arrange cursor */
    // buf->cursorY = currentLine(&buf->document)->linenumber - topLine(&buf->document)->linenumber;
    buf->visualpos = currentLine(&buf->document)->bwidth + COLPOS(&currentLine(&buf->document)->l, buf->pos) - buf->currentColumn;
    // buf->cursorX = buf->visualpos - currentLine(&buf->document)->bwidth;
#ifdef DISPLAY_DEBUG
    fprintf(stderr,
        "arrangeCursor: column=%d, cursorX=%d, visualpos=%d, pos=%d, len=%d\n",
        buf->currentColumn, buf->cursorX, buf->visualpos, buf->pos,
        currentLine(&buf->document)->len);
#endif
}

void arrangeLine(struct Buffer* buf)
{
    int i, cpos;

    if (buf->document.firstLine == 0)
        return;
    // buf->cursorY = currentLine(&buf->document)->linenumber - topLine(&buf->document)->linenumber;
    i = columnPos(&currentLine(&buf->document)->l, buf->currentColumn + buf->visualpos - currentLine(&buf->document)->bwidth);
    cpos = COLPOS(&currentLine(&buf->document)->l, i) - buf->currentColumn;
    if (cpos >= 0) {
        // buf->cursorX = cpos;
        buf->pos = i;
    } else if (currentLine(&buf->document)->l.len > i) {
        // buf->cursorX = 0;
        buf->pos = i + 1;
    } else {
        // buf->cursorX = 0;
        buf->pos = 0;
    }
#ifdef DISPLAY_DEBUG
    fprintf(stderr,
        "arrangeLine: column=%d, cursorX=%d, visualpos=%d, pos=%d, len=%d\n",
        buf->currentColumn, buf->cursorX, buf->visualpos, buf->pos,
        currentLine(&buf->document)->len);
#endif
}

void cursorXY(struct Buffer* buf, int x, int y)
{
    cursorUpDown(y);

    // int oldX;
    //
    // cursorUpDown(buf, y - buf->cursorY);
    //
    // if (buf->cursorX > x) {
    //     while (buf->cursorX > x)
    //         cursorLeft(buf, getScreen()->COLS / 2);
    // } else if (buf->cursorX < x) {
    //     while (buf->cursorX < x) {
    //         oldX = buf->cursorX;
    //
    //         cursorRight(buf, getScreen()->COLS / 2);
    //
    //         if (oldX == buf->cursorX)
    //             break;
    //     }
    //     if (buf->cursorX > x)
    //         cursorLeft(buf, getScreen()->COLS / 2);
    // }
}

void restorePosition(struct Buffer* buf, struct Buffer* orig)
{
    buf->document.topLineIndex = lineSkip(buf, buf->document.firstLine, orig->document.topLineIndex - 1, false)->linenumber;
    gotoLine(buf, orig->document.currentLineIndex);
    buf->pos = orig->pos;
    if (currentLine(&buf->document) && currentLine(&orig->document))
        buf->pos += currentLine(&orig->document)->bpos - currentLine(&buf->document)->bpos;
    buf->currentColumn = orig->currentColumn;
    arrangeCursor(buf);
}

/*
 * saveBuffer: write buffer to file
 */
static void
_saveBuffer(struct Buffer* buf, FILE* f, int cont)
{
    Str tmp;
    int is_html = false;
    int set_charset = !DisplayCharset;
    wc_ces charset = DisplayCharset ? DisplayCharset : WC_CES_US_ASCII;

    is_html = buf->content_type == CONTENTTYPE_TEXT_HTML;
}

void saveBuffer(struct Buffer* buf, FILE* f, int cont)
{
    _saveBuffer(buf, f, cont);
}

struct Url*
baseURL(struct Buffer* buf)
{
    if (buf->bufferprop & BP_NO_URL) {
        /* no URL is defined for the buffer */
        return 0;
    }
    if (buf->document.baseURL != 0) {
        /* <BASE> tag is defined in the document */
        return buf->document.baseURL;
    } else if (IS_EMPTY_PARSED_URL(&buf->currentURL))
        return 0;
    else
        return &buf->currentURL;
}

int columnSkip(struct Buffer* buf, int offset)
{
    int column = buf->currentColumn + offset;
    int nlines = getScreen()->ROWS + 1;

    int maxColumn = 0;
    struct LineList* l = topLine(&buf->document);
    for (int i = 0; i < nlines && l != 0; i++, l = l->next) {
        if (l->l.width < 0)
            l->l.width = COLPOS(&l->l, l->l.len);
        if (l->l.width - 1 > maxColumn)
            maxColumn = l->l.width - 1;
    }
    maxColumn -= getScreen()->COLS - 1;
    if (column < maxColumn)
        maxColumn = column;
    if (maxColumn < 0)
        maxColumn = 0;

    if (buf->currentColumn == maxColumn)
        return 0;
    buf->currentColumn = maxColumn;
    return 1;
}

struct LineList* lineSkip(struct Buffer* buf, struct LineList* line, int offset, int last)
{
    struct LineList* l = currentLineSkip(buf, line, offset, last);
    if (!nextpage_topline)
        for (int i = getScreen()->ROWS - 1 - (lastLine(&buf->document)->linenumber - l->linenumber);
            i > 0 && l->prev != 0; i--, l = l->prev)
            ;
    return l;
}

struct LineList* currentLineSkip(struct Buffer* buf, struct LineList* line, int offset, int last)
{
    int i, n;
    struct LineList* l = line;

    if (offset == 0)
        return l;
    if (offset > 0)
        for (i = 0; i < offset && l->next != 0; i++, l = l->next)
            ;
    else
        for (i = 0; i < -offset && l->prev != 0; i++, l = l->prev)
            ;
    return l;
}

/* get last modified time */
char* last_modified(struct Buffer* buf)
{
    TextListItem* ti;
    struct stat st;

    if (buf->document_header) {
        for (ti = buf->document_header->first; ti; ti = ti->next) {
            if (strncasecmp(ti->ptr, "Last-modified: ", 15) == 0) {
                return ti->ptr + 15;
            }
        }
        return "unknown";
    } else if (buf->currentURL.scheme == SCM_LOCAL) {
        if (stat(buf->currentURL.file, &st) < 0)
            return "unknown";
        return ctime(&st.st_mtime);
    }
    return "unknown";
}

struct Content
cookie_list_panel(struct UI ui)
{
    /* FIXME: gettextize? */
    Str src = Strnew_charp("<html><head><title>Cookies</title></head>"
                           "<body><center><b>Cookies</b></center>"
                           "<p><form method=internal action=cookie>");
    struct cookie* p;
    int i;
    char tmp2[80];

    if (!use_cookie || !First_cookie)
        return (struct Content) {};

    Strcat_charp(src, "<ol>");
    for (p = First_cookie, i = 0; p; p = p->next, i++) {
        const char* tmp = html_quote(parsedURL2Str(&p->url)->ptr);
        if (p->expires != (time_t)-1) {
            strftime(tmp2, 80, "%a, %d %b %Y %H:%M:%S GMT",
                gmtime(&p->expires));
        } else
            tmp2[0] = '\0';
        Strcat_charp(src, "<li>");
        Strcat_charp(src, "<h1><a href=\"");
        Strcat_charp(src, tmp);
        Strcat_charp(src, "\">");
        Strcat_charp(src, tmp);
        Strcat_charp(src, "</a></h1>");

        Strcat_charp(src, "<table cellpadding=0>");
        if (!(p->flag & COO_SECURE)) {
            Strcat_charp(src, "<tr><td width=\"80\"><b>Cookie:</b></td><td>");
            Strcat_charp(src, html_quote(make_cookie(p)->ptr));
            Strcat_charp(src, "</td></tr>");
        }
        if (p->comment) {
            Strcat_charp(src, "<tr><td width=\"80\"><b>Comment:</b></td><td>");
            Strcat_charp(src, html_quote(p->comment->ptr));
            Strcat_charp(src, "</td></tr>");
        }
        if (p->commentURL) {
            Strcat_charp(src,
                "<tr><td width=\"80\"><b>CommentURL:</b></td><td>");
            Strcat_charp(src, "<a href=\"");
            Strcat_charp(src, html_quote(p->commentURL->ptr));
            Strcat_charp(src, "\">");
            Strcat_charp(src, html_quote(p->commentURL->ptr));
            Strcat_charp(src, "</a>");
            Strcat_charp(src, "</td></tr>");
        }
        if (tmp2[0]) {
            Strcat_charp(src, "<tr><td width=\"80\"><b>Expires:</b></td><td>");
            Strcat_charp(src, tmp2);
            if (p->flag & COO_DISCARD)
                Strcat_charp(src, " (Discard)");
            Strcat_charp(src, "</td></tr>");
        }
        Strcat_charp(src, "<tr><td width=\"80\"><b>Version:</b></td><td>");
        Strcat_charp(src, Sprintf("%d", p->version)->ptr);
        Strcat_charp(src, "</td></tr><tr><td>");
        if (p->domain) {
            Strcat_charp(src, "<tr><td width=\"80\"><b>Domain:</b></td><td>");
            Strcat_charp(src, html_quote(p->domain->ptr));
            Strcat_charp(src, "</td></tr>");
        }
        if (p->path) {
            Strcat_charp(src, "<tr><td width=\"80\"><b>Path:</b></td><td>");
            Strcat_charp(src, html_quote(p->path->ptr));
            Strcat_charp(src, "</td></tr>");
        }
        if (p->portl) {
            Strcat_charp(src, "<tr><td width=\"80\"><b>Port:</b></td><td>");
            Strcat_charp(src, html_quote(portlist2str(p->portl)->ptr));
            Strcat_charp(src, "</td></tr>");
        }
        Strcat_charp(src, "<tr><td width=\"80\"><b>Secure:</b></td><td>");
        Strcat_charp(src, (p->flag & COO_SECURE) ? "Yes" : "No");
        Strcat_charp(src, "</td></tr><tr><td>");

        Strcat(src, Sprintf("<tr><td width=\"80\"><b>Use:</b></td><td>"
                            "<input type=radio name=\"%d\" value=1%s>Yes"
                            "&nbsp;&nbsp;"
                            "<input type=radio name=\"%d\" value=0%s>No",
                        i, (p->flag & COO_USE) ? " checked" : "", i, (!(p->flag & COO_USE)) ? " checked" : ""));
        Strcat_charp(src,
            "</td></tr><tr><td><input type=submit value=\"OK\"></table><p>");
    }
    Strcat_charp(src, "</ol></form></body></html>");

    return (struct Content) {
        .url = {},
        .page = src,
        .cc = {
            .content_type = CONTENTTYPE_TEXT_HTML,
            .charset = WC_CES_UTF_8,
        },
    };
}

struct BufferPoint getBufferPosition(struct Buffer* buf)
{
    struct UI ui = getUI();
    struct LineList* l = getLine(&buf->document, ui.viewport_cursor.y);
    if (!l) {
        return (struct BufferPoint) { 0, 0 };
    }
    int pos = columnPos(&l->l, ui.viewport_cursor.x);
    return (struct BufferPoint) {
        .line = ui.viewport_cursor.y,
        .pos = pos,
    };
}

struct Anchor*
retrieveCurrentAnchor(struct Buffer* buf)
{
    if (!buf)
        return 0;
    return retrieveAnchor(buf->document.href, getBufferPosition(buf));
}

struct Anchor*
retrieveCurrentImg(struct Buffer* buf)
{
    if (currentLine(&buf->document) == 0)
        return 0;
    return retrieveAnchor(buf->document.img,
        (struct BufferPoint) { .line = currentLine(&buf->document)->linenumber, .pos = buf->pos });
}

struct Anchor*
retrieveCurrentForm(struct Buffer* buf)
{
    if (currentLine(&buf->document) == 0)
        return 0;
    return retrieveAnchor(buf->document.formitem,
        (struct BufferPoint) { .line = currentLine(&buf->document)->linenumber, .pos = buf->pos });
}

struct Anchor*
searchAnchor(struct AnchorList* al, const char* str)
{
    int i;
    struct Anchor* a;
    if (al == 0)
        return 0;
    for (i = 0; i < al->nanchor; i++) {
        a = &al->anchors[i];
        if (a->hseq < 0)
            continue;
        if (!strcmp(a->url, str))
            return a;
    }
    return 0;
}

struct Anchor*
searchURLLabel(struct Buffer* buf, const char* url)
{
    return searchAnchor(buf->document.name, url);
}

/* renumber struct Anchor */
void reseq_anchor(struct Buffer* buf)
{
    if (!buf->document.href)
        return;

    int nmark = (buf->document.hmarklist) ? buf->document.hmarklist->nmark : 0;
    int n = nmark;
    for (int i = 0; i < buf->document.href->nanchor; i++) {
        struct Anchor* a = &buf->document.href->anchors[i];
        if (a->hseq == -2)
            n++;
    }
    if (n == nmark)
        return;

    short* seqmap = NewAtom_N(short, n);
    for (int i = 0; i < n; i++)
        seqmap[i] = i;

    struct HmarkerList* ml = 0;
    for (int i = 0; i < buf->document.href->nanchor; i++) {
        struct Anchor* a = &buf->document.href->anchors[i];
        if (a->hseq == -2) {
            a->hseq = n;
            struct Anchor* a1 = closest_next_anchor(buf->document.href, 0, a->start.pos,
                a->start.line);
            a1 = closest_next_anchor(buf->document.formitem, a1, a->start.pos,
                a->start.line);
            if (a1 && a1->hseq >= 0) {
                seqmap[n] = seqmap[a1->hseq];
                for (int j = a1->hseq; j < nmark; j++)
                    seqmap[j]++;
            }
            ml = putHmarker(ml, a->start.line, a->start.pos, seqmap[n]);
            n++;
        }
    }

    for (int i = 0; i < nmark; i++) {
        ml = putHmarker(ml, buf->document.hmarklist->marks[i].line, buf->document.hmarklist->marks[i].pos, seqmap[i]);
    }
    buf->document.hmarklist = ml;

    reseq_anchor0(buf->document.href, seqmap);
    reseq_anchor0(buf->document.formitem, seqmap);
}

void addMultirowsImg(struct Buffer* buf, struct AnchorList* al)
{
    int i, j, k, col, ecol, pos;
    struct Anchor a_img, a_href, a_form, *a;
    struct LineList *l, *ls;

    if (al == 0 || al->nanchor == 0)
        return;
    for (i = 0; i < al->nanchor; i++) {
        a_img = al->anchors[i];
        struct Image* img;
        img = a_img.image;
        if (a_img.hseq < 0 || !img || img->rows <= 1)
            continue;
        for (l = buf->document.firstLine; l != 0; l = l->next) {
            if (l->linenumber == img->y)
                break;
        }
        if (!l)
            continue;
        if (a_img.y == a_img.start.line)
            ls = l;
        else {
            for (ls = l; ls != 0;
                ls = (a_img.y < a_img.start.line) ? ls->next : ls->prev) {
                if (ls->linenumber == a_img.start.line)
                    break;
            }
            if (!ls)
                continue;
        }
        a = retrieveAnchor(buf->document.href, a_img.start);
        if (a)
            a_href = *a;
        else
            a_href.url = 0;
        a = retrieveAnchor(buf->document.formitem, a_img.start);
        if (a)
            a_form = *a;
        else
            a_form.url = 0;
        col = COLPOS(&ls->l, a_img.start.pos);
        ecol = COLPOS(&ls->l, a_img.end.pos);
        for (j = 0; l && j < img->rows; l = l->next, j++) {
            if (a_img.start.line == l->linenumber)
                continue;
            pos = columnPos(&l->l, col);
            a = registerImg(&buf->document, a_img.url, a_img.title,
                (struct BufferPoint) { .line = l->linenumber, .pos = pos });
            a->hseq = -a_img.hseq;
            a->slave = true;
            a->image = img;
            a->end.pos = pos + ecol - col;
            for (k = pos; k < a->end.pos; k++)
                l->l.propBuf[k] |= PE_IMAGE;
            if (a_href.url) {
                a = registerHref(&buf->document, a_href.url, a_href.target,
                    a_href.referer, a_href.title, a_href.accesskey,
                    (struct BufferPoint) { .line = l->linenumber, .pos = pos });
                a->hseq = a_href.hseq;
                a->slave = true;
                a->end.pos = pos + ecol - col;
                for (k = pos; k < a->end.pos; k++)
                    l->l.propBuf[k] |= PE_ANCHOR;
            }
            if (a_form.url) {
                buf->document.formitem = putAnchor(buf->document.formitem, &a,
                    (struct BufferPoint) { .line = l->linenumber, .pos = pos });
                initAnchor(a, a_form.url, a_form.target, 0, 0, '\0');
                a->hseq = a_form.hseq;
                a->end.pos = pos + ecol - col;
            }
        }
        img->rows = 0;
    }
}

void addMultirowsForm(struct Buffer* buf, struct AnchorList* al)
{
    int i, j, k, col, ecol, pos;
    struct Anchor a_form, *a;
    struct LineList *l, *ls;

    if (al == 0 || al->nanchor == 0)
        return;
    for (i = 0; i < al->nanchor; i++) {
        a_form = al->anchors[i];
        al->anchors[i].rows = 1;
        if (a_form.hseq < 0 || a_form.rows <= 1)
            continue;
        for (l = buf->document.firstLine; l != 0; l = l->next) {
            if (l->linenumber == a_form.y)
                break;
        }
        if (!l)
            continue;
        if (a_form.y == a_form.start.line)
            ls = l;
        else {
            for (ls = l; ls != 0;
                ls = (a_form.y < a_form.start.line) ? ls->next : ls->prev) {
                if (ls->linenumber == a_form.start.line)
                    break;
            }
            if (!ls)
                continue;
        }
        col = COLPOS(&ls->l, a_form.start.pos);
        ecol = COLPOS(&ls->l, a_form.end.pos);
        for (j = 0; l && j < a_form.rows; l = l->next, j++) {
            pos = columnPos(&l->l, col);
            if (j == 0) {
                buf->document.hmarklist->marks[a_form.hseq].line = l->linenumber;
                buf->document.hmarklist->marks[a_form.hseq].pos = pos;
            }
            if (a_form.start.line == l->linenumber)
                continue;
            buf->document.formitem = putAnchor(buf->document.formitem, &a,
                (struct BufferPoint) { .line = l->linenumber, .pos = pos });
            initAnchor(a, a_form.url, a_form.target, 0, 0, '\0');
            a->hseq = a_form.hseq;
            a->y = a_form.y;
            a->end.pos = pos + ecol - col;
            if (pos < 1 || a->end.pos >= l->l.size)
                continue;
            l->l.lineBuf[pos - 1] = '[';
            l->l.lineBuf[a->end.pos] = ']';
            for (k = pos; k < a->end.pos; k++)
                l->l.propBuf[k] |= PE_FORM;
        }
    }
}

const char* getAnchorText(struct Buffer* buf, struct AnchorList* al, struct Anchor* a)
{
    if (!a || a->hseq < 0)
        return 0;

    Str tmp = 0;
    int hseq = a->hseq;
    struct LineList* l = buf->document.firstLine;
    for (int i = 0; i < al->nanchor; i++) {
        a = &al->anchors[i];
        if (a->hseq != hseq)
            continue;
        for (; l; l = l->next) {
            if (l->linenumber == a->start.line)
                break;
        }
        if (!l)
            break;
        const char* p = l->l.lineBuf + a->start.pos;
        const char* ep = l->l.lineBuf + a->end.pos;
        for (; p < ep && IS_SPACE(*p); p++)
            ;
        if (p == ep)
            continue;
        if (!tmp)
            tmp = Strnew_size(ep - p);
        else
            Strcat_char(tmp, ' ');
        Strcat_charp_n(tmp, p, ep - p);
    }
    return tmp ? tmp->ptr : 0;
}

MapArea*
retrieveCurrentMapArea(struct Buffer* buf)
{
    struct Anchor *a_img, *a_form;
    struct FormItem* fi;
    MapList* ml;
    ListItem* al;
    MapArea* a;
    int i, n;

    a_img = retrieveCurrentImg(buf);
    if (!(a_img && a_img->image && a_img->image->map))
        return 0;
    a_form = retrieveCurrentForm(buf);
    if (!(a_form && a_form->url))
        return 0;
    fi = (struct FormItem*)a_form->url;
    if (!(fi && fi->parent && fi->parent->item))
        return 0;
    fi = fi->parent->item;
    ml = searchMapList(buf, fi->value ? fi->value->ptr : 0);
    if (!ml)
        return 0;
    n = searchMapArea(buf, ml, a_img);
    if (n < 0)
        return 0;
    for (i = 0, al = ml->area->first; al != 0; i++, al = al->next) {
        a = (MapArea*)al->ptr;
        if (a && i == n)
            return a;
    }
    return 0;
}

static struct Anchor*
_put_anchor_all(struct Buffer* buf, const char* p1, const char* p2, struct BufferPoint bp)
{
    Str tmp = Strnew_charp_n(p1, p2 - p1);
    return registerHref(&buf->document, url_quote(tmp->ptr), NULL, NO_REFERER, NULL, '\0', bp);
}

typedef struct Anchor* (*AnchorFunc)(struct Buffer*, const char*, const char*, struct BufferPoint);

static const char*
reAnchorPos(struct Buffer* buf, struct LineList* l, const char* p1, const char* p2, AnchorFunc anchorproc)
{
    int spos = p1 - l->l.lineBuf;
    int epos = p2 - l->l.lineBuf;
    for (int i = spos; i < epos; i++) {
        if (l->l.propBuf[i] & (PE_ANCHOR | PE_FORM))
            return p2;
    }
    for (int i = spos; i < epos; i++)
        l->l.propBuf[i] |= PE_ANCHOR;
    while (spos > l->l.len && l->next && l->next->bpos) {
        spos -= l->l.len;
        epos -= l->l.len;
        l = l->next;
    }

    int hseq = -2;
    while (1) {
        struct Anchor* a = anchorproc(buf, p1, p2, (struct BufferPoint) { .line = l->linenumber, .pos = spos });
        a->hseq = hseq;
        if (hseq == -2) {
            reseq_anchor(buf);
            hseq = a->hseq;
        }
        a->end.line = l->linenumber;
        if (epos > l->l.len && l->next && l->next->bpos) {
            a->end.pos = l->l.len;
            spos = 0;
            epos -= l->l.len;
            l = l->next;
        } else {
            a->end.pos = epos;
            break;
        }
    }
    return p2;
}

void reAnchorWord(struct Buffer* buf, struct LineList* l, int spos, int epos)
{
    reAnchorPos(buf, l, &l->l.lineBuf[spos], &l->l.lineBuf[epos], _put_anchor_all);
}

/* search regexp and register them as anchors */
/* returns error message if any               */
static const char*
reAnchorAny(struct Buffer* buf, const char* re, AnchorFunc anchorproc)
{
    struct LineList* l;

    if (re == NULL || *re == '\0') {
        return NULL;
    }
    if ((re = regexCompile(re, 1)) != NULL) {
        return re;
    }

    const char* p = NULL;
    for (l = MarkAllPages ? buf->document.firstLine : topLine(&buf->document); l != NULL && (MarkAllPages || l->linenumber < topLine(&buf->document)->linenumber + getScreen()->ROWS - 1);
        l = l->next) {
        if (p && l->bpos)
            continue;
        p = l->l.lineBuf;
        for (;;) {
            if (regexMatch(p, &l->l.lineBuf[l->l.size] - p, p == l->l.lineBuf) == 1) {
                const char *p1, *p2;
                matchedPosition(&p1, &p2);
                p = reAnchorPos(buf, l, p1, p2, anchorproc);
            } else
                break;
        }
    }
    return NULL;
}

const char* reAnchor(struct Buffer* buf, const char* re)
{
    return reAnchorAny(buf, re, _put_anchor_all);
}
