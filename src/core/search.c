#include "search.h"
#include "tty.h"
#include "ui.h"
#include "buffer.h"
#include "display.h"
#include "regex.h"
#include "mysignal.h"
#include "linein.h"
#include "keymap.h"
#include "history.h"
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <wtf.h>

char SearchConv = (true);
int IgnoreCase = (true);
int WrapSearch = (false);
int show_srch_str = (true);

#ifdef _WIN32
#else
static sigjmp_buf IntReturn;
static void intTrap(int _dummy)
{ /* Interrupt catcher */
    siglongjmp(IntReturn, 0);
}
#endif

static char* SearchString = NULL;
static SearchFunc searchRoutine;

static void
set_mark(Line* l, int pos, int epos)
{
    for (; pos < epos && pos < l->size; pos++)
        l->propBuf[pos] |= PE_MARK;
}

/* normalize search string */
char* conv_search_string(const char* str, wc_ces f_ces)
{
    if (SearchConv && !WcOption.pre_conv && Currentbuf->document_charset != f_ces)
        str = wtf_conv_fit(str, Currentbuf->document_charset);
    return str;
}

enum SearchResultFlags forwardSearch(Buffer* buf, char* str)
{
    char *p, *first, *last;
    Line *l, *begin;
    int wrapped = false;
    int pos;

    if ((p = regexCompile(str, IgnoreCase)) != NULL) {
        message(getUI(), MSG_INFO, p);
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
            if (WrapSearch) {
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

enum SearchResultFlags backwardSearch(Buffer* buf, char* str)
{
    char *p, *q, *found, *found_last, *first, *last;
    Line *l, *begin;
    int wrapped = false;
    int pos;

    if ((p = regexCompile(str, IgnoreCase)) != NULL) {
        message(getUI(), MSG_INFO, p);
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
clear_mark(Line* l)
{
    int pos;
    if (!l)
        return;
    for (pos = 0; pos < l->size; pos++)
        l->propBuf[pos] &= ~PE_MARK;
}

/* search by regular expression */
static int srchcore(char* str, SearchFunc func)
{
    volatile int result = SR_NOTFOUND;

    if (str != NULL && str != SearchString)
        SearchString = str;
    if (SearchString == NULL || *SearchString == '\0')
        return SR_NOTFOUND;

    str = conv_search_string(SearchString, DisplayCharset);
    MySignalFunc prevtrap = mySignal(SIGINT, intTrap);
    crmode();
    if (sigsetjmp(IntReturn, 1) == 0) {

        result = func(Currentbuf, str);
        if (result & SR_FOUND)
            clear_mark(Currentbuf->currentLine);
    }
    mySignal(SIGINT, prevtrap);
    term_raw();
    return result;
}

static void
disp_srchresult(int result, char* prompt, char* str)
{
    if (str == NULL)
        str = "";
    if (result & SR_NOTFOUND)
        message(getUI(), MSG_INFO, Sprintf("Not found: %s", str)->ptr);
    else if (result & SR_WRAPPED)
        message(getUI(), MSG_INFO, Sprintf("Search wrapped: %s", str)->ptr);
    else if (show_srch_str)
        message(getUI(), MSG_INFO, Sprintf("%s%s", prompt, str)->ptr);
}

static int
dispincsrch(int ch, Str buf, Lineprop* prop)
{
    static Buffer sbuf;
    char* str;
    int do_next_search = false;

    if (ch == 0 && buf == NULL) {
        SAVE_BUFPOSITION(&sbuf); /* search starting point */
        return -1;
    }

    str = buf->ptr;
    switch (ch) {
    case 022: /* C-r */
        searchRoutine = backwardSearch;
        do_next_search = true;
        break;
    case 023: /* C-s */
        searchRoutine = forwardSearch;
        do_next_search = true;
        break;

    default:
        if (ch >= 0)
            return ch; /* use InputKeymap */
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

    clear_mark(Currentbuf->currentLine);
    return -1;
}

void isrch(SearchFunc func, char* prompt)
{
    char* str;
    Buffer sbuf;
    SAVE_BUFPOSITION(&sbuf);
    dispincsrch(0, NULL, NULL); /* initialize incremental search state */

    searchRoutine = func;
    str = inputLineHistSearch(getUI(), prompt, NULL, IN_STRING, TextHist, dispincsrch);
    if (str == NULL) {
        RESTORE_BUFPOSITION(&sbuf);
    }
}

void srch(SearchFunc func, char* prompt)
{
    char* str;
    int result;
    int disp = false;
    int pos;

    str = searchKeyData();
    if (str == NULL || *str == '\0') {
        str = inputStrHist(getUI(), prompt, NULL, TextHist);
        if (str != NULL && *str == '\0')
            str = SearchString;
        if (str == NULL) {

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

    if (disp)
        disp_srchresult(result, prompt, str);
    searchRoutine = func;
}

void srch_nxtprv(bool reverse)
{
    static SearchFunc routine[2] = {
        forwardSearch, backwardSearch
    };

    if (searchRoutine == NULL) {
        message(getUI(), MSG_INFO, "No previous regular expression");
        return;
    }

    if (reverse)
        reverse = true;
    if (searchRoutine == backwardSearch)
        reverse = !reverse;
    if (!reverse)
        Currentbuf->pos += 1;

    enum SearchResultFlags result = srchcore(SearchString, routine[reverse]);
    if (result & SR_FOUND)
        clear_mark(Currentbuf->currentLine);
    else {
        if (!reverse)
            Currentbuf->pos -= 1;
    }

    disp_srchresult(result, (reverse ? "Backward: " : "Forward: "), SearchString);
}
