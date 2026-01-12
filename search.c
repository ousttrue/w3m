#include "search.h"
#include "linein.h"
#include "document.h"
#include "message.h"
#include "buffer.h"
#include "w3m_rc.h"
#include "regex.h"
#include "mysignal.h"
#include <signal.h>
#include <unistd.h>
#include <libwc/status.h>

static void
set_mark(struct Line* l, int pos, int epos)
{
    for (; pos < epos && pos < l->size; pos++)
        l->propBuf[pos] |= PE_MARK;
}

#ifdef USE_MIGEMO
/* Migemo: romaji --> kana+kanji in regexp */
static FILE *migemor = NULL, *migemow = NULL;
static int migemo_running;
static int migemo_pid = 0;

void init_migemo()
{
    migemo_active = migemo_running = use_migemo;
    if (migemor != NULL)
        fclose(migemor);
    if (migemow != NULL)
        fclose(migemow);
    migemor = migemow = NULL;
    if (migemo_pid)
        kill(migemo_pid, SIGKILL);
    migemo_pid = 0;
}

static int
open_migemo(char* migemo_command)
{
    migemo_pid = open_pipe_rw(&migemor, &migemow);
    if (migemo_pid < 0)
        goto err0;
    if (migemo_pid == 0) {
        /* child */
        setup_child(FALSE, 2, -1);
        myExec(migemo_command);
        /* XXX: ifdef __EMX__, use start /f ? */
    }
    return 1;
err0:
    migemo_pid = 0;
    migemo_active = migemo_running = 0;
    return 0;
}

static char*
migemostr(char* str)
{
    Str tmp = NULL;
    if (migemor == NULL || migemow == NULL)
        if (open_migemo(migemo_command) == 0)
            return str;
    fprintf(migemow, "%s\n", conv_to_system(str));
again:
    if (fflush(migemow) != 0) {
        switch (errno) {
        case EINTR:
            goto again;
        default:
            goto err;
        }
    }
    tmp = Str_conv_from_system(Strfgets(migemor));
    Strchop(tmp);
    if (tmp->length == 0)
        goto err;
    return conv_search_string(tmp->ptr, SystemCharset);
err:
    /* XXX: backend migemo is not working? */
    init_migemo();
    migemo_active = migemo_running = 0;
    return str;
}
#endif /* USE_MIGEMO */

/* normalize search string */
const char* conv_search_string(const char* str, enum wc_ces f_ces, enum wc_ces doc_charset)
{
    if (getRuntime()->SearchConv && !WcOption.pre_conv && doc_charset != f_ces)
        str = wtf_conv_fit(str, doc_charset);
    return str;
}

enum SearchResult forwardSearch(struct DefunContext ctx, const char* str)
{
    bool wrapped = false;

    const char* p = regexCompile(str, getRuntime()->IgnoreCase);
    if (p) {
        message(p);
        return SR_NOTFOUND;
    }

    struct Line* l = ctx.buf->doc->currentLine;
    if (l == NULL) {
        return SR_NOTFOUND;
    }

    int pos = ctx.buf->doc->pos;
    if (l->bpos) {
        pos += l->bpos;
        while (l->bpos && l->prev)
            l = l->prev;
    }
    struct Line* begin = l;
    while (pos < l->size && l->propBuf[pos] & PC_WCHAR2)
        pos++;
    if (pos < l->size && regexMatch(&l->lineBuf[pos], l->size - pos, 0) == 1) {
        const char *first, *last;
        matchedPosition(&first, &last);
        pos = first - l->lineBuf;
        while (pos >= l->len && l->next && l->next->bpos) {
            pos -= l->len;
            l = l->next;
        }
        ctx.buf->doc->pos = pos;
        if (l != ctx.buf->doc->currentLine)
            doc_gotoLine(ctx.buf->doc, l->linenumber);
        doc_arrangeCursor(ctx.buf->doc);
        set_mark(l, pos, pos + last - first);
        return SR_FOUND;
    }
    for (l = l->next;; l = l->next) {
        if (l == NULL) {
            if (getRuntime()->WrapSearch) {
                l = ctx.buf->doc->firstLine;
                wrapped = TRUE;
            } else {
                break;
            }
        }
        if (l->bpos)
            continue;
        if (regexMatch(l->lineBuf, l->size, 1) == 1) {
            const char *first, *last;
            matchedPosition(&first, &last);
            pos = first - l->lineBuf;
            while (pos >= l->len && l->next && l->next->bpos) {
                pos -= l->len;
                l = l->next;
            }
            ctx.buf->doc->pos = pos;
            ctx.buf->doc->currentLine = l;
            doc_gotoLine(ctx.buf->doc, l->linenumber);
            doc_arrangeCursor(ctx.buf->doc);
            set_mark(l, pos, pos + last - first);
            return SR_FOUND | (wrapped ? SR_WRAPPED : 0);
        }
        if (wrapped && l == begin) /* no match */
            break;
    }
    return SR_NOTFOUND;
}

enum SearchResult backwardSearch(struct DefunContext ctx, const char* str)
{
    const char* p = regexCompile(str, getRuntime()->IgnoreCase);
    if (p) {
        message(p);
        return SR_NOTFOUND;
    }

    struct Line* l = ctx.buf->doc->currentLine;
    if (l == NULL) {
        return SR_NOTFOUND;
    }

    int pos = ctx.buf->doc->pos;
    if (l->bpos) {
        pos += l->bpos;
        while (l->bpos && l->prev)
            l = l->prev;
    }
    struct Line* begin = l;

    // char *p, *q, *found, *found_last, *first, *last;
    if (pos > 0) {
        pos--;
        while (pos > 0 && l->propBuf[pos] & PC_WCHAR2)
            pos--;
        p = &l->lineBuf[pos];
        const char* found = NULL;
        const char* found_last = NULL;
        const char* q = l->lineBuf;
        while (regexMatch(q, &l->lineBuf[l->size] - q, q == l->lineBuf) == 1) {
            const char *first, *last;
            matchedPosition(&first, &last);
            if (first <= p) {
                found = first;
                found_last = last;
            }
            if (q - l->lineBuf >= l->size)
                break;
            q++;
            while (q - l->lineBuf < l->size
                && l->propBuf[q - l->lineBuf] & PC_WCHAR2)
                q++;
            if (q > p)
                break;
        }
        if (found) {
            pos = found - l->lineBuf;
            while (pos >= l->len && l->next && l->next->bpos) {
                pos -= l->len;
                l = l->next;
            }
            ctx.buf->doc->pos = pos;
            if (l != ctx.buf->doc->currentLine)
                doc_gotoLine(ctx.buf->doc, l->linenumber);
            doc_arrangeCursor(ctx.buf->doc);
            set_mark(l, pos, pos + found_last - found);
            return SR_FOUND;
        }
    }

    bool wrapped = false;
    for (l = l->prev;; l = l->prev) {
        if (l == NULL) {
            if (getRuntime()->WrapSearch) {
                l = ctx.buf->doc->lastLine;
                wrapped = TRUE;
            } else {
                break;
            }
        }
        const char* found = NULL;
        const char* found_last = NULL;
        const char* q = l->lineBuf;
        while (regexMatch(q, &l->lineBuf[l->size] - q, q == l->lineBuf) == 1) {
            const char *first, *last;
            matchedPosition(&first, &last);
            found = first;
            found_last = last;
            if (q - l->lineBuf >= l->size)
                break;
            q++;
            while (q - l->lineBuf < l->size
                && l->propBuf[q - l->lineBuf] & PC_WCHAR2)
                q++;
        }
        if (found) {
            pos = found - l->lineBuf;
            while (pos >= l->len && l->next && l->next->bpos) {
                pos -= l->len;
                l = l->next;
            }
            ctx.buf->doc->pos = pos;
            doc_gotoLine(ctx.buf->doc, l->linenumber);
            doc_arrangeCursor(ctx.buf->doc);
            set_mark(l, pos, pos + found_last - found);
            return SR_FOUND | (wrapped ? SR_WRAPPED : 0);
        }
        if (wrapped && l == begin) /* no match */
            break;
    }
    return SR_NOTFOUND;
}

static void
clear_mark(struct Line* l)
{
    if (!l)
        return;
    int pos;
    for (pos = 0; pos < l->size; pos++)
        l->propBuf[pos] &= ~PE_MARK;
}

/* search by regular expression */
static int
srchcore(struct DefunContext ctx, const char* str, SearchFunc func)
{
    volatile int i, result = SR_NOTFOUND;

    if (str != NULL && str != getRuntime()->SearchString)
        getRuntime()->SearchString = str;
    if (getRuntime()->SearchString == NULL || *getRuntime()->SearchString == '\0')
        return SR_NOTFOUND;

    str = conv_search_string(getRuntime()->SearchString, getRuntime()->DisplayCharset, ctx.buf->doc->charset);
    auto prevtrap = mySignal(SIGINT, intTrap);
    tty_cbreak(true);
    static JMP_BUF IntReturn;
    if (SETJMP(IntReturn) == 0) {
        for (i = 0; i < PREC_NUM; i++) {
            result = func(ctx, str);
            if (i < PREC_NUM - 1 && result & SR_FOUND)
                clear_mark(ctx.buf->doc->currentLine);
        }
    }
    mySignal(SIGINT, prevtrap);
    tty_cbreak(false);
    return result;
}

static void
disp_srchresult(int result, const char* prompt, const char* str)
{
    if (str == NULL)
        str = "";
    if (result & SR_NOTFOUND)
        disp_message(Sprintf("Not found: %s", str)->ptr, TRUE);
    else if (result & SR_WRAPPED)
        disp_message(Sprintf("Search wrapped: %s", str)->ptr, TRUE);
    else if (getRuntime()->show_srch_str)
        disp_message(Sprintf("%s%s", prompt, str)->ptr, TRUE);
}

SearchFunc searchRoutine = NULL;

void srch(struct DefunContext ctx, SearchFunc func, const char* prompt)
{
    int disp = FALSE;

    const char* str = searchKeyData();
    if (str == NULL || *str == '\0') {
        str = inputStrHist(prompt, NULL, getRuntime()->TextHist);
        if (str != NULL && *str == '\0')
            str = getRuntime()->SearchString;
        if (str == NULL) {
            return;
        }
        disp = TRUE;
    }
    int pos = ctx.buf->doc->pos;
    if (func == forwardSearch)
        ctx.buf->doc->pos += 1;
    int result = srchcore(ctx, str, func);
    if (result & SR_FOUND)
        clear_mark(ctx.buf->doc->currentLine);
    else
        ctx.buf->doc->pos = pos;
    if (disp)
        disp_srchresult(result, prompt, str);
    searchRoutine = func;
}

static int
dispincsrch(struct DefunContext ctx, int ch, Str buf, Lineprop* prop)
{
    static struct Document sbuf;
    if (ch == 0 && buf == NULL) {
        COPY_BUFPOSITION(&sbuf, ctx.buf->doc); /* search starting point */
        return -1;
    }

    char* str = buf->ptr;
    bool do_next_search = false;
    switch (ch) {
    case 022: /* C-r */
        searchRoutine = backwardSearch;
        do_next_search = TRUE;
        break;
    case 023: /* C-s */
        searchRoutine = forwardSearch;
        do_next_search = TRUE;
        break;

    default:
        if (ch >= 0)
            return ch; /* use InputKeymap */
    }

    if (do_next_search) {
        if (*str) {
            if (searchRoutine == forwardSearch)
                ctx.buf->doc->pos += 1;
            COPY_BUFPOSITION(&sbuf, ctx.buf->doc);
            if (srchcore(ctx, str, searchRoutine) == SR_NOTFOUND
                && searchRoutine == forwardSearch) {
                ctx.buf->doc->pos -= 1;
                COPY_BUFPOSITION(&sbuf, ctx.buf->doc);
            }
            doc_arrangeCursor(ctx.buf->doc);
            clear_mark(ctx.buf->doc->currentLine);
            return -1;
        } else
            return 020; /* _prev completion for C-s C-s */
    } else if (*str) {
        COPY_BUFPOSITION(ctx.buf->doc, &sbuf);
        doc_arrangeCursor(ctx.buf->doc);
        srchcore(ctx, str, searchRoutine);
        doc_arrangeCursor(ctx.buf->doc);
    }
    clear_mark(ctx.buf->doc->currentLine);
    return -1;
}

void isrch(struct DefunContext ctx, SearchFunc func, const char* prompt)
{
    struct Document sbuf;
    COPY_BUFPOSITION(&sbuf, ctx.buf->doc);
    dispincsrch(ctx, 0, NULL, NULL); /* initialize incremental search state */

    searchRoutine = func;
    const char* str = inputLineHistSearch(prompt, NULL, IN_STRING, getRuntime()->TextHist, dispincsrch, ctx);
    if (str == NULL) {
        COPY_BUFPOSITION(ctx.buf->doc, &sbuf);
    }
}

void srch_nxtprv(struct DefunContext ctx, int reverse)
{
    int result;
    /* *INDENT-OFF* */
    static SearchFunc routine[2] = {
        forwardSearch, backwardSearch
    };
    /* *INDENT-ON* */

    if (searchRoutine == NULL) {
        /* FIXME: gettextize? */
        disp_message("No previous regular expression", TRUE);
        return;
    }
    if (reverse != 0)
        reverse = 1;
    if (searchRoutine == backwardSearch)
        reverse ^= 1;
    if (reverse == 0)
        ctx.buf->doc->pos += 1;
    result = srchcore(ctx, getRuntime()->SearchString, routine[reverse]);
    if (result & SR_FOUND)
        clear_mark(ctx.buf->doc->currentLine);
    else {
        if (reverse == 0)
            ctx.buf->doc->pos -= 1;
    }
    disp_srchresult(result, (reverse ? "Backward: " : "Forward: "),
        getRuntime()->SearchString);
}
