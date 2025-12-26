#include "buffer.h"
#include "URLFile.h"
#include "etc.h"
#include "file.h"
#include "message.h"
#include "display.h"
#include "ctrlcode.h"
#include "anchor.h"
#include "tab.h"
#include "w3m_rc.h"
#include "image.h"
#include "fm.h"
#include <unistd.h>
#include <assert.h>

int REV_LB[MAX_LB] = {
    LB_N_FRAME,
    LB_FRAME,
    LB_N_INFO,
    LB_INFO,
    LB_N_SOURCE,
};

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

void cmd_loadBuffer(struct Buffer* buf, int prop, enum LinkBufferID linkid)
{
    if (buf == NULL) {
        disp_err_message("Can't load string", FALSE);
    } else if (buf != NO_BUFFER) {
        buf->bufferprop |= (BP_INTERNAL | prop);
        if (!(buf->bufferprop & BP_NO_URL))
            copyParsedURL(&buf->currentURL, &Currentbuf->currentURL);
        if (linkid != LB_NOLINK) {
            buf->linkBuffer[REV_LB[linkid]] = Currentbuf;
            Currentbuf->linkBuffer[linkid] = buf;
        }
        pushBuffer(buf);
    }
}

char* NullLine = "";
Lineprop NullProp[] = { 0 };

struct Buffer* newBuffer(int width)
{
    struct Buffer* n = New(struct Buffer);
    assert(n);
    memset(n, 0, sizeof(struct Buffer));
    n->width = width;
    n->COLS = TTY_COLS();
    n->LINES = LASTLINE();
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
    return n;
}

/*
 * Create null buffer
 */
struct Buffer*
nullBuffer(void)
{
    struct Buffer* b;

    b = newBuffer(TTY_COLS());
    b->buffername = "*Null*";
    return b;
}

/*
 * clearBuffer: clear buffer content
 */
void clearBuffer(struct Buffer* buf)
{
    buf->doc.firstLine = buf->doc.topLine = buf->doc.currentLine = buf->doc.lastLine = NULL;
    buf->allLine = 0;
}

/*
 * discardBuffer: free buffer structure
 */

void discardBuffer(struct Buffer* buf)
{
    int i;
    struct Buffer* b;

#ifdef USE_IMAGE
    deleteImage(buf);
#endif
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
    if (buf->pagerSource)
        ISclose(buf->pagerSource);
    if (buf->sourcefile && (!buf->real_type || strncasecmp(buf->real_type, "image/", 6))) {
        if (buf->real_scheme != SCM_LOCAL || buf->bufferprop & BP_FRAME)
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
    if (all == 0 && buf->doc.lastLine != NULL)
        all = buf->doc.lastLine->linenumber;
    screen_move(n, 0);
    /* FIXME: gettextize? */
    msg = Sprintf("<%s> [%d lines]", buf->buffername, all);
    if (buf->content.filename != NULL) {
        switch (buf->currentURL.scheme) {
        case SCM_LOCAL:
        case SCM_LOCAL_CGI:
            if (strcmp(buf->currentURL.file, "-")) {
                Strcat_char(msg, ' ');
                Strcat_charp(msg, conv_from_system(buf->currentURL.real_file));
            }
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
    screen_wc_addnstr_sup(msg->ptr, TTY_COLS() - 1);
}

/*
 * gotoLine: go to line number
 */
void gotoLine(struct Buffer* buf, int n)
{
    char msg[36];
    struct Line* l = buf->doc.firstLine;

    if (l == NULL)
        return;
    if (buf->pagerSource && !(buf->bufferprop & BP_CLOSE)) {
        if (buf->doc.lastLine->linenumber < n)
            getNextPage(buf, n - buf->doc.lastLine->linenumber);
        while ((buf->doc.lastLine->linenumber < n) && (getNextPage(buf, 1) != NULL))
            ;
    }
    if (l->linenumber > n) {
        /* FIXME: gettextize? */
        sprintf(msg, "First line is #%ld", l->linenumber);
        set_delayed_message(msg);
        buf->doc.topLine = buf->doc.currentLine = l;
        return;
    }
    if (buf->doc.lastLine->linenumber < n) {
        l = buf->doc.lastLine;
        /* FIXME: gettextize? */
        sprintf(msg, "Last line is #%ld", buf->doc.lastLine->linenumber);
        set_delayed_message(msg);
        buf->doc.currentLine = l;
        buf->doc.topLine = lineSkip(buf, buf->doc.currentLine, -(buf->LINES - 1),
            FALSE);
        return;
    }
    for (; l != NULL; l = l->next) {
        if (l->linenumber >= n) {
            buf->doc.currentLine = l;
            if (n < buf->doc.topLine->linenumber || buf->doc.topLine->linenumber + buf->LINES <= n)
                buf->doc.topLine = lineSkip(buf, l, -(buf->LINES + 1) / 2, FALSE);
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
    struct Line* l = buf->doc.firstLine;

    if (l == NULL)
        return;
    if (buf->pagerSource && !(buf->bufferprop & BP_CLOSE)) {
        if (buf->doc.lastLine->real_linenumber < n)
            getNextPage(buf, n - buf->doc.lastLine->real_linenumber);
        while ((buf->doc.lastLine->real_linenumber < n) && (getNextPage(buf, 1) != NULL))
            ;
    }
    if (l->real_linenumber > n) {
        /* FIXME: gettextize? */
        sprintf(msg, "First line is #%ld", l->real_linenumber);
        set_delayed_message(msg);
        buf->doc.topLine = buf->doc.currentLine = l;
        return;
    }
    if (buf->doc.lastLine->real_linenumber < n) {
        l = buf->doc.lastLine;
        /* FIXME: gettextize? */
        sprintf(msg, "Last line is #%ld", buf->doc.lastLine->real_linenumber);
        set_delayed_message(msg);
        buf->doc.currentLine = l;
        buf->doc.topLine = lineSkip(buf, buf->doc.currentLine, -(buf->LINES - 1),
            FALSE);
        return;
    }
    for (; l != NULL; l = l->next) {
        if (l->real_linenumber >= n) {
            buf->doc.currentLine = l;
            if (n < buf->doc.topLine->real_linenumber || buf->doc.topLine->real_linenumber + buf->LINES <= n)
                buf->doc.topLine = lineSkip(buf, l, -(buf->LINES + 1) / 2, FALSE);
            break;
        }
    }
}

static struct Buffer*
listBuffer(struct Buffer* top, struct Buffer* current)
{
    int i, c = 0;
    struct Buffer* buf = top;

    screen_move(0, 0);
    if (getRuntime()->useColor) {
        screen_setfcolor(getRuntime()->basic_color);
        screen_setbcolor(getRuntime()->bg_color);
    }
    screen_clrtobotx();
    for (i = 0; i < LASTLINE(); i++) {
        if (buf == current) {
            c = i;
            screen_standout();
        }
        writeBufferName(buf, i);
        if (buf == current) {
            screen_standend();
            screen_clrtoeolx();
            screen_move(i, 0);
            screen_toggle_stand();
        } else
            screen_clrtoeolx();
        if (buf->nextBuffer == NULL) {
            screen_move(i + 1, 0);
            screen_clrtobotx();
            break;
        }
        buf = buf->nextBuffer;
    }
    screen_standout();
    /* FIXME: gettextize? */
    message("Buffer selection mode: SPC for select / D for delete buffer", 0,
        0);
    screen_standend();
    /*
     * move(LASTLINE(), COLS - 1); */
    screen_move(c, 0);
    return buf->nextBuffer;
}

/*
 * Select buffer visually
 */
struct Buffer*
selectBuffer(struct Buffer* firstbuf, struct Buffer* currentbuf, char* selectchar)
{
    int i, cpoint, /* Current Buffer Number */
        spoint, /* Current Line on Screen */
        maxbuf, sclimit = LASTLINE(); /* Upper limit of line * number in
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
#ifdef __EMX__
        else if (!c)
            switch (getch()) {
            case K_UP:
                c = 'k';
                break;
            case K_DOWN:
                c = 'j';
                break;
            case K_RIGHT:
                c = ' ';
                break;
            case K_LEFT:
                c = 'B';
            }
#endif
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
                screen_standout();
                writeBufferName(currentbuf, spoint);
                screen_standend();
                screen_move(spoint, 0);
                screen_toggle_stand();
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
                screen_standout();
                writeBufferName(currentbuf, spoint);
                screen_standend();
                screen_move(spoint, 0);
                screen_toggle_stand();
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
         * move(LASTLINE(), COLS - 1);
         */
        screen_move(spoint, 0);
    }
}

void reshapeBuffer(struct Buffer* buf)
{
    buf->width = INIT_BUFFER_WIDTH;
    if (!buf->sourcefile)
        return;

    struct URLFile f;
    init_stream(&f, SCM_LOCAL, NULL);
    examineFile(buf->mailcap_source ? buf->mailcap_source : buf->sourcefile, &f, false);
    if (!f.stream)
        return;

    struct Buffer sbuf;
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
        if (buf->currentURL.scheme != SCM_LOCAL || buf->mailcap_source || !strcmp(buf->currentURL.file, "-")) {
            struct URLFile h;
            init_stream(&h, SCM_LOCAL, NULL);
            examineFile(buf->header_source, &h, false);
            if (h.stream) {
                readHeader(&h, buf, TRUE, NULL);
                UFclose(&h);
            }
        } else if (buf->search_header) /* -m option */
            readHeader(&f, buf, TRUE, NULL);
    }

    {
        wc_uint8 old_auto_detect = WcOption.auto_detect;
        WcOption.auto_detect = WC_OPT_DETECT_OFF;
        UseContentCharset = FALSE;
        if (is_html_type(buf->type))
            loadHTMLBuffer(&f, buf);
        else
            loadBuffer(&f, buf);
        UFclose(&f);
        WcOption.auto_detect = old_auto_detect;
        UseContentCharset = TRUE;
    }

    if (buf->doc.firstLine && sbuf.doc.firstLine) {
        struct Line* cur = sbuf.doc.currentLine;
        int n;

        buf->pos = sbuf.pos + cur->bpos;
        while (cur->bpos && cur->prev)
            cur = cur->prev;
        if (cur->real_linenumber > 0)
            gotoRealLine(buf, cur->real_linenumber);
        else
            gotoLine(buf, cur->linenumber);
        n = (buf->doc.currentLine->linenumber - buf->doc.topLine->linenumber)
            - (cur->linenumber - sbuf.doc.topLine->linenumber);
        if (n) {
            buf->doc.topLine = lineSkip(buf, buf->doc.topLine, n, FALSE);
            if (cur->real_linenumber > 0)
                gotoRealLine(buf, cur->real_linenumber);
            else
                gotoLine(buf, cur->linenumber);
        }
        buf->pos -= buf->doc.currentLine->bpos;
        if (getRuntime()->FoldLine && !is_html_type(buf->type))
            buf->currentColumn = 0;
        else
            buf->currentColumn = sbuf.currentColumn;
        arrangeCursor(buf);
    }
    if (buf->check_url & CHK_URL)
        chkURLBuffer(buf);
    if (buf->check_url & CHK_NMID)
        chkNMIDBuffer(buf);
    if (buf->real_scheme == SCM_NNTP || buf->real_scheme == SCM_NEWS)
        reAnchorNewsheader(buf);
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

    if (buf->doc.firstLine == NULL)
        goto _error1;

    Str tmp = tmpfname(TMPF_CACHE, NULL);
    buf->savecache = tmp->ptr;
    cache = fopen(buf->savecache, "w");
    if (!cache)
        goto _error1;

    if (fwrite1(buf->doc.currentLine->linenumber, cache) || fwrite1(buf->doc.topLine->linenumber, cache))
        goto _error;

    for (l = buf->doc.firstLine; l; l = l->next) {
        if (fwrite1(l->real_linenumber, cache) || fwrite1(l->usrflags, cache) || fwrite1(l->width, cache) || fwrite1(l->len, cache) || fwrite1(l->size, cache) || fwrite1(l->bpos, cache) || fwrite1(l->bwidth, cache))
            goto _error;
        if (l->bpos == 0) {
            if (fwrite(l->lineBuf, 1, l->size, cache) < l->size || fwrite(l->propBuf, sizeof(Lineprop), l->size, cache) < l->size)
                goto _error;
        }
#ifdef USE_ANSI_COLOR
        colorflag = l->colorBuf ? 1 : 0;
        if (fwrite1(colorflag, cache))
            goto _error;
        if (colorflag) {
            if (l->bpos == 0) {
                if (fwrite(l->colorBuf, sizeof(Linecolor), l->size, cache) < l->size)
                    goto _error;
            }
        }
#endif
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

bool readBufferCache(struct Buffer* buf)
{
    if (!buf->savecache) {
        return false;
    }

    long clnum, tlnum;
    FILE* cache = fopen(buf->savecache, "r");
    if (!cache || fread1(clnum, cache) || fread1(tlnum, cache)) {
        if (cache) {
            fclose(cache);
        }
        buf->savecache = NULL;
        return false;
    }

    struct Line* l = NULL;
    struct Line* prevl = NULL;
    struct Line* basel = NULL;
    int lnum = 0;
    while (!feof(cache)) {
        lnum++;
        prevl = l;
        l = New(struct Line);
        l->prev = prevl;
        if (prevl)
            prevl->next = l;
        else
            buf->doc.firstLine = l;
        l->linenumber = lnum;
        if (lnum == clnum)
            buf->doc.currentLine = l;
        if (lnum == tlnum)
            buf->doc.topLine = l;
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

        int colorflag;
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
        buf->doc.lastLine = prevl;
        buf->doc.lastLine->next = NULL;
    }
    fclose(cache);
    unlink(buf->savecache);
    buf->savecache = NULL;
    return true;
}

void delBuffer(struct Buffer* buf)
{
    if (buf == NULL)
        return;
    if (Currentbuf == buf)
        Currentbuf = buf->nextBuffer;
    Firstbuf = deleteBuffer(Firstbuf, buf);
    if (!Currentbuf)
        Currentbuf = Firstbuf;
}

void restorePosition(struct Buffer* buf, struct Buffer* orig)
{
    buf->doc.topLine = lineSkip(buf, buf->doc.firstLine, TOP_LINENUMBER(orig) - 1,
        FALSE);
    gotoLine(buf, CUR_LINENUMBER(orig));
    buf->pos = orig->pos;
    if (buf->doc.currentLine && orig->doc.currentLine)
        buf->pos += orig->doc.currentLine->bpos - buf->doc.currentLine->bpos;
    buf->currentColumn = orig->currentColumn;
    arrangeCursor(buf);
}

void cursorXY(struct Buffer* buf, int x, int y)
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

void arrangeLine(struct Buffer* buf)
{
    int i, cpos;

    if (buf->doc.firstLine == NULL)
        return;
    buf->cursorY = buf->doc.currentLine->linenumber - buf->doc.topLine->linenumber;
    i = columnPos(buf->doc.currentLine, buf->currentColumn + buf->visualpos - buf->doc.currentLine->bwidth);
    cpos = COLPOS(buf->doc.currentLine, i) - buf->currentColumn;
    if (cpos >= 0) {
        buf->cursorX = cpos;
        buf->pos = i;
    } else if (buf->doc.currentLine->len > i) {
        buf->cursorX = 0;
        buf->pos = i + 1;
    } else {
        buf->cursorX = 0;
        buf->pos = 0;
    }
}

void cursorUp0(struct Buffer* buf, int n)
{
    if (buf->cursorY > 0)
        cursorUpDown(buf, -1);
    else {
        buf->doc.topLine = lineSkip(buf, buf->doc.topLine, -n, FALSE);
        if (buf->doc.currentLine->prev != NULL)
            buf->doc.currentLine = buf->doc.currentLine->prev;
        arrangeLine(buf);
    }
}

void cursorUp(struct Buffer* buf, int n)
{
    struct Line* l = buf->doc.currentLine;
    if (buf->doc.firstLine == NULL)
        return;
    while (buf->doc.currentLine->prev && buf->doc.currentLine->bpos)
        cursorUp0(buf, n);
    if (buf->doc.currentLine == buf->doc.firstLine) {
        gotoLine(buf, l->linenumber);
        arrangeLine(buf);
        return;
    }
    cursorUp0(buf, n);
    while (buf->doc.currentLine->prev && buf->doc.currentLine->bpos && buf->doc.currentLine->bwidth >= buf->currentColumn + buf->visualpos)
        cursorUp0(buf, n);
}

void cursorDown0(struct Buffer* buf, int n)
{
    if (buf->cursorY < buf->LINES - 1)
        cursorUpDown(buf, 1);
    else {
        buf->doc.topLine = lineSkip(buf, buf->doc.topLine, n, FALSE);
        if (buf->doc.currentLine->next != NULL)
            buf->doc.currentLine = buf->doc.currentLine->next;
        arrangeLine(buf);
    }
}

void cursorDown(struct Buffer* buf, int n)
{
    struct Line* l = buf->doc.currentLine;
    if (buf->doc.firstLine == NULL)
        return;
    while (buf->doc.currentLine->next && buf->doc.currentLine->next->bpos)
        cursorDown0(buf, n);
    if (buf->doc.currentLine == buf->doc.lastLine) {
        gotoLine(buf, l->linenumber);
        arrangeLine(buf);
        return;
    }
    cursorDown0(buf, n);
    while (buf->doc.currentLine->next && buf->doc.currentLine->next->bpos && buf->doc.currentLine->bwidth + buf->doc.currentLine->width < buf->currentColumn + buf->visualpos)
        cursorDown0(buf, n);
}

void cursorUpDown(struct Buffer* buf, int n)
{
    struct Line* cl = buf->doc.currentLine;

    if (buf->doc.firstLine == NULL)
        return;
    if ((buf->doc.currentLine = currentLineSkip(buf, cl, n, FALSE)) == cl)
        return;
    arrangeLine(buf);
}

void cursorRight(struct Buffer* buf, int n)
{
    int i, delta = 1, cpos, vpos2;
    struct Line* l = buf->doc.currentLine;

    if (buf->doc.firstLine == NULL)
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

void cursorLeft(struct Buffer* buf, int n)
{
    int i, delta = 1, cpos;
    struct Line* l = buf->doc.currentLine;

    if (buf->doc.firstLine == NULL)
        return;
    i = buf->pos;
    Lineprop* p = l->propBuf;
    while (i - delta > 0 && p[i - delta] & PC_WCHAR2)
        delta++;
    if (i >= delta)
        buf->pos = i - delta;
    else if (l->prev && l->bpos) {
        cursorUp0(buf, -1);
        buf->pos = buf->doc.currentLine->len - 1;
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

void cursorHome(struct Buffer* buf)
{
    buf->visualpos = 0;
    buf->cursorX = buf->cursorY = 0;
}

/*
 * Arrange line,column and cursor position according to current line and
 * current position.
 */
void arrangeCursor(struct Buffer* buf)
{
    int col, col2, pos;
    int delta = 1;
    if (buf == NULL || buf->doc.currentLine == NULL)
        return;
    /* Arrange line */
    if (buf->doc.currentLine->linenumber - buf->doc.topLine->linenumber >= buf->LINES
        || buf->doc.currentLine->linenumber < buf->doc.topLine->linenumber) {
        /*
         * buf->doc.topLine = buf->doc.currentLine;
         */
        buf->doc.topLine = lineSkip(buf, buf->doc.currentLine, 0, FALSE);
    }
    /* Arrange column */
    while (buf->pos < 0 && buf->doc.currentLine->prev && buf->doc.currentLine->bpos) {
        pos = buf->pos + buf->doc.currentLine->prev->len;
        cursorUp0(buf, 1);
        buf->pos = pos;
    }
    while (buf->pos >= buf->doc.currentLine->len && buf->doc.currentLine->next && buf->doc.currentLine->next->bpos) {
        pos = buf->pos - buf->doc.currentLine->len;
        cursorDown0(buf, 1);
        buf->pos = pos;
    }
    if (buf->doc.currentLine->len == 0 || buf->pos < 0)
        buf->pos = 0;
    else if (buf->pos >= buf->doc.currentLine->len)
        buf->pos = buf->doc.currentLine->len - 1;
    while (buf->pos > 0 && buf->doc.currentLine->propBuf[buf->pos] & PC_WCHAR2)
        buf->pos--;
    col = COLPOS(buf->doc.currentLine, buf->pos);
    while (buf->pos + delta < buf->doc.currentLine->len && buf->doc.currentLine->propBuf[buf->pos + delta] & PC_WCHAR2)
        delta++;
    col2 = COLPOS(buf->doc.currentLine, buf->pos + delta);
    if (col < buf->currentColumn || col2 > buf->COLS + buf->currentColumn) {
        buf->currentColumn = 0;
        if (col2 > buf->COLS)
            columnSkip(buf, col);
    }
    /* Arrange cursor */
    buf->cursorY = buf->doc.currentLine->linenumber - buf->doc.topLine->linenumber;
    buf->visualpos = buf->doc.currentLine->bwidth + COLPOS(buf->doc.currentLine, buf->pos) - buf->currentColumn;
    buf->cursorX = buf->visualpos - buf->doc.currentLine->bwidth;
}

static void
addnewline2(struct Buffer* buf, char* line, Lineprop* prop, Linecolor* color, int pos, int nlines)
{
    struct Line* l;
    l = New(struct Line);
    l->next = NULL;
    l->lineBuf = line;
    l->propBuf = prop;
#ifdef USE_ANSI_COLOR
    l->colorBuf = color;
#endif
    l->len = pos;
    l->width = -1;
    l->size = pos;
    l->bpos = 0;
    l->bwidth = 0;
    l->prev = buf->doc.currentLine;
    if (buf->doc.currentLine) {
        l->next = buf->doc.currentLine->next;
        buf->doc.currentLine->next = l;
    } else
        l->next = NULL;
    if (buf->doc.lastLine == NULL || buf->doc.lastLine == buf->doc.currentLine)
        buf->doc.lastLine = l;
    buf->doc.currentLine = l;
    if (buf->doc.firstLine == NULL)
        buf->doc.firstLine = l;
    l->linenumber = ++buf->allLine;
    if (nlines < 0) {
        /*     l->real_linenumber = l->linenumber;     */
        l->real_linenumber = 0;
    } else {
        l->real_linenumber = nlines;
    }
    l = NULL;
}

void addnewline(struct Buffer* buf, const char* line, Lineprop* prop, Linecolor* color, int pos, int width, int nlines)
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
        l = buf->doc.currentLine;
        l->bpos = bpos;
        l->bwidth = bwidth;
        i = columnLen(l, width);
        if (i == 0) {
            i++;
#ifdef USE_M17N
            while (i < l->len && p[i] & PC_WCHAR2)
                i++;
#endif
        }
        l->len = i;
        l->width = COLPOS(l, l->len);
        if (pos <= i)
            return;
        bpos += l->len;
        bwidth += l->width;
        s += i;
        p += i;
#ifdef USE_ANSI_COLOR
        if (c)
            c += i;
#endif
        pos -= i;
        addnewline2(buf, s, p, c, pos, nlines);
    }
}
