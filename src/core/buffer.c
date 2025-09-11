#include "buffer.h"
#include "Content.h"
#include "runtime.h"
#include "html_quote.h"
#include "cookie.h"
#include "convertline.h"
#include "quote.h"
#include "screen_effects.h"
#include "alloc.h"
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
 * Buffer creation
 */
Buffer*
newBuffer()
{
    Buffer* n = New(Buffer);
    memset(n, 0, sizeof(Buffer));
    n->width = 0;
    n->currentURL.scheme = SCM_UNKNOWN;
    n->baseURL = NULL;
    n->baseTarget = NULL;
    n->buffername = "";
    n->bufferprop = BP_NORMAL;
    n->clone = New(int);
    *n->clone = 1;
    n->trbyte = 0;
    n->ssl_certificate = NULL;
    n->auto_detect = WcOption.auto_detect;
    n->check_url = MarkAllPages;
    return n;
}

/*
 * Create null buffer
 */
Buffer*
nullBuffer(void)
{
    Buffer* b;

    b = newBuffer();
    b->buffername = "*Null*";
    return b;
}

/*
 * clearBuffer: clear buffer content
 */
void clearBuffer(Buffer* buf)
{
    buf->firstLine = buf->topLine = buf->currentLine = buf->lastLine = NULL;
    buf->allLine = 0;
}

/*
 * discardBuffer: free buffer structure
 */

void discardBuffer(Buffer* buf)
{
    deleteImage(buf);
    clearBuffer(buf);
    for (int i = 0; i < MAX_LB; i++) {
        Buffer* b = buf->linkBuffer[i];
        if (b == NULL)
            continue;
        b->linkBuffer[REV_LB[i]] = NULL;
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
Buffer*
namedBuffer(Buffer* first, char* name)
{
    Buffer* buf;

    if (!strcmp(first->buffername, name)) {
        return first;
    }
    for (buf = first; buf->nextBuffer != NULL; buf = buf->nextBuffer) {
        if (!strcmp(buf->nextBuffer->buffername, name)) {
            return buf->nextBuffer;
        }
    }
    return NULL;
}

/*
 * deleteBuffer: delete buffer
 */
Buffer*
deleteBuffer(Buffer* first, Buffer* delbuf)
{
    Buffer *buf, *b;

    if (first == delbuf && first->nextBuffer != NULL) {
        buf = first->nextBuffer;
        discardBuffer(first);
        return buf;
    }
    if ((buf = prevBuffer(first, delbuf)) != NULL) {
        b = buf->nextBuffer;
        buf->nextBuffer = b->nextBuffer;
        discardBuffer(b);
    }
    return first;
}

/*
 * replaceBuffer: replace buffer
 */
Buffer*
replaceBuffer(Buffer* first, Buffer* delbuf, Buffer* newbuf)
{
    Buffer* buf;

    if (delbuf == NULL) {
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

Buffer*
nthBuffer(Buffer* firstbuf, int n)
{
    int i;
    Buffer* buf = firstbuf;

    if (n < 0)
        return firstbuf;
    for (i = 0; i < n; i++) {
        if (buf == NULL)
            return NULL;
        buf = buf->nextBuffer;
    }
    return buf;
}

static void
writeBufferName(Buffer* buf, int n)
{
    int all = buf->allLine;
    if (all == 0 && buf->lastLine != NULL)
        all = buf->lastLine->linenumber;
    vt_move(getScreen(), n, 0);

    Str msg = Sprintf("<%s> [%d lines]", buf->buffername, all);
    if (buf->filename != NULL) {
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
void gotoLine(Buffer* buf, int n)
{
    char msg[36];
    Line* l = buf->firstLine;
    if (l == NULL)
        return;
    if (l->linenumber > n) {
        /* FIXME: gettextize? */
        sprintf(msg, "First line is #%ld", l->linenumber);
        set_delayed_message(msg);
        buf->topLine = buf->currentLine = l;
        return;
    }
    if (buf->lastLine->linenumber < n) {
        l = buf->lastLine;
        /* FIXME: gettextize? */
        sprintf(msg, "Last line is #%ld", buf->lastLine->linenumber);
        set_delayed_message(msg);
        buf->currentLine = l;
        buf->topLine = lineSkip(buf, buf->currentLine, -(getScreen()->ROWS - 1), false);
        return;
    }
    for (; l != NULL; l = l->next) {
        if (l->linenumber >= n) {
            buf->currentLine = l;
            if (n < buf->topLine->linenumber || buf->topLine->linenumber + getScreen()->ROWS <= n)
                buf->topLine = lineSkip(buf, l, -(getScreen()->ROWS + 1) / 2, false);
            break;
        }
    }
}

/*
 * gotoRealLine: go to real line number
 */
void gotoRealLine(Buffer* buf, int n)
{
    char msg[36];
    Line* l = buf->firstLine;

    if (l == NULL)
        return;
    if (l->real_linenumber > n) {
        /* FIXME: gettextize? */
        sprintf(msg, "First line is #%ld", l->real_linenumber);
        set_delayed_message(msg);
        buf->topLine = buf->currentLine = l;
        return;
    }
    if (buf->lastLine->real_linenumber < n) {
        l = buf->lastLine;
        /* FIXME: gettextize? */
        sprintf(msg, "Last line is #%ld", buf->lastLine->real_linenumber);
        set_delayed_message(msg);
        buf->currentLine = l;
        buf->topLine = lineSkip(buf, buf->currentLine, -(getScreen()->ROWS - 1),
            false);
        return;
    }
    for (; l != NULL; l = l->next) {
        if (l->real_linenumber >= n) {
            buf->currentLine = l;
            if (n < buf->topLine->real_linenumber || buf->topLine->real_linenumber + getScreen()->ROWS <= n)
                buf->topLine = lineSkip(buf, l, -(getScreen()->ROWS + 1) / 2, false);
            break;
        }
    }
}

static Buffer*
listBuffer(Buffer* top, Buffer* current)
{
    struct VirtualTerm* vt = getScreen();
    int i, c = 0;
    Buffer* buf = top;

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
        if (buf->nextBuffer == NULL) {
            vt_move(vt, i + 1, 0);
            vt_clrtobotx(vt);
            break;
        }
        buf = buf->nextBuffer;
    }
    vt_standout(vt);
    /* FIXME: gettextize? */
    message(getUI(), MSG_INFO, "Buffer selection mode: SPC for select / D for delete buffer");
    vt_standend(vt);
    vt_move(vt, c, 0);
    // refresh(ttyWriter());
    return buf->nextBuffer;
}

/*
 * Select buffer visually
 */
Buffer*
selectBuffer(Buffer* firstbuf, Buffer* currentbuf, char* selectchar)
{
    struct VirtualTerm* vt = getScreen();
    int i, cpoint, /* Current Buffer Number */
        spoint, /* Current Line on Screen */
        maxbuf, sclimit = getScreen()->ROWS - 1; /* Upper limit of line * number in
                                                  * the * screen */
    Buffer *buf, *topbuf;
    char c;

    i = cpoint = 0;
    for (buf = firstbuf; buf != NULL; buf = buf->nextBuffer) {
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
                if (currentbuf->nextBuffer == NULL)
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
void reshapeBuffer(Buffer* buf, int cols)
{
    buf->width = cols;
    if (buf->sourcefile == NULL)
        return;

    union input_stream* stream = examineFile(buf->mailcap_source ? buf->mailcap_source : buf->sourcefile);
    if (stream == NULL)
        return;

    Buffer sbuf;
    copyBuffer(&sbuf, buf);
    clearBuffer(buf);

    buf->href = NULL;
    buf->name = NULL;
    buf->img = NULL;
    buf->formitem = NULL;
    buf->formlist = NULL;
    buf->linklist = NULL;
    buf->maplist = NULL;
    if (buf->hmarklist)
        buf->hmarklist->nmark = 0;
    if (buf->imarklist)
        buf->imarklist->nmark = 0;

    WcOption.auto_detect = WC_OPT_DETECT_OFF;
    if (buf->content_type == CONTENTTYPE_TEXT_HTML)
        loadHTMLBuffer(buf->currentURL, stream, buf);
    else
        loadBuffer(buf->currentURL, stream, buf);
    ISclose(stream);
    wc_uint8 old_auto_detect = WcOption.auto_detect;
    WcOption.auto_detect = old_auto_detect;

    // buf->height = getScreen()->ROWS - 1 + 1;
    if (buf->firstLine && sbuf.firstLine) {
        Line* cur = sbuf.currentLine;
        int n;

        buf->pos = sbuf.pos + cur->bpos;
        while (cur->bpos && cur->prev)
            cur = cur->prev;
        if (cur->real_linenumber > 0)
            gotoRealLine(buf, cur->real_linenumber);
        else
            gotoLine(buf, cur->linenumber);
        n = (buf->currentLine->linenumber - buf->topLine->linenumber)
            - (cur->linenumber - sbuf.topLine->linenumber);
        if (n) {
            buf->topLine = lineSkip(buf, buf->topLine, n, false);
            if (cur->real_linenumber > 0)
                gotoRealLine(buf, cur->real_linenumber);
            else
                gotoLine(buf, cur->linenumber);
        }
        buf->pos -= buf->currentLine->bpos;
        if (FoldLine && buf->content_type != CONTENTTYPE_TEXT_HTML)
            buf->currentColumn = 0;
        else
            buf->currentColumn = sbuf.currentColumn;
        arrangeCursor(buf);
    }
    if (buf->check_url & CHK_URL)
        chkURLBuffer(buf);
    formResetBuffer(buf, sbuf.formitem);
}

/* shallow copy */
void copyBuffer(Buffer* a, Buffer* b)
{
    readBufferCache(b);
    memcpy(a, b, sizeof(Buffer));
}

Buffer*
prevBuffer(Buffer* first, Buffer* buf)
{
    Buffer* b;

    for (b = first; b != NULL && b->nextBuffer != buf; b = b->nextBuffer)
        ;
    return b;
}

#define fwrite1(d, f) (fwrite(&d, sizeof(d), 1, f) == 0)
#define fread1(d, f) (fread(&d, sizeof(d), 1, f) == 0)

int writeBufferCache(Buffer* buf)
{
    Str tmp;
    FILE* cache = NULL;
    Line* l;
    int colorflag;

    if (buf->savecache)
        return -1;

    if (buf->firstLine == NULL)
        goto _error1;

    tmp = tmpfname(TMPF_CACHE, NULL);
    buf->savecache = tmp->ptr;
    cache = fopen(buf->savecache, "w");
    if (!cache)
        goto _error1;

    if (fwrite1(buf->currentLine->linenumber, cache) || fwrite1(buf->topLine->linenumber, cache))
        goto _error;

    for (l = buf->firstLine; l; l = l->next) {
        if (fwrite1(l->real_linenumber, cache) || fwrite1(l->usrflags, cache) || fwrite1(l->width, cache) || fwrite1(l->len, cache) || fwrite1(l->size, cache) || fwrite1(l->bpos, cache) || fwrite1(l->bwidth, cache))
            goto _error;
        if (l->bpos == 0) {
            if (fwrite(l->lineBuf, 1, l->size, cache) < l->size || fwrite(l->propBuf, sizeof(Lineprop), l->size, cache) < l->size)
                goto _error;
        }
        colorflag = l->colorBuf ? 1 : 0;
        if (fwrite1(colorflag, cache))
            goto _error;
        if (colorflag) {
            if (l->bpos == 0) {
                if (fwrite(l->colorBuf, sizeof(Linecolor), l->size, cache) < l->size)
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
    buf->savecache = NULL;
    return -1;
}

int readBufferCache(Buffer* buf)
{
    FILE* cache;
    Line *l = NULL, *prevl = NULL, *basel = NULL;
    long lnum = 0, clnum, tlnum;
    int colorflag;

    if (buf->savecache == NULL)
        return -1;

    cache = fopen(buf->savecache, "r");
    if (cache == NULL || fread1(clnum, cache) || fread1(tlnum, cache)) {
        if (cache != NULL)
            fclose(cache);
        buf->savecache = NULL;
        return -1;
    }

    while (!feof(cache)) {
        lnum++;
        prevl = l;
        l = New(Line);
        l->prev = prevl;
        if (prevl)
            prevl->next = l;
        else
            buf->firstLine = l;
        l->linenumber = lnum;
        if (lnum == clnum)
            buf->currentLine = l;
        if (lnum == tlnum)
            buf->topLine = l;
        if (fread1(l->real_linenumber, cache) || fread1(l->usrflags, cache) || fread1(l->width, cache) || fread1(l->len, cache) || fread1(l->size, cache) || fread1(l->bpos, cache) || fread1(l->bwidth, cache))
            break;
        if (l->bpos == 0) {
            basel = l;
            l->lineBuf = NewAtom_N(char, l->size + 1);
            fread(l->lineBuf, 1, l->size, cache);
            l->lineBuf[l->size] = '\0';
            l->propBuf = NewAtom_N(Lineprop, l->size);
            fread(l->propBuf, sizeof(Lineprop), l->size, cache);
        } else if (basel) {
            l->lineBuf = basel->lineBuf + l->bpos;
            l->propBuf = basel->propBuf + l->bpos;
        } else
            break;
        if (fread1(colorflag, cache))
            break;
        if (colorflag) {
            if (l->bpos == 0) {
                l->colorBuf = NewAtom_N(Linecolor, l->size);
                fread(l->colorBuf, sizeof(Linecolor), l->size, cache);
            } else
                l->colorBuf = basel->colorBuf + l->bpos;
        } else {
            l->colorBuf = NULL;
        }
    }
    if (prevl) {
        buf->lastLine = prevl;
        buf->lastLine->next = NULL;
    }
    fclose(cache);
    unlink(buf->savecache);
    buf->savecache = NULL;
    return 0;
}

void cursorUp0(Buffer* buf, int n)
{
    if (buf->cursorY > 0)
        cursorUpDown(buf, -1);
    else {
        buf->topLine = lineSkip(buf, buf->topLine, -n, false);
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
    if (buf->cursorY < getScreen()->ROWS - 1)
        cursorUpDown(buf, 1);
    else {
        buf->topLine = lineSkip(buf, buf->topLine, n, false);
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
    if ((buf->currentLine = currentLineSkip(buf, cl, n, false)) == cl)
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
    if (vpos2 >= getScreen()->COLS && n) {
        columnSkip(buf, n + (vpos2 - getScreen()->COLS) - (vpos2 - getScreen()->COLS) % n);
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
    if (buf->currentLine->linenumber - buf->topLine->linenumber >= getScreen()->ROWS
        || buf->currentLine->linenumber < buf->topLine->linenumber) {
        /*
         * buf->topLine = buf->currentLine;
         */
        buf->topLine = lineSkip(buf, buf->currentLine, 0, false);
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
    if (col < buf->currentColumn || col2 > getScreen()->COLS + buf->currentColumn) {
        buf->currentColumn = 0;
        if (col2 > getScreen()->COLS)
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
            cursorLeft(buf, getScreen()->COLS / 2);
    } else if (buf->cursorX < x) {
        while (buf->cursorX < x) {
            oldX = buf->cursorX;

            cursorRight(buf, getScreen()->COLS / 2);

            if (oldX == buf->cursorX)
                break;
        }
        if (buf->cursorX > x)
            cursorLeft(buf, getScreen()->COLS / 2);
    }
}

void restorePosition(Buffer* buf, Buffer* orig)
{
    buf->topLine = lineSkip(buf, buf->firstLine, TOP_LINENUMBER(orig) - 1,
        false);
    gotoLine(buf, CUR_LINENUMBER(orig));
    buf->pos = orig->pos;
    if (buf->currentLine && orig->currentLine)
        buf->pos += orig->currentLine->bpos - buf->currentLine->bpos;
    buf->currentColumn = orig->currentColumn;
    arrangeCursor(buf);
}

/*
 * saveBuffer: write buffer to file
 */
static void
_saveBuffer(Buffer* buf, Line* l, FILE* f, int cont)
{
    Str tmp;
    int is_html = false;
    int set_charset = !DisplayCharset;
    wc_ces charset = DisplayCharset ? DisplayCharset : WC_CES_US_ASCII;

    is_html = buf->content_type == CONTENTTYPE_TEXT_HTML;
}

void saveBuffer(Buffer* buf, FILE* f, int cont)
{
    _saveBuffer(buf, buf->firstLine, f, cont);
}

struct Url*
baseURL(Buffer* buf)
{
    if (buf->bufferprop & BP_NO_URL) {
        /* no URL is defined for the buffer */
        return NULL;
    }
    if (buf->baseURL != NULL) {
        /* <BASE> tag is defined in the document */
        return buf->baseURL;
    } else if (IS_EMPTY_PARSED_URL(&buf->currentURL))
        return NULL;
    else
        return &buf->currentURL;
}

static char* url_unquote_conv(char* url, wc_ces charset)
{
    wc_uint8 old_auto_detect = WcOption.auto_detect;
    Str tmp;
    tmp = Str_url_unquote(Strnew_charp(url), false, true);
    if (!charset || charset == WC_CES_US_ASCII)
        charset = SystemCharset;
    WcOption.auto_detect = WC_OPT_DETECT_ON;
    tmp = convertLine(tmp, RAW_MODE, &charset, charset, InnerCharset);
    WcOption.auto_detect = old_auto_detect;
    return tmp->ptr;
}

char* url_decode2(const char* url, const Buffer* buf)
{
    if (!DecodeURL)
        return (char*)url;
    wc_ces url_charset = buf ? buf->document_charset : 0;
    return url_unquote_conv((char*)url, url_charset);
}

int columnSkip(Buffer* buf, int offset)
{
    int i, maxColumn;
    int column = buf->currentColumn + offset;
    int nlines = getScreen()->ROWS + 1;
    Line* l;

    maxColumn = 0;
    for (i = 0, l = buf->topLine; i < nlines && l != NULL; i++, l = l->next) {
        if (l->width < 0)
            l->width = COLPOS(l, l->len);
        if (l->width - 1 > maxColumn)
            maxColumn = l->width - 1;
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

Line* lineSkip(Buffer* buf, Line* line, int offset, int last)
{
    int i;
    Line* l;

    l = currentLineSkip(buf, line, offset, last);
    if (!nextpage_topline)
        for (i = getScreen()->ROWS - 1 - (buf->lastLine->linenumber - l->linenumber);
            i > 0 && l->prev != NULL; i--, l = l->prev)
            ;
    return l;
}

Line* currentLineSkip(Buffer* buf, Line* line, int offset, int last)
{
    int i, n;
    Line* l = line;

    if (offset == 0)
        return l;
    if (offset > 0)
        for (i = 0; i < offset && l->next != NULL; i++, l = l->next)
            ;
    else
        for (i = 0; i < -offset && l->prev != NULL; i++, l = l->prev)
            ;
    return l;
}

/* get last modified time */
char* last_modified(Buffer* buf)
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

Buffer*
cookie_list_panel(void)
{
    /* FIXME: gettextize? */
    Str src = Strnew_charp("<html><head><title>Cookies</title></head>"
                           "<body><center><b>Cookies</b></center>"
                           "<p><form method=internal action=cookie>");
    struct cookie* p;
    int i;
    char tmp2[80];

    if (!use_cookie || !First_cookie)
        return NULL;

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
    return loadHTMLString(src);
}
