#include "buffer.h"
#include "input_stream.h"
#include "terms.h"
#include "html_form.h"
#include "frame.h"
#include "alloc.h"
#include "line.h"
#include "etc.h"
#include "file.h"
#include "message.h"
#include "display.h"
#include "ctrlcode.h"
#include "anchor.h"
#include "tab.h"
#include "w3m_rc.h"
#include "image.h"
#include <string.h>
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
    } else if (IS_EMPTY_PARSED_URL(&buf->content.url))
        return NULL;
    else
        return &buf->content.url;
}

void cmd_loadBuffer(struct Buffer* buf, int prop, enum LinkBufferID linkid)
{
    if (buf == NULL) {
        disp_err_message("Can't load string", FALSE);
    } else {
        buf->bufferprop |= (BP_INTERNAL | prop);
        if (!(buf->bufferprop & BP_NO_URL))
            copyParsedURL(&buf->content.url, &Currentbuf->content.url);
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
    n->doc.COLS = TTY_COLS();
    n->doc.LINES = LASTLINE();
    n->content.url.scheme = SCM_UNKNOWN;
    n->baseURL = NULL;
    n->baseTarget = NULL;
    n->doc.title = "";
    n->bufferprop = BP_NORMAL;
    n->clone = New(int);
    *n->clone = 1;
    n->trbyte = 0;
    n->content.ssl_certificate = NULL;
    n->auto_detect = WcOption.auto_detect;
    n->check_url = getRuntime()->MarkAllPages; /* use default from -o mark_all_pages */
    return n;
}

/*
 * Create null buffer
 */
struct Buffer*
nullBuffer(void)
{
    struct Buffer* b = newBuffer(TTY_COLS());
    b->doc.title = "*Null*";
    return b;
}

/*
 * clearBuffer: clear buffer content
 */
void clearBuffer(struct Buffer* buf)
{
    buf->doc.firstLine = buf->doc.topLine = buf->doc.currentLine = buf->doc.lastLine = NULL;
    buf->doc.allLine = 0;
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
    if (buf->content.sourcefile) {
        unlink(buf->content.sourcefile);
    }
    if (buf->content.header_source)
        unlink(buf->content.header_source);
    if (buf->content.mailcap_source)
        unlink(buf->content.mailcap_source);
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

    if (!strcmp(first->doc.title, name)) {
        return first;
    }
    for (buf = first; buf->nextBuffer != NULL; buf = buf->nextBuffer) {
        if (!strcmp(buf->nextBuffer->doc.title, name)) {
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
    int all = buf->doc.allLine;
    if (all == 0 && buf->doc.lastLine != NULL)
        all = buf->doc.lastLine->linenumber;
    screen_move(n, 0);
    Str msg = Sprintf("<%s> [%d lines]", buf->doc.title, all);
    if (buf->content.filename != NULL) {
        switch (buf->content.url.scheme) {
        case SCM_LOCAL:
        case SCM_LOCAL_CGI:
            if (strcmp(buf->content.url.file, "-")) {
                Strcat_char(msg, ' ');
                Strcat_charp(msg, conv_from_system(buf->content.url.real_file));
            }
            break;
        case SCM_UNKNOWN:
        case SCM_MISSING:
            break;
        default:
            Strcat_char(msg, ' ');
            Strcat(msg, parsedURL2Str(&buf->content.url));
            break;
        }
    }
    screen_wc_addnstr_sup(msg->ptr, TTY_COLS() - 1);
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

    struct input_stream* stream = NULL;
    if (buf->content.mailcap_source) {
        stream = decompress_stream(examineFile(buf->content.mailcap_source), buf->content.mailcap_source);
    } else if (buf->content.sourcefile) {
        stream = decompress_stream(examineFile(buf->content.sourcefile), buf->content.sourcefile);
    }
    if (!stream)
        return;

    struct Buffer sbuf;
    copyBuffer(&sbuf, buf);
    clearBuffer(buf);
    while (buf->frameset) {
        deleteFrameSet(buf->frameset);
        buf->frameset = popFrameTree(&(buf->frameQ));
    }

    buf->doc.href = NULL;
    buf->doc.name = NULL;
    buf->doc.img = NULL;
    buf->doc.formitem = NULL;
    buf->doc.formlist = NULL;
    buf->doc.linklist = NULL;
    buf->doc.maplist = NULL;
    if (buf->doc.hmarklist)
        buf->doc.hmarklist->nmark = 0;
    if (buf->doc.imarklist)
        buf->doc.imarklist->nmark = 0;

    if (buf->content.header_source) {
        if (buf->content.url.scheme != SCM_LOCAL || buf->content.mailcap_source || !strcmp(buf->content.url.file, "-")) {
            struct input_stream* stream = decompress_stream(examineFile(buf->content.header_source), buf->content.header_source);
            if (stream) {
                getHttpResponseHeader(&buf->content, buf->content.url, stream);
                is_close(stream);
            }
        }
    }

    {
        wc_uint8 old_auto_detect = WcOption.auto_detect;
        WcOption.auto_detect = WC_OPT_DETECT_OFF;
        getRuntime()->UseContentCharset = FALSE;
        if (is_html_type(buf->type))
            loadHTMLBuffer(buf->content.url, stream,
                NULL, buf, buf->bufferprop & BP_FRAME);
        else
            loadBuffer(buf->content.url, stream,
                NULL, buf, buf->bufferprop & BP_FRAME);
        is_close(stream);
        WcOption.auto_detect = old_auto_detect;
        getRuntime()->UseContentCharset = TRUE;
    }

    if (buf->doc.firstLine && sbuf.doc.firstLine) {
        struct Line* cur = sbuf.doc.currentLine;
        int n;

        buf->doc.pos = sbuf.doc.pos + cur->bpos;
        while (cur->bpos && cur->prev)
            cur = cur->prev;
        if (cur->real_linenumber > 0)
            doc_gotoRealLine(&buf->doc, cur->real_linenumber);
        else
            doc_gotoLine(&buf->doc, cur->linenumber);
        n = (buf->doc.currentLine->linenumber - buf->doc.topLine->linenumber)
            - (cur->linenumber - sbuf.doc.topLine->linenumber);
        if (n) {
            buf->doc.topLine = doc_lineSkip(&buf->doc, buf->doc.topLine, n);
            if (cur->real_linenumber > 0)
                doc_gotoRealLine(&buf->doc, cur->real_linenumber);
            else
                doc_gotoLine(&buf->doc, cur->linenumber);
        }
        buf->doc.pos -= buf->doc.currentLine->bpos;
        if (getRuntime()->FoldLine && !is_html_type(buf->type))
            buf->doc.currentColumn = 0;
        else
            buf->doc.currentColumn = sbuf.doc.currentColumn;
        doc_arrangeCursor(&buf->doc);
    }
    if (buf->check_url & CHK_URL)
        chkURLBuffer(buf);
    // if (buf->check_url & CHK_NMID)
    //     chkNMIDBuffer(buf);
    formResetBuffer(buf, sbuf.doc.formitem);
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
