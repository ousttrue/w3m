#include "search.h"
#include "term_tty.h"
#include "display.h"
#include "terms.h"
#include "signal_util.h"
#include "line_input.h"
#include "history.h"
#include "main.h"
#include "global.h"
#include "wc_util.h"
#include "buffer.h"
#include "display.h"
#include "regex.h"

#include <libwc/wtf.h>

#include <signal.h>
#include <errno.h>
#include <unistd.h>

SrchFunc searchRoutine = 0;

static void
set_mark(struct Line* l, int pos, int epos)
{
    for (; pos < epos && pos < l->size; pos++)
        l->propBuf[pos] |= PE_MARK;
}

/* normalize search string */
const char* conv_search_string(const char* str, wc_ces f_ces)
{
    if (SearchConv && !WcOption.pre_conv && Currentbuf->document_charset != f_ces)
        str = wtf_conv_fit(WcOption, str, Currentbuf->document_charset);
    return str;
}

int forwardSearch(struct Buffer* buf, const char* str)
{
    const char *p, *first, *last;
    struct Line *l, *begin;
    int wrapped = false;
    int pos;

    if ((p = regexCompile(str, IgnoreCase)) != NULL) {
        message(p, 0, 0);
        return SR_NOTFOUND;
    }
    l = buf->currentLine;
    if (l == NULL) {
        return SR_NOTFOUND;
    }
    pos = buf->pos;
    if (l->bpos) {
        pos += l->bpos;
        while (l->bpos && l->prev)
            l = l->prev;
    }
    begin = l;
    while (pos < l->size && l->propBuf[pos] & PC_WCHAR2)
        pos++;
    if (pos < l->size && regexMatch(&l->lineBuf[pos], l->size - pos, 0) == 1) {
        matchedPosition(&first, &last);
        pos = first - l->lineBuf;
        while (pos >= l->len && l->next && l->next->bpos) {
            pos -= l->len;
            l = l->next;
        }
        buf->pos = pos;
        if (l != buf->currentLine)
            gotoLine(buf, l->linenumber);
        arrangeCursor(buf);
        set_mark(l, pos, pos + last - first);
        return SR_FOUND;
    }
    for (l = l->next;; l = l->next) {
        if (l == NULL) {
            if (buf->pagerSource) {
                l = getNextPage(buf, 1);
                if (l == NULL) {
                    if (WrapSearch && !wrapped) {
                        l = buf->firstLine;
                        wrapped = true;
                    } else {
                        break;
                    }
                }
            } else if (WrapSearch) {
                l = buf->firstLine;
                wrapped = true;
            } else {
                break;
            }
        }
        if (l->bpos)
            continue;
        if (regexMatch(l->lineBuf, l->size, 1) == 1) {
            matchedPosition(&first, &last);
            pos = first - l->lineBuf;
            while (pos >= l->len && l->next && l->next->bpos) {
                pos -= l->len;
                l = l->next;
            }
            buf->pos = pos;
            buf->currentLine = l;
            gotoLine(buf, l->linenumber);
            arrangeCursor(buf);
            set_mark(l, pos, pos + last - first);
            return SR_FOUND | (wrapped ? SR_WRAPPED : 0);
        }
        if (wrapped && l == begin) /* no match */
            break;
    }
    return SR_NOTFOUND;
}

int backwardSearch(struct Buffer* buf, const char* str)
{
    const char *p, *q, *found, *found_last, *first, *last;
    struct Line *l, *begin;
    int wrapped = false;
    int pos;

    if ((p = regexCompile(str, IgnoreCase)) != NULL) {
        message(p, 0, 0);
        return SR_NOTFOUND;
    }
    l = buf->currentLine;
    if (l == NULL) {
        return SR_NOTFOUND;
    }
    pos = buf->pos;
    if (l->bpos) {
        pos += l->bpos;
        while (l->bpos && l->prev)
            l = l->prev;
    }
    begin = l;
    if (pos > 0) {
        pos--;
        while (pos > 0 && l->propBuf[pos] & PC_WCHAR2)
            pos--;
        p = &l->lineBuf[pos];
        found = NULL;
        found_last = NULL;
        q = l->lineBuf;
        while (regexMatch(q, &l->lineBuf[l->size] - q, q == l->lineBuf) == 1) {
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
            buf->pos = pos;
            if (l != buf->currentLine)
                gotoLine(buf, l->linenumber);
            arrangeCursor(buf);
            set_mark(l, pos, pos + found_last - found);
            return SR_FOUND;
        }
    }
    for (l = l->prev;; l = l->prev) {
        if (l == NULL) {
            if (WrapSearch) {
                l = buf->lastLine;
                wrapped = true;
            } else {
                break;
            }
        }
        found = NULL;
        found_last = NULL;
        q = l->lineBuf;
        while (regexMatch(q, &l->lineBuf[l->size] - q, q == l->lineBuf) == 1) {
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
            buf->pos = pos;
            gotoLine(buf, l->linenumber);
            arrangeCursor(buf);
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
    for (int pos = 0; pos < l->size; pos++)
        l->propBuf[pos] &= ~PE_MARK;
}

static void
disp_srchresult(struct CmdArgs* args, int result, const char* prompt, const char* str)
{
    if (str == NULL)
        str = "";
    if (result & SR_NOTFOUND)
        disp_message(args, Sprintf("Not found: %s", str)->ptr, true);
    else if (result & SR_WRAPPED)
        disp_message(args, Sprintf("Search wrapped: %s", str)->ptr, true);
    else if (show_srch_str)
        disp_message(args, Sprintf("%s%s", prompt, str)->ptr, true);
}

void srch(struct CmdArgs* args, SrchFunc func, const char* prompt)
{
    int result;
    int disp = false;
    int pos;

    const char* str = searchKeyData();
    if (str == NULL || *str == '\0') {
        str = inputStrHist(args, prompt, NULL, TextHist);
        if (str != NULL && *str == '\0')
            str = SearchString;
        if (str == NULL) {
            displayBuffer(args, B_NORMAL);
            return;
        }
        disp = true;
    }
    pos = Currentbuf->pos;
    if (func == forwardSearch)
        Currentbuf->pos += 1;
    result = srchcore(str, func);
    if (result & SR_FOUND)
        clear_mark(Currentbuf->currentLine);
    else
        Currentbuf->pos = pos;
    displayBuffer(args, B_NORMAL);
    if (disp)
        disp_srchresult(args, result, prompt, str);
    searchRoutine = func;
}

// search by regular expression
int srchcore(const char* str, SrchFunc func)
{
    volatile int i, result = SR_NOTFOUND;

    if (str != NULL && str != SearchString)
        SearchString = str;
    if (SearchString == NULL || *SearchString == '\0')
        return SR_NOTFOUND;

    str = conv_search_string(SearchString, DisplayCharset);
    auto prevtrap = signal(SIGINT, intTrap);
    crmode();
    if (SETJMP(IntReturn) == 0) {
        for (i = 0; i < PREC_NUM; i++) {
            result = func(Currentbuf, str);
            if (i < PREC_NUM - 1 && result & SR_FOUND)
                clear_mark(Currentbuf->currentLine);
        }
    }
    signal(SIGINT, prevtrap);
    term_raw();
    return result;
}

int dispincsrch(struct CmdArgs* args, Str buf, Lineprop* prop)
{
    static struct Buffer sbuf;
    bool do_next_search = false;

    if (args->ch == 0 && buf == NULL) {
        SAVE_BUFPOSITION(&sbuf); /* search starting point */
        return -1;
    }

    const char* str = buf->ptr;
    switch (args->ch) {
    case 022: /* C-r */
        searchRoutine = backwardSearch;
        do_next_search = true;
        break;
    case 023: /* C-s */
        searchRoutine = forwardSearch;
        do_next_search = true;
        break;

    default:
        if (args->ch >= 0)
            return args->ch; /* use InputKeymap */
    }

    if (do_next_search) {
        if (*str) {
            if (searchRoutine == forwardSearch)
                Currentbuf->pos += 1;
            SAVE_BUFPOSITION(&sbuf);
            if (srchcore(str, searchRoutine) == SR_NOTFOUND
                && searchRoutine == forwardSearch) {
                Currentbuf->pos -= 1;
                SAVE_BUFPOSITION(&sbuf);
            }
            arrangeCursor(Currentbuf);
            displayBuffer(args, B_FORCE_REDRAW);
            clear_mark(Currentbuf->currentLine);
            return -1;
        } else
            return 020; /* _prev completion for C-s C-s */
    } else if (*str) {
        RESTORE_BUFPOSITION(&sbuf);
        arrangeCursor(Currentbuf);
        srchcore(str, searchRoutine);
        arrangeCursor(Currentbuf);
    }
    displayBuffer(args, B_FORCE_REDRAW);
    clear_mark(Currentbuf->currentLine);
    return -1;
}

void isrch(struct CmdArgs* args, SrchFunc func, const char* prompt)
{
    struct Buffer sbuf;
    SAVE_BUFPOSITION(&sbuf);
    dispincsrch(args, NULL, NULL); /* initialize incremental search state */

    searchRoutine = func;
    const char* str = inputLineHistSearch(args, prompt, NULL, IN_STRING, TextHist, dispincsrch);
    if (str == NULL) {
        RESTORE_BUFPOSITION(&sbuf);
    }
    displayBuffer(args, B_FORCE_REDRAW);
}

void srch_nxtprv(struct CmdArgs *args, int reverse)
{
    static SrchFunc routine[2] = {
        forwardSearch, backwardSearch
    };

    if (searchRoutine == NULL) {
        /* FIXME: gettextize? */
        disp_message(args, "No previous regular expression", true);
        return;
    }
    if (reverse != 0)
        reverse = 1;
    if (searchRoutine == backwardSearch)
        reverse ^= 1;
    if (reverse == 0)
        Currentbuf->pos += 1;

    int result = srchcore(SearchString, routine[reverse]);
    if (result & SR_FOUND)
        clear_mark(Currentbuf->currentLine);
    else {
        if (reverse == 0)
            Currentbuf->pos -= 1;
    }
    displayBuffer(args, B_NORMAL);
    disp_srchresult(args, result, (reverse ? "Backward: " : "Forward: "),
        SearchString);
}
