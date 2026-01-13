#include "buffer.h"
#include "linein.h"
#include "siteconf.h"
#include "tab_list.h"
#include "maparea.h"
#include "indep.h"
#include "input_stream.h"
#include "screen.h"
#include "html_form.h"
#include "alloc.h"
#include "line.h"
#include "etc.h"
#include "file.h"
#include "message.h"
#include "display.h"
#include "ctrlcode.h"
#include "tab.h"
#include "w3m_rc.h"
#include "image.h"
#include <libwc/status.h>
#include <libwc/charset.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

char* NullLine = "";
Lineprop NullProp[] = { 0 };

int REV_LB[MAX_LB] = {
    LB_N_INFO,
    LB_INFO,
    LB_N_SOURCE,
};

struct Buffer* buf_new(struct Content* content)
{
    struct Buffer* buf = New(struct Buffer);
    *buf = (struct Buffer) {
        .content = content,
        .doc = NULL,
        .nextBuffer = NULL,
        .linkBuffer = { 0 },
        .bufferprop = BP_NORMAL,
        .clone = New(int),
        .check_url = getRuntime()->MarkAllPages, /* use default from -o mark_all_pages */
        .savecache = NULL,
        .edit = NULL,
        .event = NULL,
    };
    *buf->clone = 1;
    return buf;
}

struct Url*
buf_baseUrl(struct Buffer* buf)
{
    if (buf->bufferprop & BP_NO_URL) {
        // no URL is defined for the buffer
        return NULL;
    }
    if (buf->doc && buf->doc->baseURL) {
        // <BASE> tag is defined in the document
        return buf->doc->baseURL;
    }
    if (buf->content && !IS_EMPTY_PARSED_URL(&buf->content->url)) {
        // content URL
        return &buf->content->url;
    }
    return NULL;
}

void buf_set_link(struct Buffer* buf,
    struct Buffer* link_buf, enum BufferPropertyFlags prop, enum LinkBufferID linkid)
{
    link_buf->bufferprop |= (BP_INTERNAL | prop);
    if (!(link_buf->bufferprop & BP_NO_URL)) {
        if (link_buf->content && buf->content && link_buf->content != buf->content) {
            copyParsedURL(&link_buf->content->url, &buf->content->url);
        }
    }
    if (linkid != LB_NOLINK) {
        link_buf->linkBuffer[REV_LB[linkid]] = buf;
        buf->linkBuffer[linkid] = link_buf;
    }
}

/*
 * discardBuffer: free buffer structure
 */

void discardBuffer(struct Buffer* buf)
{
    int i;
    struct Buffer* b;

    deleteImage(buf);
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
    if (buf->content) {
        if (buf->content->sourcefile)
            unlink(buf->content->sourcefile);
        if (buf->content->header_source)
            unlink(buf->content->header_source);
        if (buf->content->mailcap_source)
            unlink(buf->content->mailcap_source);
    }
}

/*
 * namedBuffer: Select buffer which have specified name
 */
struct Buffer*
namedBuffer(struct Buffer* first, char* name)
{
    struct Buffer* buf;

    if (!strcmp(first->doc->title, name)) {
        return first;
    }
    for (buf = first; buf->nextBuffer != NULL; buf = buf->nextBuffer) {
        if (!strcmp(buf->nextBuffer->doc->title, name)) {
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
    int all = buf->doc->allLine;
    if (all == 0 && buf->doc->lastLine != NULL)
        all = buf->doc->lastLine->linenumber;
    screen_move((struct Vec2) { .y = n, .x = 0 });
    Str msg = Sprintf("<%s> [%d lines]", buf->doc->title, all);
    if (buf->content->filename != NULL) {
        switch (buf->content->url.scheme) {
        case SCM_LOCAL:
        case SCM_LOCAL_CGI:
            if (strcmp(buf->content->url.file, "-")) {
                Strcat_char(msg, ' ');
                Strcat_charp(msg, conv_from_system(buf->content->url.real_file));
            }
            break;
        case SCM_UNKNOWN:
        case SCM_MISSING:
            break;
        default:
            Strcat_char(msg, ' ');
            Strcat(msg, parsedURL2Str(&buf->content->url));
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

    screen_move((struct Vec2) { 0 });
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
            screen_move((struct Vec2) { .y = i, .x = 0 });
            screen_toggle_stand();
        } else
            screen_clrtoeolx();
        if (buf->nextBuffer == NULL) {
            screen_move((struct Vec2) { .y = i + 1, .x = 0 });
            screen_clrtobotx();
            break;
        }
        buf = buf->nextBuffer;
    }
    screen_standout();
    message("Buffer selection mode: SPC for select / D for delete buffer");
    screen_standend();
    /*
     * move(LASTLINE(), COLS - 1); */
    screen_move((struct Vec2) { .y = c, .x = 0 });
    tty_write_screen();
    return buf->nextBuffer;
}

/*
 * Select buffer visually
 */
struct Buffer*
selectBuffer(struct Buffer* firstbuf, struct Buffer* currentbuf, char* selectchar)
{
    int i = 0;
    int cpoint = 0;
    for (struct Buffer* buf = firstbuf; buf != NULL; buf = buf->nextBuffer) {
        if (buf == currentbuf)
            cpoint = i;
        i++;
    }
    int maxbuf = i;

    int sclimit = LASTLINE();
    int spoint;
    struct Buffer* topbuf;
    if (cpoint >= sclimit) {
        spoint = sclimit / 2;
        topbuf = nthBuffer(firstbuf, cpoint - spoint);
    } else {
        topbuf = firstbuf;
        spoint = cpoint;
    }
    listBuffer(topbuf, currentbuf);

    for (;;) {
        char c;
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
                screen_standout();
                writeBufferName(currentbuf, spoint);
                screen_standend();
                screen_move((struct Vec2) { .y = spoint, .x = 0 });
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
                screen_move((struct Vec2) { .y = spoint, .x = 0 });
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
        screen_move((struct Vec2) { .y = spoint, .x = 0 });
    }
}

void reshapeBuffer(struct Buffer* buf)
{
    struct input_stream* stream = NULL;
    if (buf->content->mailcap_source) {
        stream = decompress_stream(examineFile(buf->content->mailcap_source), buf->content->mailcap_source);
    } else if (buf->content->sourcefile) {
        stream = decompress_stream(examineFile(buf->content->sourcefile), buf->content->sourcefile);
    }
    if (!stream)
        return;

    if (buf->content->header_source) {
        if (buf->content->url.scheme != SCM_LOCAL || buf->content->mailcap_source || !strcmp(buf->content->url.file, "-")) {
            struct input_stream* stream = decompress_stream(examineFile(buf->content->header_source), buf->content->header_source);
            if (stream) {
                getHttpResponseHeader(buf->content, buf->content->url, stream);
                is_close(stream);
            }
        }
    }

    if (is_html_type(buf->content->content_type))
        loadHTMLBuffer(buf->content->url, stream,
            NULL, buf, buf->bufferprop & BP_FRAME);
    else
        loadBuffer(buf->content->url, stream,
            NULL, buf, buf->bufferprop & BP_FRAME);
    is_close(stream);
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

    if (buf->doc->firstLine == NULL)
        goto _error1;

    Str tmp = tmpfname(TMPF_CACHE, NULL);
    buf->savecache = tmp->ptr;
    cache = fopen(buf->savecache, "w");
    if (!cache)
        goto _error1;

    if (fwrite1(buf->doc->currentLine->linenumber, cache) || fwrite1(buf->doc->topLine->linenumber, cache))
        goto _error;

    for (l = buf->doc->firstLine; l; l = l->next) {
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
            buf->doc->firstLine = l;
        l->linenumber = lnum;
        if (lnum == clnum)
            buf->doc->currentLine = l;
        if (lnum == tlnum)
            buf->doc->topLine = l;
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
        buf->doc->lastLine = prevl;
        buf->doc->lastLine->next = NULL;
    }
    fclose(cache);
    unlink(buf->savecache);
    buf->savecache = NULL;
    return true;
}

bool checkBackBuffer(struct Buffer* buf)
{
    if (buf->nextBuffer)
        return TRUE;

    return FALSE;
}

Str page_info_panel(struct Buffer* buf)
{
    Str tmp = Strnew_size(1024);
    Strcat_charp(tmp, "<html><head>\
<title>Information about current page</title>\
</head><body>\
<h1>Information about current page</h1>\n");
    if (buf == NULL)
        goto end;
    int all = buf->doc->allLine;
    if (all == 0 && buf->doc->lastLine)
        all = buf->doc->lastLine->linenumber;
    Strcat_charp(tmp, "<form method=internal action=charset>");
    const char* p = url_decode2(buf_baseUrl(buf), buf->doc, parsedURL2Str(&buf->content->url)->ptr);
    Strcat_m_charp(tmp, "<table cellpadding=0>",
        "<tr valign=top><td nowrap>Title<td>",
        html_quote(buf->doc->title),
        "<tr valign=top><td nowrap>Current URL<td>",
        html_quote(p),
        "<tr valign=top><td nowrap>Document Type<td>",
        "unknown",
        "<tr valign=top><td nowrap>Last Modified<td>",
        html_quote(last_modified(buf)), NULL);

    if (buf->doc->charset != getRuntime()->InnerCharset) {
        wc_ces_list* list = wc_get_ces_list();
        Strcat_charp(tmp,
            "<tr><td nowrap>Document Charset<td><select name=charset>");
        for (; list->name != NULL; list++) {
            char charset[16];
            sprintf(charset, "%d", (unsigned int)list->id);
            Strcat_m_charp(tmp, "<option value=", charset,
                (buf->doc->charset == list->id) ? " selected>"
                                                : ">",
                list->desc, NULL);
        }
        Strcat_charp(tmp, "</select>");
        Strcat_charp(tmp, "<tr><td><td><input type=submit value=Change>");
    }

    Strcat_m_charp(tmp,
        "<tr valign=top><td nowrap>Number of lines<td>",
        Sprintf("%d", all)->ptr,
        "<tr valign=top><td nowrap>Transferred bytes<td>",
        Sprintf("%lu", (unsigned long)buf->doc->trbyte)->ptr, NULL);

    struct Anchor* a = doc_retrieveCurrentAnchor(buf->doc);
    if (a != NULL) {
        struct Url pu;
        parseURL2(a->url, &pu, buf_baseUrl(buf));
        p = parsedURL2Str(&pu)->ptr;
        const char* q = html_quote(p);
        if (getRuntime()->DecodeURL)
            p = html_quote(url_decode2(buf_baseUrl(buf), buf->doc, p));
        else
            p = q;
        Strcat_m_charp(tmp,
            "<tr valign=top><td nowrap>URL of current anchor<td><a href=\"",
            q, "\">", p, "</a>", NULL);
    }
    a = doc_retrieveCurrentImg(buf->doc);
    if (a != NULL) {
        struct Url pu;
        parseURL2(a->url, &pu, buf_baseUrl(buf));
        p = parsedURL2Str(&pu)->ptr;
        const char* q = html_quote(p);
        if (getRuntime()->DecodeURL)
            p = html_quote(url_decode2(buf_baseUrl(buf), buf->doc, p));
        else
            p = q;
        Strcat_m_charp(tmp,
            "<tr valign=top><td nowrap>URL of current image<td><a href=\"",
            q, "\">", p, "</a>", NULL);
    }
    a = doc_retrieveCurrentForm(buf->doc);
    if (a != NULL) {
        struct FormItemList* fi = (struct FormItemList*)a->url;
        p = form2str(fi);
        p = html_quote(url_decode2(buf_baseUrl(buf), buf->doc, p));
        Strcat_m_charp(tmp,
            "<tr valign=top><td nowrap>Method/type of current form&nbsp;<td>",
            p, NULL);
        if (fi->parent->method == FORM_METHOD_INTERNAL
            && !Strcmp_charp(fi->parent->action, "map"))
            append_map_info(buf_baseUrl(buf), buf->doc, tmp, fi->parent->item);
    }
    Strcat_charp(tmp, "</table>\n");
    Strcat_charp(tmp, "</form>");

    append_link_info(buf_baseUrl(buf), buf->doc, tmp, buf->doc->linklist);

    if (buf->content->document_header != NULL) {
        Strcat_charp(tmp, "<hr width=50%><h1>Header information</h1><pre>\n");
        for (TextListItem* ti = buf->content->document_header->first; ti != NULL; ti = ti->next)
            Strcat_m_charp(tmp, "<pre_int>", html_quote(ti->ptr),
                "</pre_int>\n", NULL);
        Strcat_charp(tmp, "</pre>\n");
    }

    if (buf->content->ssl_certificate)
        Strcat_m_charp(tmp, "<h1>SSL certificate</h1><pre>\n",
            html_quote(buf->content->ssl_certificate), "</pre>\n", NULL);
end:
    Strcat_charp(tmp, "</body></html>");
    return tmp;
}

static struct FormItemList*
save_submit_formlist(struct FormItemList* src)
{
    struct FormList* list;
    struct FormList* srclist;
    struct FormItemList* srcitem;
    struct FormItemList* item;
    struct FormItemList* ret = NULL;
    struct FormSelectOptionItem* opt;
    struct FormSelectOptionItem* curopt;
    struct FormSelectOptionItem* srcopt;

    if (src == NULL)
        return NULL;
    srclist = src->parent;
    list = New(struct FormList);
    list->method = srclist->method;
    list->action = Strdup(srclist->action);
    list->charset = srclist->charset;
    list->enctype = srclist->enctype;
    list->nitems = srclist->nitems;
    list->body = srclist->body;
    list->boundary = srclist->boundary;
    list->length = srclist->length;

    for (srcitem = srclist->item; srcitem; srcitem = srcitem->next) {
        item = New(struct FormItemList);
        item->type = srcitem->type;
        item->name = Strdup(srcitem->name);
        item->value = Strdup(srcitem->value);
        item->checked = srcitem->checked;
        item->accept = srcitem->accept;
        item->size = srcitem->size;
        item->rows = srcitem->rows;
        item->maxlength = srcitem->maxlength;
        item->readonly = srcitem->readonly;

        opt = curopt = NULL;
        for (srcopt = srcitem->select_option; srcopt; srcopt = srcopt->next) {
            if (!srcopt->checked)
                continue;
            opt = New(struct FormSelectOptionItem);
            opt->value = Strdup(srcopt->value);
            opt->label = Strdup(srcopt->label);
            opt->checked = srcopt->checked;
            if (item->select_option == NULL) {
                item->select_option = curopt = opt;
            } else {
                curopt->next = opt;
                curopt = curopt->next;
            }
        }
        item->select_option = opt;
        if (srcitem->label)
            item->label = Strdup(srcitem->label);

        item->parent = list;
        item->next = NULL;

        if (list->lastitem == NULL) {
            list->item = list->lastitem = item;
        } else {
            list->lastitem->next = item;
            list->lastitem = item;
        }

        if (srcitem == src)
            ret = item;
    }

    return ret;
}

static struct Buffer* do_submit(struct Buffer* buf, struct Anchor* a, struct FormItemList* fi,
    const char* p,
    struct FollowOption option)
{
    int multipart = (fi->parent->method == FORM_METHOD_POST && fi->parent->enctype == FORM_ENCTYPE_MULTIPART);
    Str tmp = query_from_followform(buf, fi, multipart);

    Str tmp2 = Strdup(fi->parent->action);
    if (!Strcmp_charp(tmp2, "!CURRENT_URL!")) {
        /* It means "current URL" */
        tmp2 = parsedURL2Str(&buf->content->url);
        if ((p = strchr(tmp2->ptr, '?')) != NULL)
            Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
    }

    if (fi->parent->method == FORM_METHOD_GET) {
        if ((p = strchr(tmp2->ptr, '?')) != NULL)
            Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
        Strcat_charp(tmp2, "?");
        Strcat(tmp2, tmp);
        return loadLink(tmp2->ptr, NULL, a->target, NULL, option);
    } else if (fi->parent->method == FORM_METHOD_POST) {
        if (multipart) {
            struct stat st;
            stat(fi->parent->body, &st);
            fi->parent->length = st.st_size;
        } else {
            fi->parent->body = tmp->ptr;
            fi->parent->length = tmp->length;
        }
        struct Buffer* new_buf = loadLink(tmp2->ptr, fi->parent, a->target, NULL, option);
        tab_push_buffer(CurrentTab(), new_buf);
        if (multipart) {
            unlink(fi->parent->body);
        }
        if (new_buf && !(new_buf->bufferprop & BP_REDIRECTED)) { /* buf must be Currentbuf */
            /* BP_REDIRECTED means that the buffer is obtained through
             * Location: header. In this case, buf->form_submit must not be set
             * because the page is not loaded by POST method but GET method.
             */
            new_buf->doc->form_submit = save_submit_formlist(fi);
        }
        return new_buf;
    } else if ((fi->parent->method == FORM_METHOD_INTERNAL
                   && (!Strcmp_charp(fi->parent->action, "map")
                       || !Strcmp_charp(fi->parent->action, "none")))
        || buf->bufferprop & BP_INTERNAL) { /* internal */
        do_internal(tmp2->ptr, tmp->ptr);
    } else {
        disp_err_message("Can't send form because of illegal method.", false);
    }
    return NULL;
}

static struct Buffer* form_follow(struct Buffer* buf, struct Anchor* a, struct FormItemList* fi, struct FollowOption option, bool submit)
{
    switch (fi->type) {
    case FORM_INPUT_TEXT: {
        if (submit) {
            return do_submit(buf, a, fi, NULL, option);
        }
        if (fi->readonly) {
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
        }
        const char* p = inputStrHist("TEXT:", fi->value ? fi->value->ptr : NULL, g_runtime.TextHist);
        if (!p || fi->readonly)
            break;
        fi->value = Strnew_charp(p);
        doc_formUpdateBuffer(buf->doc, a, fi);
        if (fi->accept || fi->parent->nitems == 1) {
            return do_submit(buf, a, fi, p, option);
        }
        break;
    }

    case FORM_INPUT_FILE: {
        if (submit) {
            return do_submit(buf, a, fi, NULL, option);
        }
        if (fi->readonly)
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
        const char* p = inputFilenameHist("Filename:", fi->value ? fi->value->ptr : NULL, NULL);
        if (!p || fi->readonly)
            break;
        fi->value = Strnew_charp(p);
        doc_formUpdateBuffer(buf->doc, a, fi);
        if (fi->accept || fi->parent->nitems == 1) {
            return do_submit(buf, a, fi, p, option);
        }
        break;
    }

    case FORM_INPUT_PASSWORD: {
        if (submit) {
            return do_submit(buf, a, fi, NULL, option);
        }
        if (fi->readonly) {
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
            break;
        }
        const char* p = inputLine("Password:", fi->value ? fi->value->ptr : NULL, IN_PASSWORD);
        if (!p)
            break;
        fi->value = Strnew_charp(p);
        doc_formUpdateBuffer(buf->doc, a, fi);
        if (fi->accept) {
            return do_submit(buf, a, fi, p, option);
        }
        break;
    }

    case FORM_TEXTAREA:
        if (submit) {
            return do_submit(buf, a, fi, NULL, option);
        }
        if (fi->readonly) {
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
            break;
        }
        input_textarea(fi);
        doc_formUpdateBuffer(buf->doc, a, fi);
        break;

    case FORM_INPUT_RADIO:
        if (submit) {
            return do_submit(buf, a, fi, NULL, option);
        }
        if (fi->readonly) {
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
            break;
        }
        formRecheckRadio(buf, a, fi);
        break;

    case FORM_INPUT_CHECKBOX:
        if (submit) {
            return do_submit(buf, a, fi, NULL, option);
        }
        if (fi->readonly) {
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
            break;
        }
        fi->checked = !fi->checked;
        doc_formUpdateBuffer(buf->doc, a, fi);
        break;

    case FORM_SELECT:
        if (submit) {
            return do_submit(buf, a, fi, NULL, option);
        }
        if (!formChooseOptionByMenu(fi,
                buf->doc->cursorX - buf->doc->pos + a->start.pos + buf->doc->rootX,
                buf->doc->cursorY + buf->doc->rootY))
            break;
        doc_formUpdateBuffer(buf->doc, a, fi);
        if (fi->parent->nitems == 1) {
            return do_submit(buf, a, fi, NULL, option);
        }
        break;

    case FORM_INPUT_IMAGE:
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
        return do_submit(buf, a, fi, NULL, option);

    case FORM_INPUT_RESET:
        for (int i = 0; i < buf->doc->formitem.nanchor; i++) {
            struct Anchor* a2 = &buf->doc->formitem.anchors[i];
            struct FormItemList* f2 = (struct FormItemList*)a2->url;
            if (f2->parent == fi->parent
                && f2->name
                && f2->value
                && f2->type != FORM_INPUT_SUBMIT
                && f2->type != FORM_INPUT_HIDDEN
                && f2->type != FORM_INPUT_RESET) {
                f2->value = f2->init_value;
                f2->checked = f2->init_checked;
                f2->label = f2->init_label;
                f2->selected = f2->init_selected;
                doc_formUpdateBuffer(buf->doc, a2, f2);
            }
        }
        break;

    case FORM_INPUT_HIDDEN:
    default:
        break;
    }

    return 0;
}

struct FollowResult buf_followForm(struct Buffer* buf, struct FollowOption option, bool submit)
{
    if (!buf->doc->firstLine)
        return (struct FollowResult) { 0 };

    struct Anchor* a = doc_retrieveCurrentForm(buf->doc);
    if (!a)
        return (struct FollowResult) { 0 };

    struct FormItemList* fi = (struct FormItemList*)a->url;

    struct Buffer* new_buf = form_follow(buf, a, fi, option, submit);
    return (struct FollowResult) {
        .anchor = a,
        .new_buf = new_buf,
    };
}

struct FollowResult buf_followA(struct Buffer* buf, struct FollowOption option)
{
    if (buf->doc->firstLine == NULL) {
        return (struct FollowResult) { 0 };
    }

    struct Anchor* a = doc_retrieveCurrentImg(buf->doc);
    if (a && a->image && a->image->map) {
        return buf_followForm(buf, option, false);
    }

    int x = 0, y = 0, map = 0;
    if (a && a->image && a->image->ismap) {
        getMapXY(buf->doc, a, &x, &y);
        map = 1;
    }

    a = doc_retrieveCurrentAnchor(buf->doc);
    if (a == NULL) {
        return buf_followForm(buf, option, false);
    }
    if (*a->url == '#') { /* index within this buffer */
        return gotoLabel(buf, a->url + 1);
    }

    struct Url u;
    parseURL2(a->url, &u, buf_baseUrl(buf));
    if (Strcmp(parsedURL2Str(&u), parsedURL2Str(&buf->content->url)) == 0) {
        /* index within this buffer */
        if (u.label) {
            return gotoLabel(buf, u.label);
        }
    }
    if (handleMailto(a->url))
        return (struct FollowResult) { 0 };

    const char* url = a->url;
    if (map)
        url = Sprintf("%s?%d,%d", a->url, x, y)->ptr;

    return (struct FollowResult) {
        .anchor = a,
        .new_buf = loadLink(url, NULL, a->target, a->referer, option),
    };
}

struct Buffer* loadLink(const char* url, struct FormList* request,
    const char* target, const char* referer, struct FollowOption option)
{
    message(Sprintf("loading %s", url)->ptr);

    const int* no_referer_ptr = query_SCONF_NO_REFERER_FROM(&CurrentTab()->currentBuffer->content->url);
    struct Url* base = buf_baseUrl(Currentbuf);
    if ((no_referer_ptr && *no_referer_ptr) || base == NULL || base->scheme == SCM_LOCAL || base->scheme == SCM_LOCAL_CGI)
        referer = NO_REFERER;
    if (referer == NULL)
        referer = parsedURL2RefererStr(&Currentbuf->content->url)->ptr;
    if (option.do_download) {
        download_content(url, request,
            (struct LoadOption) { .base_url = buf_baseUrl(Currentbuf), .referer = referer, .flag = 0 });
        return NULL;
    }

    struct Content* content = get_content_cache(url, request,
        (struct LoadOption) { .base_url = buf_baseUrl(Currentbuf), .referer = referer, .flag = 0 });
    if (!content) {
        char* emsg = Sprintf("Can't load %s", url)->ptr;
        disp_err_message(emsg, FALSE);
        return NULL;
    }
    struct Buffer* buf = buf_new(content);

    // struct Url pu;
    // parseURL2(url, &pu, base);
    // pushHashHist(g_runtime.URLHist, parsedURL2Str(&pu)->ptr);
    //
    // if (!option.on_target) /* open link as an indivisual page */
    //     return loadNormalBuf(buf, TRUE);
    //
    // if (option.do_download) /* download (thus no need to render frames) */
    //     return loadNormalBuf(buf, FALSE);
    //
    // if (target == NULL || /* no target specified (that means this page is not a frame page) */
    //     !strcmp(target, "_top") || /* this link is specified to be opened as an indivisual * page */
    //     !(Currentbuf->bufferprop & BP_FRAME) /* This page is not a frame page */
    // ) {
    //     return loadNormalBuf(buf, TRUE);
    // }
    // struct Buffer* nfbuf = Currentbuf->linkBuffer[LB_N_FRAME];
    // if (nfbuf == NULL) {
    //     /* original page (that contains <frameset> tag) doesn't exist */
    //     return loadNormalBuf(buf, TRUE);
    // }
    //
    // union frameset_element* f_element = search_frame(nfbuf->doc.frameset, target);
    // if (f_element == NULL) {
    //     /* specified target doesn't exist in this frameset */
    //     return loadNormalBuf(buf, TRUE);
    // }
    //
    // /* frame page */
    //
    // /* stack current frameset */
    // pushFrameTree(&(nfbuf->doc.frameQ), copyFrameSet(nfbuf->doc.frameset), Currentbuf);
    // /* delete frame view buffer */
    // delBuffer(Currentbuf);
    // Currentbuf = nfbuf;
    // /* nfbuf->frameset = copyFrameSet(nfbuf->frameset); */
    // resetFrameElement(f_element, buf, referer, request);
    // discardBuffer(buf);
    // rFrame();
    // {
    //     struct Anchor* al = NULL;
    //     char* label = pu.label;
    //
    //     if (label && f_element->element->attr == F_BODY) {
    //         al = searchAnchor(f_element->body->nameList, label);
    //     }
    //     if (!al) {
    //         label = Strnew_m_charp("_", target, NULL)->ptr;
    //         al = searchURLLabel(Currentbuf->doc, label);
    //     }
    //     if (al) {
    //         doc_gotoLine(&Currentbuf->doc, al->start.line);
    //         if (g_runtime.label_topline)
    //             Currentbuf->doc.topLine = doc_lineSkip(&Currentbuf->doc, Currentbuf->doc.topLine,
    //                 Currentbuf->doc.currentLine->linenumber - Currentbuf->doc.topLine->linenumber);
    //         Currentbuf->doc.pos = al->start.pos;
    //         doc_arrangeCursor(&Currentbuf->doc);
    //     }
    // }
    return buf;
}
