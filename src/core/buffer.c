#include "buffer.h"
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
    buf->firstLine = NULL;
    buf->topLineIndex = 0;
    buf->currentLineIndex = 0;
    buf->allLine = 0;
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

struct LineList* lastLine(struct Buffer* buf)
{
    struct LineList* l = buf->firstLine;
    if (!l) {
        return NULL;
    }
    for (; l->next; l = l->next) {
    }
    return l;
}

struct LineList* currentLine(struct Buffer* buf)
{
    for (struct LineList* l = buf->firstLine; l; l = l->next) {
        if (l->linenumber == buf->currentLineIndex) {
            return l;
        }
    }
    return NULL;
}

struct LineList* topLine(struct Buffer* buf)
{
    for (struct LineList* l = buf->firstLine; l; l = l->next) {
        if (l->linenumber == buf->topLineIndex) {
            return l;
        }
    }
    return NULL;
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
struct Buffer*
deleteBuffer(struct Buffer* first, struct Buffer* delbuf)
{
    struct Buffer *buf, *b;

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
struct Buffer*
replaceBuffer(struct Buffer* first, struct Buffer* delbuf, struct Buffer* newbuf)
{
    struct Buffer* buf;

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

struct Buffer*
nthBuffer(struct Buffer* firstbuf, int n)
{
    int i;
    struct Buffer* buf = firstbuf;

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
writeBufferName(struct Buffer* buf, int n)
{
    int all = buf->allLine;
    if (all == 0 && lastLine(buf) != NULL)
        all = lastLine(buf)->linenumber;
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
void gotoLine(struct Buffer* buf, int n)
{
    char msg[36];
    struct LineList* l = buf->firstLine;
    if (l == NULL)
        return;
    if (l->linenumber > n) {
        /* FIXME: gettextize? */
        sprintf(msg, "First line is #%ld", l->linenumber);
        set_delayed_message(msg);
        buf->topLineIndex = buf->currentLineIndex = l->linenumber;
        return;
    }
    if (lastLine(buf)->linenumber < n) {
        l = lastLine(buf);
        /* FIXME: gettextize? */
        sprintf(msg, "Last line is #%ld", lastLine(buf)->linenumber);
        set_delayed_message(msg);
        buf->currentLineIndex = l->linenumber;
        buf->topLineIndex = lineSkip(buf, currentLine(buf), -(getScreen()->ROWS - 1), false)->linenumber;
        return;
    }
    for (; l != NULL; l = l->next) {
        if (l->linenumber >= n) {
            buf->currentLineIndex = l->linenumber;
            if (n < topLine(buf)->linenumber || topLine(buf)->linenumber + getScreen()->ROWS <= n)
                buf->topLineIndex = lineSkip(buf, l, -(getScreen()->ROWS + 1) / 2, false)->linenumber;
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
//     if (l == NULL)
//         return;
//     if (l->real_linenumber > n) {
//         /* FIXME: gettextize? */
//         sprintf(msg, "First line is #%ld", l->real_linenumber);
//         set_delayed_message(msg);
//         topLine(buf) = currentLine(buf) = l;
//         return;
//     }
//     if (lastLine(buf)->real_linenumber < n) {
//         l = lastLine(buf);
//         /* FIXME: gettextize? */
//         sprintf(msg, "Last line is #%ld", lastLine(buf)->real_linenumber);
//         set_delayed_message(msg);
//         currentLine(buf) = l;
//         topLine(buf) = lineSkip(buf, currentLine(buf), -(getScreen()->ROWS - 1),
//             false);
//         return;
//     }
//     for (; l != NULL; l = l->next) {
//         if (l->real_linenumber >= n) {
//             currentLine(buf) = l;
//             if (n < topLine(buf)->real_linenumber || topLine(buf)->real_linenumber + getScreen()->ROWS <= n)
//                 topLine(buf) = lineSkip(buf, l, -(getScreen()->ROWS + 1) / 2, false);
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
        if (buf->nextBuffer == NULL) {
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
void reshapeBuffer(struct Buffer* buf, int cols)
{
    buf->width = cols;
    if (buf->sourcefile == NULL)
        return;

    union input_stream* stream = examineFile(buf->mailcap_source ? buf->mailcap_source : buf->sourcefile);
    if (stream == NULL)
        return;

    struct Buffer sbuf;
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
        loadHTMLBuffer(buf->currentURL, stream, buf->document_charset, buf);
    else
        loadBuffer(buf->currentURL, stream, buf);
    ISclose(stream);
    wc_uint8 old_auto_detect = WcOption.auto_detect;
    WcOption.auto_detect = old_auto_detect;

    // buf->height = getScreen()->ROWS - 1 + 1;
    if (buf->firstLine && sbuf.firstLine) {
        struct LineList* cur = currentLine(&sbuf);
        int n;

        buf->pos = sbuf.pos + cur->bpos;
        while (cur->bpos && cur->prev)
            cur = cur->prev;
        // if (cur->real_linenumber > 0)
        //     gotoRealLine(buf, cur->real_linenumber);
        // else
        gotoLine(buf, cur->linenumber);
        n = (currentLine(buf)->linenumber - topLine(buf)->linenumber)
            - (cur->linenumber - sbuf.topLineIndex);
        if (n) {
            buf->topLineIndex = lineSkip(buf, topLine(buf), n, false)->linenumber;
            // if (cur->real_linenumber > 0)
            //     gotoRealLine(buf, cur->real_linenumber);
            // else
            gotoLine(buf, cur->linenumber);
        }
        buf->pos -= currentLine(buf)->bpos;
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
void copyBuffer(struct Buffer* a, struct Buffer* b)
{
    readBufferCache(b);
    memcpy(a, b, sizeof(struct Buffer));
}

struct Buffer*
prevBuffer(struct Buffer* first, struct Buffer* buf)
{
    struct Buffer* b;

    for (b = first; b != NULL && b->nextBuffer != buf; b = b->nextBuffer)
        ;
    return b;
}

#define fwrite1(d, f) (fwrite(&d, sizeof(d), 1, f) == 0)
#define fread1(d, f) (fread(&d, sizeof(d), 1, f) == 0)

int writeBufferCache(struct Buffer* buf)
{
    Str tmp;
    FILE* cache = NULL;
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

    if (fwrite1(currentLine(buf)->linenumber, cache) || fwrite1(topLine(buf)->linenumber, cache))
        goto _error;

    struct LineList* l;
    for (l = buf->firstLine; l; l = l->next) {
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
    buf->savecache = NULL;
    return -1;
}

int readBufferCache(struct Buffer* buf)
{
    FILE* cache;
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

    struct LineList *l = NULL, *prevl = NULL, *basel = NULL;
    while (!feof(cache)) {
        lnum++;
        prevl = l;
        l = New(struct Line);
        l->prev = prevl;
        if (prevl)
            prevl->next = l;
        else
            buf->firstLine = l;
        l->linenumber = lnum;
        if (lnum == clnum)
            buf->currentLineIndex = l->linenumber;
        if (lnum == tlnum)
            buf->topLineIndex = l->linenumber;
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
            l->l.colorBuf = NULL;
        }
    }
    if (prevl) {
        lastLine(buf)->next = NULL;
    }
    fclose(cache);
    unlink(buf->savecache);
    buf->savecache = NULL;
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
    if (buf == NULL || currentLine(buf) == NULL)
        return;
    /* Arrange line */
    if (currentLine(buf)->linenumber - topLine(buf)->linenumber >= getScreen()->ROWS
        || currentLine(buf)->linenumber < topLine(buf)->linenumber) {
        /*
         * topLine(buf) = currentLine(buf);
         */
        buf->topLineIndex = lineSkip(buf, currentLine(buf), 0, false)->linenumber;
    }
    /* Arrange column */
    while (buf->pos < 0 && currentLine(buf)->prev && currentLine(buf)->bpos) {
        pos = buf->pos + currentLine(buf)->prev->l.len;
        cursorUp(1);
        buf->pos = pos;
    }
    while (buf->pos >= currentLine(buf)->l.len && currentLine(buf)->next && currentLine(buf)->next->bpos) {
        pos = buf->pos - currentLine(buf)->l.len;
        cursorDown(1);
        buf->pos = pos;
    }
    if (currentLine(buf)->l.len == 0 || buf->pos < 0)
        buf->pos = 0;
    else if (buf->pos >= currentLine(buf)->l.len)
        buf->pos = currentLine(buf)->l.len - 1;
    while (buf->pos > 0 && currentLine(buf)->l.propBuf[buf->pos] & PC_WCHAR2)
        buf->pos--;
    col = COLPOS(&currentLine(buf)->l, buf->pos);
    while (buf->pos + delta < currentLine(buf)->l.len && currentLine(buf)->l.propBuf[buf->pos + delta] & PC_WCHAR2)
        delta++;
    col2 = COLPOS(&currentLine(buf)->l, buf->pos + delta);
    if (col < buf->currentColumn || col2 > getScreen()->COLS + buf->currentColumn) {
        buf->currentColumn = 0;
        if (col2 > getScreen()->COLS)
            columnSkip(buf, col);
    }
    /* Arrange cursor */
    // buf->cursorY = currentLine(buf)->linenumber - topLine(buf)->linenumber;
    buf->visualpos = currentLine(buf)->bwidth + COLPOS(&currentLine(buf)->l, buf->pos) - buf->currentColumn;
    // buf->cursorX = buf->visualpos - currentLine(buf)->bwidth;
#ifdef DISPLAY_DEBUG
    fprintf(stderr,
        "arrangeCursor: column=%d, cursorX=%d, visualpos=%d, pos=%d, len=%d\n",
        buf->currentColumn, buf->cursorX, buf->visualpos, buf->pos,
        currentLine(buf)->len);
#endif
}

void arrangeLine(struct Buffer* buf)
{
    int i, cpos;

    if (buf->firstLine == NULL)
        return;
    // buf->cursorY = currentLine(buf)->linenumber - topLine(buf)->linenumber;
    i = columnPos(&currentLine(buf)->l, buf->currentColumn + buf->visualpos - currentLine(buf)->bwidth);
    cpos = COLPOS(&currentLine(buf)->l, i) - buf->currentColumn;
    if (cpos >= 0) {
        // buf->cursorX = cpos;
        buf->pos = i;
    } else if (currentLine(buf)->l.len > i) {
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
        currentLine(buf)->len);
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
    buf->topLineIndex = lineSkip(buf, buf->firstLine, orig->topLineIndex - 1, false)->linenumber;
    gotoLine(buf, orig->currentLineIndex);
    buf->pos = orig->pos;
    if (currentLine(buf) && currentLine(orig))
        buf->pos += currentLine(orig)->bpos - currentLine(buf)->bpos;
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

char* url_decode2(const char* url, const struct Buffer* buf)
{
    if (!DecodeURL)
        return (char*)url;
    wc_ces url_charset = buf ? buf->document_charset : 0;
    return url_unquote_conv((char*)url, url_charset);
}

int columnSkip(struct Buffer* buf, int offset)
{
    int column = buf->currentColumn + offset;
    int nlines = getScreen()->ROWS + 1;

    int maxColumn = 0;
    struct LineList* l = topLine(buf);
    for (int i = 0; i < nlines && l != NULL; i++, l = l->next) {
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
        for (int i = getScreen()->ROWS - 1 - (lastLine(buf)->linenumber - l->linenumber);
            i > 0 && l->prev != NULL; i--, l = l->prev)
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
        for (i = 0; i < offset && l->next != NULL; i++, l = l->next)
            ;
    else
        for (i = 0; i < -offset && l->prev != NULL; i++, l = l->prev)
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

struct Buffer*
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
    return loadHTMLString(src, WC_CES_UTF_8);
}

struct Int2 updateCursor(struct Buffer* buf, struct Int2 viewport_size,
    struct Int2 viewport_cursor, struct Int2 cursor_delta, bool* hasScroll)
{
    int x = viewport_cursor.x + cursor_delta.x;
    if (x < 0) {
        // left
        x = 0;
        *hasScroll = true;
    } else if (x >= viewport_size.x) {
        // right
        x = viewport_size.x - 1;
        *hasScroll = true;
    }

    int y = viewport_cursor.y + cursor_delta.y;
    if (y < 0) {
        // up
        buf->topLineIndex += y;
        y = 0;
        *hasScroll = true;
    } else if (y >= viewport_size.y) {
        // down
        buf->topLineIndex += (1 + y - viewport_size.y);
        y = viewport_size.y - 1;
        *hasScroll = true;
    }

    return (struct Int2) {
        .x = x,
        .y = y,
    };
}
