#include "buffer.h"
#include "display.h"
#include "ctrlcode.h"
#include "anchor.h"
#include "tab.h"
#include "w3m_rc.h"
#include "image.h"
#include "fm.h"
#include <unistd.h>

int REV_LB[MAX_LB] = {
    LB_N_FRAME,
    LB_FRAME,
    LB_N_INFO,
    LB_INFO,
    LB_N_SOURCE,
};

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
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

#ifdef USE_MOUSE
#ifdef USE_GPM
#include <gpm.h>
#endif
#if defined(USE_GPM) || defined(USE_SYSMOUSE)
extern int do_getch();
#define getch() do_getch()
#endif /* USE_GPM */
#endif /* USE_MOUSE */

#ifdef __EMX__
#include <sys/kbdscan.h>
#include <strings.h>
#endif
char* NullLine = "";
Lineprop NullProp[] = { 0 };

/*
 * Buffer creation
 */
struct Buffer*
newBuffer(int width)
{
    struct Buffer* n;

    n = New(struct Buffer);
    if (n == NULL)
        exit(3);
    bzero((void*)n, sizeof(struct Buffer));
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
#ifdef USE_SSL
    n->ssl_certificate = NULL;
#endif
#ifdef USE_M17N
    n->auto_detect = WcOption.auto_detect;
#endif
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

    b = newBuffer(TTY_COLS());
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
    if (all == 0 && buf->lastLine != NULL)
        all = buf->lastLine->linenumber;
    screen_move(n, 0);
    /* FIXME: gettextize? */
    msg = Sprintf("<%s> [%d lines]", buf->buffername, all);
    if (buf->filename != NULL) {
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
    screen_addnstr_sup(msg->ptr, TTY_COLS() - 1);
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
            FALSE);
        return;
    }
    for (; l != NULL; l = l->next) {
        if (l->linenumber >= n) {
            buf->currentLine = l;
            if (n < buf->topLine->linenumber || buf->topLine->linenumber + buf->LINES <= n)
                buf->topLine = lineSkip(buf, l, -(buf->LINES + 1) / 2, FALSE);
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
            FALSE);
        return;
    }
    for (; l != NULL; l = l->next) {
        if (l->real_linenumber >= n) {
            buf->currentLine = l;
            if (n < buf->topLine->real_linenumber || buf->topLine->real_linenumber + buf->LINES <= n)
                buf->topLine = lineSkip(buf, l, -(buf->LINES + 1) / 2, FALSE);
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
    if (useColor) {
        screen_setfcolor(basic_color);
        screen_setbcolor(bg_color);
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
    tty_refresh();
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
        tty_refresh();
    }
}

/*
 * Reshape HTML buffer
 */
void reshapeBuffer(struct Buffer* buf)
{
    URLFile f;
    struct Buffer sbuf;
#ifdef USE_M17N
    wc_uint8 old_auto_detect = WcOption.auto_detect;
#endif

    if (!buf->need_reshape)
        return;
    buf->need_reshape = FALSE;
    buf->width = INIT_BUFFER_WIDTH;
    if (buf->sourcefile == NULL)
        return;
    init_stream(&f, SCM_LOCAL, NULL);
    examineFile(buf->mailcap_source ? buf->mailcap_source : buf->sourcefile,
        &f, false);
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
        if (buf->currentURL.scheme != SCM_LOCAL || buf->mailcap_source || !strcmp(buf->currentURL.file, "-")) {
            URLFile h;
            init_stream(&h, SCM_LOCAL, NULL);
            examineFile(buf->header_source, &h, false);
            if (h.stream) {
                readHeader(&h, buf, TRUE, NULL);
                UFclose(&h);
            }
        } else if (buf->search_header) /* -m option */
            readHeader(&f, buf, TRUE, NULL);
    }

#ifdef USE_M17N
    WcOption.auto_detect = WC_OPT_DETECT_OFF;
    UseContentCharset = FALSE;
#endif
    if (is_html_type(buf->type))
        loadHTMLBuffer(&f, buf);
    else
        loadBuffer(&f, buf);
    UFclose(&f);
#ifdef USE_M17N
    WcOption.auto_detect = old_auto_detect;
    UseContentCharset = TRUE;
#endif

    buf->height = LASTLINE() + 1;
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
            buf->topLine = lineSkip(buf, buf->topLine, n, FALSE);
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
#ifdef USE_NNTP
    if (buf->check_url & CHK_NMID)
        chkNMIDBuffer(buf);
    if (buf->real_scheme == SCM_NNTP || buf->real_scheme == SCM_NEWS)
        reAnchorNewsheader(buf);
#endif
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
    Str tmp;
    FILE* cache = NULL;
    struct Line* l;
#ifdef USE_ANSI_COLOR
    int colorflag;
#endif

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

int readBufferCache(struct Buffer* buf)
{
    FILE* cache;
    struct Line *l = NULL, *prevl = NULL, *basel = NULL;
    long lnum = 0, clnum, tlnum;
#ifdef USE_ANSI_COLOR
    int colorflag;
#endif

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
#ifdef USE_ANSI_COLOR
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
#endif
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
