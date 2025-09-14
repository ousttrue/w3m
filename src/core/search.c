#include "search.h"
#include "buffer_list.h"
#include "tty.h"
#include "ui.h"
#include "buffer.h"
#include "display.h"
#include "regex.h"
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

static const char* SearchString = NULL;
static SearchFunc searchRoutine;

static void
set_mark(struct Line* l, int pos, int epos)
{
    for (; pos < epos && pos < l->size; pos++)
        l->propBuf[pos] |= PE_MARK;
}

/* normalize search string */
const char* conv_search_string(struct UI ui, const char* str, wc_ces f_ces)
{
    if (SearchConv && !WcOption.pre_conv && ui.current_buffer->document.charset != f_ces)
        str = wtf_conv_fit(str, ui.current_buffer->document.charset);
    return str;
}

enum SearchResultFlags forwardSearch(struct UI ui, const char* str)
{

    const char* p;
    if ((p = regexCompile(str, IgnoreCase)) != NULL) {
        message(getUI(), MSG_INFO, p);
        return SR_NOTFOUND;
    }
    struct LineList* l = currentLine(&ui.current_buffer->document);
    if (l == NULL) {
        return SR_NOTFOUND;
    }
    int pos = ui.current_buffer->pos;
    if (l->bpos) {
        pos += l->bpos;
        while (l->bpos && l->prev)
            l = l->prev;
    }

    struct LineList* begin = l;
    while (pos < l->l.size && l->l.propBuf[pos] & PC_WCHAR2)
        pos++;
    if (pos < l->l.size && regexMatch(&l->l.lineBuf[pos], l->l.size - pos, 0) == 1) {
        const char *first, *last;
        matchedPosition(&first, &last);
        pos = first - l->l.lineBuf;
        while (pos >= l->l.len && l->next && l->next->bpos) {
            pos -= l->l.len;
            l = l->next;
        }
        ui.current_buffer->pos = pos;
        if (l != currentLine(&ui.current_buffer->document))
            gotoLine(&ui.current_buffer->document, l->linenumber);
        set_mark(&l->l, pos, pos + last - first);
        return SR_FOUND;
    }

    bool wrapped = false;
    for (l = l->next;; l = l->next) {
        if (l == NULL) {
            if (WrapSearch) {
                l = ui.current_buffer->document.firstLine;
                wrapped = true;
            } else {
                break;
            }
        }
        if (l->bpos)
            continue;
        if (regexMatch(l->l.lineBuf, l->l.size, 1) == 1) {
            const char *first, *last;
            matchedPosition(&first, &last);
            pos = first - l->l.lineBuf;
            while (pos >= l->l.len && l->next && l->next->bpos) {
                pos -= l->l.len;
                l = l->next;
            }
            ui.current_buffer->pos = pos;
            ui.current_buffer->document.currentLineIndex = l->linenumber;
            gotoLine(&ui.current_buffer->document, l->linenumber);
            set_mark(&l->l, pos, pos + last - first);
            return SR_FOUND | (wrapped ? SR_WRAPPED : 0);
        }
        if (wrapped && l == begin) /* no match */
            break;
    }
    return SR_NOTFOUND;
}

enum SearchResultFlags backwardSearch(struct UI ui, const char* str)
{
    const char* p;

    if ((p = regexCompile(str, IgnoreCase)) != NULL) {
        message(getUI(), MSG_INFO, p);
        return SR_NOTFOUND;
    }
    struct LineList* l = currentLine(&ui.current_buffer->document);
    if (l == NULL) {
        return SR_NOTFOUND;
    }
    int pos = ui.current_buffer->pos;
    if (l->bpos) {
        pos += l->bpos;
        while (l->bpos && l->prev)
            l = l->prev;
    }

    struct LineList* begin = l;
    if (pos > 0) {
        pos--;
        while (pos > 0 && l->l.propBuf[pos] & PC_WCHAR2)
            pos--;
        p = &l->l.lineBuf[pos];
        const char* found = NULL;
        const char* found_last = NULL;
        const char* q = l->l.lineBuf;
        while (regexMatch(q, &l->l.lineBuf[l->l.size] - q, q == l->l.lineBuf) == 1) {
            const char *first, *last;
            matchedPosition(&first, &last);
            if (first <= p) {
                found = first;
                found_last = last;
            }
            if (q - l->l.lineBuf >= l->l.size)
                break;
            q++;
            while (q - l->l.lineBuf < l->l.size
                && l->l.propBuf[q - l->l.lineBuf] & PC_WCHAR2)
                q++;
            if (q > p)
                break;
        }
        if (found) {
            pos = found - l->l.lineBuf;
            while (pos >= l->l.len && l->next && l->next->bpos) {
                pos -= l->l.len;
                l = l->next;
            }
            ui.current_buffer->pos = pos;
            if (l != currentLine(&ui.current_buffer->document))
                gotoLine(&ui.current_buffer->document, l->linenumber);
            set_mark(&l->l, pos, pos + found_last - found);
            return SR_FOUND;
        }
    }

    bool wrapped = false;
    for (l = l->prev;; l = l->prev) {
        if (l == NULL) {
            if (WrapSearch) {
                l = lastLine(&ui.current_buffer->document);
                wrapped = true;
            } else {
                break;
            }
        }
        const char* found = NULL;
        const char* found_last = NULL;
        const char* q = l->l.lineBuf;
        while (regexMatch(q, &l->l.lineBuf[l->l.size] - q, q == l->l.lineBuf) == 1) {
            const char *first, *last;
            matchedPosition(&first, &last);
            found = first;
            found_last = last;
            if (q - l->l.lineBuf >= l->l.size)
                break;
            q++;
            while (q - l->l.lineBuf < l->l.size
                && l->l.propBuf[q - l->l.lineBuf] & PC_WCHAR2)
                q++;
        }
        if (found) {
            pos = found - l->l.lineBuf;
            while (pos >= l->l.len && l->next && l->next->bpos) {
                pos -= l->l.len;
                l = l->next;
            }
            ui.current_buffer->pos = pos;
            gotoLine(&ui.current_buffer->document, l->linenumber);
            set_mark(&l->l, pos, pos + found_last - found);
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
    int pos;
    if (!l)
        return;
    for (pos = 0; pos < l->size; pos++)
        l->propBuf[pos] &= ~PE_MARK;
}

typedef void (*MySignalFunc)(int);
static MySignalFunc mySignal(int signal_number, MySignalFunc action)
{
#ifdef SA_RESTART
    struct sigaction new_action, old_action;

    sigemptyset(&new_action.sa_mask);
    new_action.sa_handler = action;
    if (signal_number == SIGALRM) {
#ifdef SA_INTERRUPT
        new_action.sa_flags = SA_INTERRUPT;
#else
        new_action.sa_flags = 0;
#endif
    } else {
        new_action.sa_flags = SA_RESTART;
    }
    sigaction(signal_number, &new_action, &old_action);
    return (old_action.sa_handler);
#else
    return (signal(signal_number, action));
#endif
}

/* search by regular expression */
static int srchcore(struct UI ui, const char* str, SearchFunc func)
{
    volatile int result = SR_NOTFOUND;

    if (str != NULL && str != SearchString)
        SearchString = str;
    if (SearchString == NULL || *SearchString == '\0')
        return SR_NOTFOUND;

    str = conv_search_string(ui, SearchString, DisplayCharset);
    MySignalFunc prevtrap = mySignal(SIGINT, intTrap);
    crmode();
    if (sigsetjmp(IntReturn, 1) == 0) {

        result = func(ui, str);
        if (result & SR_FOUND)
            clear_mark(&currentLine(&ui.current_buffer->document)->l);
    }
    mySignal(SIGINT, prevtrap);
    term_raw();
    return result;
}

static void
disp_srchresult(int result, const char* prompt, const char* str)
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
dispincsrch(struct UI ui, int ch, Str buf, Lineprop* prop)
{
    static struct Buffer sbuf;
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
                ui.current_buffer->pos += 1;
            SAVE_BUFPOSITION(&sbuf);
            if (srchcore(ui, str, searchRoutine) == SR_NOTFOUND
                && searchRoutine == forwardSearch) {
                ui.current_buffer->pos -= 1;
                SAVE_BUFPOSITION(&sbuf);
            }

            clear_mark(&currentLine(&ui.current_buffer->document)->l);
            return -1;
        } else
            return 020; /* _prev completion for C-s C-s */
    } else if (*str) {
        RESTORE_BUFPOSITION(&sbuf);
        srchcore(ui, str, searchRoutine);
    }

    clear_mark(&currentLine(&ui.current_buffer->document)->l);
    return -1;
}

void isrch(struct UI ui, SearchFunc func, char* prompt)
{
    struct Buffer sbuf;
    SAVE_BUFPOSITION(&sbuf);
    dispincsrch(ui, 0, NULL, NULL); /* initialize incremental search state */

    searchRoutine = func;
    const char* str = inputLineHistSearch(ui, prompt, NULL, IN_STRING, TextHist, dispincsrch);
    if (str == NULL) {
        RESTORE_BUFPOSITION(&sbuf);
    }
}

void srch(struct UI ui, SearchFunc func, char* prompt)
{
    int result;
    int disp = false;
    int pos;

    const char* str = searchKeyData();
    if (str == NULL || *str == '\0') {
        str = inputStrHist(getUI(), prompt, NULL, TextHist);
        if (str != NULL && *str == '\0')
            str = SearchString;
        if (str == NULL) {

            return;
        }
        disp = true;
    }
    pos = ui.current_buffer->pos;
    if (func == forwardSearch)
        ui.current_buffer->pos += 1;
    result = srchcore(ui, str, func);
    if (result & SR_FOUND)
        clear_mark(&currentLine(&ui.current_buffer->document)->l);
    else
        ui.current_buffer->pos = pos;

    if (disp)
        disp_srchresult(result, prompt, str);
    searchRoutine = func;
}

void srch_nxtprv(struct UI ui, bool reverse)
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
        ui.current_buffer->pos += 1;

    enum SearchResultFlags result = srchcore(ui, SearchString, routine[reverse]);
    if (result & SR_FOUND)
        clear_mark(&currentLine(&ui.current_buffer->document)->l);
    else {
        if (!reverse)
            ui.current_buffer->pos -= 1;
    }

    disp_srchresult(result, (reverse ? "Backward: " : "Forward: "), SearchString);
}
