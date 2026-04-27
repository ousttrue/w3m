#include "buffer.h"
#include "file.h"
#include "http_response.h"
#include "content_type.h"
#include "UrlFile.h"
#include "main.h"
#include "term_tty.h"
#include "ctrlcode.h"
#include "html.h"
#include "frame.h"
#include "terms.h"
#include "input_stream.h"
#include "alloc.h"
#include "anchor.h"
#include "wc_util.h"
#include "url.h"
#include "etc.h"
#include "display.h"
#include "global.h"
#include <unistd.h>

char* NullLine = "";
Lineprop NullProp[] = { 0 };

int REV_LB[MAX_LB] = {
    LB_N_FRAME,
    LB_FRAME,
    LB_N_INFO,
    LB_INFO,
    LB_N_SOURCE,
};

/*
 * struct Buffer creation
 */
struct Buffer*
newBuffer(int width)
{
    struct Buffer* n;

    n = New(struct Buffer);
    if (n == NULL)
        exit(3);
    memset((void*)n, 0, sizeof(struct Buffer));
    n->width = width;
    n->COLS = COLS;
    n->LINES = (LINES - 1);
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
    n->check_url = MarkAllPages; /* use default from -o mark_all_pages */
    n->need_reshape = 1; /* always reshape new buffers to mark URLs */
    return n;
}

/*
 * Create null buffer
 */
struct Buffer*
nullBuffer(void)
{
    struct Buffer* b;

    b = newBuffer(COLS);
    b->buffername = "*Null*";
    return b;
}

/*
 * clearBuffer: clear buffer content
 */
void clearBuffer(struct Buffer* buf)
{
    buf->firstLine = buf->topLine = buf->currentLine = buf->lastLine = NULL;
    buf->allLine = 0;
}

/*
 * discardBuffer: free buffer structure
 */

void discardBuffer(struct Buffer* buf)
{
    int i;
    struct Buffer* b;

    deleteImage(buf);
    clearBuffer(buf);
    for (i = 0; i < MAX_LB; i++) {
        b = buf->linkBuffer[i];
        if (b == NULL)
            continue;
        b->linkBuffer[REV_LB[i]] = NULL;
    }
    if (buf->savecache)
        unlink(buf->savecache);
    if (--(*buf->clone))
        return;
    if (buf->pagerSource) {
        if (ist_destroy(buf->pagerSource)) {
            buf->pagerSource = NULL;
        }
    }
    if (buf->sourcefile && (!buf->real_type || strncasecmp(buf->real_type, "image/", 6))) {
        if (buf->real_scheme != SCM_FILE || buf->bufferprop & BP_FRAME)
            unlink(buf->sourcefile);
    }
    if (buf->header_source)
        unlink(buf->header_source);
    if (buf->mailcap_source)
        unlink(buf->mailcap_source);
    while (buf->frameset) {
        deleteFrameSet(buf->frameset);
        buf->frameset = popFrameTree(&(buf->frameQ));
    }
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
    Str msg;
    int all;

    all = buf->allLine;
    if (all == 0 && buf->lastLine != NULL)
        all = buf->lastLine->linenumber;
    move(n, 0);
    /* FIXME: gettextize? */
    msg = Sprintf("<%s> [%d lines]", buf->buffername, all);
    if (buf->filename != NULL) {
        switch (buf->currentURL.scheme) {
        case SCM_FILE:
        case SCM_LOCAL_CGI:
            if (strcmp(buf->currentURL.file, "-")) {
                Strcat_char(msg, ' ');
                Strcat_charp(msg, conv_from_system(buf->currentURL.real_file));
            }
            break;
        default:
            Strcat_char(msg, ' ');
            Strcat(msg, parsedURL2Str(&buf->currentURL));
            break;
        }
    }
    addnstr_sup(msg->ptr, COLS - 1);
}

/*
 * gotoLine: go to line number
 */
void gotoLine(struct Buffer* buf, int n)
{
    char msg[36];
    struct Line* l = buf->firstLine;

    if (l == NULL)
        return;
    if (buf->pagerSource && !(buf->bufferprop & BP_CLOSE)) {
        if (buf->lastLine->linenumber < n)
            getNextPage(buf, n - buf->lastLine->linenumber);
        while ((buf->lastLine->linenumber < n) && (getNextPage(buf, 1) != NULL))
            ;
    }
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
        buf->topLine = lineSkip(buf, buf->currentLine, -(buf->LINES - 1),
            false);
        return;
    }
    for (; l != NULL; l = l->next) {
        if (l->linenumber >= n) {
            buf->currentLine = l;
            if (n < buf->topLine->linenumber || buf->topLine->linenumber + buf->LINES <= n)
                buf->topLine = lineSkip(buf, l, -(buf->LINES + 1) / 2, false);
            break;
        }
    }
}

/*
 * gotoRealLine: go to real line number
 */
void gotoRealLine(struct Buffer* buf, int n)
{
    char msg[36];
    struct Line* l = buf->firstLine;

    if (l == NULL)
        return;
    if (buf->pagerSource && !(buf->bufferprop & BP_CLOSE)) {
        if (buf->lastLine->real_linenumber < n)
            getNextPage(buf, n - buf->lastLine->real_linenumber);
        while ((buf->lastLine->real_linenumber < n) && (getNextPage(buf, 1) != NULL))
            ;
    }
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
        buf->topLine = lineSkip(buf, buf->currentLine, -(buf->LINES - 1),
            false);
        return;
    }
    for (; l != NULL; l = l->next) {
        if (l->real_linenumber >= n) {
            buf->currentLine = l;
            if (n < buf->topLine->real_linenumber || buf->topLine->real_linenumber + buf->LINES <= n)
                buf->topLine = lineSkip(buf, l, -(buf->LINES + 1) / 2, false);
            break;
        }
    }
}

static struct Buffer*
listBuffer(struct Buffer* top, struct Buffer* current)
{
    int i, c = 0;
    struct Buffer* buf = top;

    move(0, 0);
    if (useColor) {
        setfcolor(basic_color);
        setbcolor(bg_color);
    }
    clrtobotx();
    for (i = 0; i < (LINES - 1); i++) {
        if (buf == current) {
            c = i;
            standout();
        }
        writeBufferName(buf, i);
        if (buf == current) {
            standend();
            clrtoeolx();
            move(i, 0);
            toggle_stand();
        } else
            clrtoeolx();
        if (buf->nextBuffer == NULL) {
            move(i + 1, 0);
            clrtobotx();
            break;
        }
        buf = buf->nextBuffer;
    }
    standout();
    /* FIXME: gettextize? */
    message("struct Buffer selection mode: SPC for select / D for delete buffer", 0,
        0);
    standend();
    /*
     * move((LINES-1), COLS - 1); */
    move(c, 0);
    refresh();
    return buf->nextBuffer;
}

/*
 * Select buffer visually
 */
struct Buffer*
selectBuffer(struct CmdArgs* args, struct Buffer* firstbuf, struct Buffer* currentbuf, char* selectchar)
{
    int i, cpoint, /* Current struct Buffer Number */
        spoint, /* Current Line on Screen */
        maxbuf, sclimit = (LINES - 1); /* Upper limit of line * number in
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

    for (;;) {
        if ((c = getch(args)) == ESC_CODE) {
            if ((c = getch(args)) == '[' || c == 'O') {
                switch (c = getch(args)) {
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
                standout();
                writeBufferName(currentbuf, spoint);
                standend();
                move(spoint, 0);
                toggle_stand();
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
                standout();
                writeBufferName(currentbuf, spoint);
                standend();
                move(spoint, 0);
                toggle_stand();
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
            return currentbuf;
        }
        /*
         * move((LINES-1), COLS - 1);
         */
        move(spoint, 0);
        refresh();
    }
}

/*
 * Reshape HTML buffer
 */
void reshapeBuffer(struct CmdArgs* args, struct Buffer* buf)
{
    struct Buffer sbuf;
    wc_uint8 old_auto_detect = WcOption.auto_detect;

    if (!buf->need_reshape)
        return;
    buf->need_reshape = false;
    buf->width = INIT_BUFFER_WIDTH;
    if (buf->sourcefile == NULL)
        return;

    struct URLFile f = examineFile(buf->mailcap_source ? buf->mailcap_source : buf->sourcefile);
    if (f.stream == NULL)
        return;
    copyBuffer(&sbuf, buf);
    clearBuffer(buf);
    while (buf->frameset) {
        deleteFrameSet(buf->frameset);
        buf->frameset = popFrameTree(&(buf->frameQ));
    }

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

    if (buf->header_source) {
        if (buf->currentURL.scheme != SCM_FILE || buf->mailcap_source || !strcmp(buf->currentURL.file, "-")) {
            struct URLFile h = examineFile(buf->header_source);
            if (h.stream) {
                struct HttpResponse res = http_response_header(h.stream, h.scheme);
                if (!buf->header_source) {
                    buf->header_source = http_response_save_header_source(&res);
                }
                http_response_process(&res, args, &h, NULL);
                buf->document_header = res.headers;
                UFclose(&h);
            }
        } else if (buf->search_header) { /* -m option */
            struct HttpResponse res = http_response_header(f.stream, f.scheme);
            if (!buf->header_source) {
                buf->header_source = http_response_save_header_source(&res);
            }
            http_response_process(&res, args, &f, NULL);
            buf->document_header = res.headers;
        }
    }

    WcOption.auto_detect = WC_OPT_DETECT_OFF;
    UseContentCharset = false;
    if (is_html_type(buf->type))
        loadHTMLBuffer(args, &f, buf);
    else
        loadBuffer(args, &f, buf);
    UFclose(&f);
    WcOption.auto_detect = old_auto_detect;
    UseContentCharset = true;

    buf->height = (LINES - 1) + 1;
    if (buf->firstLine && sbuf.firstLine) {
        struct Line* cur = sbuf.currentLine;
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
        if (FoldLine && !is_html_type(buf->type))
            buf->currentColumn = 0;
        else
            buf->currentColumn = sbuf.currentColumn;
        arrangeCursor(buf);
    }
    if (buf->check_url & CHK_URL)
        chkURLBuffer(buf);
    if (buf->check_url & CHK_NMID)
        chkNMIDBuffer(buf);
    formResetBuffer(buf, sbuf.formitem);
}

/* shallow copy */
void copyBuffer(struct Buffer* a, struct Buffer* b)
{
    readBufferCache(b);
    bcopy((void*)b, (void*)a, sizeof(struct Buffer));
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
    FILE* cache = NULL;
    struct Line* l;
    int colorflag;

    if (buf->savecache)
        return -1;

    if (buf->firstLine == NULL)
        goto _error1;

    buf->savecache = tmpfname(TMPF_CACHE, NULL);
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

int readBufferCache(struct Buffer* buf)
{
    FILE* cache;
    struct Line *l = NULL, *prevl = NULL, *basel = NULL;
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
        l = New(struct Line);
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

void showImageProgress(struct Buffer* buf)
{
    if (!buf)
        return;

    struct AnchorList* al = buf->img;
    if (!al)
        return;

    struct Anchor* a = al->anchors;
    int n = 0;
    int l = 0;
    for (int i = 0; i < al->nanchor; i++, a++) {
        if (a->image && a->hseq >= 0) {
            n++;
            if (a->image->cache && a->image->cache->loaded & IMG_FLAG_LOADED)
                l++;
        }
    }
    if (n) {
        if (enable_inline_image && n == l)
            drawImage();
        message(Sprintf("%d/%d images loaded", l, n)->ptr,
            buf->cursorX + buf->rootX, buf->cursorY + buf->rootY);
        refresh();
    }
}

static void
addnewline2(struct Buffer* buf, char* line, Lineprop* prop, Linecolor* color, int pos,
    int nlines)
{
    struct Line* l;
    l = New(struct Line);
    l->next = NULL;
    l->lineBuf = line;
    l->propBuf = prop;
    l->colorBuf = color;
    l->len = pos;
    l->width = -1;
    l->size = pos;
    l->bpos = 0;
    l->bwidth = 0;
    l->prev = buf->currentLine;
    if (buf->currentLine) {
        l->next = buf->currentLine->next;
        buf->currentLine->next = l;
    } else
        l->next = NULL;
    if (buf->lastLine == NULL || buf->lastLine == buf->currentLine)
        buf->lastLine = l;
    buf->currentLine = l;
    if (buf->firstLine == NULL)
        buf->firstLine = l;
    l->linenumber = ++buf->allLine;
    if (nlines < 0) {
        /*     l->real_linenumber = l->linenumber;     */
        l->real_linenumber = 0;
    } else {
        l->real_linenumber = nlines;
    }
    l = NULL;
}

void addnewline(struct Buffer* buf, char* line, Lineprop* prop, Linecolor* color, int pos, int width, int nlines)
{
    char* s;
    Lineprop* p;
    Linecolor* c;
    struct Line* l;
    int i, bpos, bwidth;

    if (pos > 0) {
        s = allocStr(line, pos);
        p = NewAtom_N(Lineprop, pos);
        bcopy((void*)prop, (void*)p, pos * sizeof(Lineprop));
    } else {
        s = NullLine;
        p = NullProp;
    }
    if (pos > 0 && color) {
        c = NewAtom_N(Linecolor, pos);
        bcopy((void*)color, (void*)c, pos * sizeof(Linecolor));
    } else {
        c = NULL;
    }
    addnewline2(buf, s, p, c, pos, nlines);
    if (pos <= 0 || width <= 0)
        return;
    bpos = 0;
    bwidth = 0;
    while (1) {
        l = buf->currentLine;
        l->bpos = bpos;
        l->bwidth = bwidth;
        i = columnLen(l, width);
        if (i == 0) {
            i++;
            while (i < l->len && p[i] & PC_WCHAR2)
                i++;
        }
        l->len = i;
        l->width = COLPOS(l, l->len);
        if (pos <= i)
            return;
        bpos += l->len;
        bwidth += l->width;
        s += i;
        p += i;
        if (c)
            c += i;
        pos -= i;
        addnewline2(buf, s, p, c, pos, nlines);
    }
}

int currentLn(struct Buffer* buf)
{
    if (buf->currentLine)
        /*     return buf->currentLine->real_linenumber + 1;      */
        return buf->currentLine->linenumber + 1;
    else
        return 1;
}
