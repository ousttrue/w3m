#include "line_input.h"
#include "LineInput.h"
#include "alloc.h"
#include "history.h"
#include "term_tty.h"
#include "global.h"
#include "ctrlcode.h"
#include "terms.h"
#include "indep.h"
#include "tab.h"
#include "display.h"

#include "wc_util.h"
#include <libwc/charset.h>

// clang-format off
InputFunc InputKeymap[32] = {
    /*  C-@     C-a     C-b     C-c     C-d     C-e     C-f     C-g     */
    _compl, _mvB, _mvL, _inbrk, delC, _mvE, _mvR, _inbrk,
    /*  C-h     C-i     C-j     C-k     C-l     C-m     C-n     C-o     */
    _bs, iself, _enter, killn, iself, _enter, _next, _editor,
    /*  C-p     C-q     C-r     C-s     C-t     C-u     C-v     C-w     */
    _prev, _quo, _bsw, iself, _mvLw, killb, _quo, _bsw,
    /*  C-x     C-y     C-z     C-[     C-\     C-]     C-^     C-_     */
    _tcompl, _mvRw, iself, _esc, iself, iself, iself, iself,
};
// clang-format on

#define CLEN (COLS - 2)

char* inputLineHistSearch(struct CmdArgs* args, const char* prompt, const char* def_str,
    enum InputLineFlags flag, enum HistoryType hist, IncrFunc incrfunc)
{
    struct LineInput g = LineInputInit(def_str, flag, hist);

    int opos = get_strwidth(WcOption, prompt);
    int epos = CLEN - opos;
    if (epos < 0)
        epos = 0;
    int lpos = epos / 3;
    int rpos = epos * 2 / 3;

    wc_char_conv_init(wc_guess_8bit_charset(DisplayCharset), InnerCharset);
    do {
        int x = calcPosition(g.strBuf->ptr, g.strProp, g.CLen, g.CPos, 0, CP_FORCE);
        if (x - rpos > g.offset) {
            int y = calcPosition(g.strBuf->ptr, g.strProp, g.CLen, g.CLen, 0, CP_AUTO);
            if (y - epos > x - rpos)
                g.offset = x - rpos;
            else if (y - epos > 0)
                g.offset = y - epos;
        } else if (x - lpos < g.offset) {
            if (x - lpos > 0)
                g.offset = x - lpos;
            else
                g.offset = 0;
        }
        move((LINES - 1), 0);
        addstr(prompt);
        if (g.is_passwd)
            addPasswd(g.strBuf->ptr, g.strProp, g.CLen, g.offset, COLS - opos);
        else
            addStr(g.strBuf->ptr, g.strProp, g.CLen, g.offset, COLS - opos);
        clrtoeolx();
        move((LINES - 1), opos + x - g.offset);
        refresh();

    next_char:
        args->ch = getch(args);
        g.cm_clear = true;
        g.cm_disp_clear = true;
        if (!g.i_quote && (((g.cm_mode & CPL_ALWAYS) && (args->ch == CTRL_I || (space_autocomplete && args->ch == ' '))) || ((g.cm_mode & CPL_ON) && (args->ch == CTRL_I)))) {
            if (emacs_like_lineedit && g.cm_next) {
                _dcompl(args, &g);
                g.need_redraw = true;
            } else {
                _compl(args, &g);
                g.cm_disp_next = -1;
            }
        } else if (!g.i_quote && g.CLen == g.CPos && (g.cm_mode & CPL_ALWAYS || g.cm_mode & CPL_ON) && args->ch == CTRL_D) {
            if (!emacs_like_lineedit) {
                _dcompl(args, &g);
                g.need_redraw = true;
            }
        } else if (!g.i_quote && args->ch == DEL_CODE) {
            _bs(args, &g);
            g.cm_next = false;
            g.cm_disp_next = -1;
        } else if (!g.i_quote && args->ch < 0x20) { /* Control code */
            if (incrfunc == NULL
                || (args->ch = incrfunc(args, g.strBuf->ptr, g.strProp)) < 0x20) {
                (*InputKeymap[args->ch])(args, &g);
            }
            if (incrfunc && args->ch != (unsigned char)-1 && args->ch != CTRL_J)
                incrfunc(args, g.strBuf->ptr, g.strProp);
            if (g.cm_clear)
                g.cm_next = false;
            if (g.cm_disp_clear)
                g.cm_disp_next = -1;
        } else {
            Str tmp = Strnew_wc_output(wc_char_conv(WcOption, args->ch));
            if (tmp == NULL) {
                g.i_quote = true;
                goto next_char;
            }
            g.i_quote = false;
            g.cm_next = false;
            g.cm_disp_next = -1;
            if (g.CLen + tmp->length > STR_LEN || !tmp->length)
                goto next_char;
            ins_char(&g, args, tmp);
            if (incrfunc)
                incrfunc(args, g.strBuf->ptr, g.strProp);
        }
        if (g.CLen && (flag & IN_CHAR))
            break;
    } while (g.i_cont);

    if (CurrentTab) {
        if (g.need_redraw)
            displayBuffer(args, B_FORCE_REDRAW);
    }

    if (g.i_broken)
        return NULL;

    move((LINES - 1), 0);
    refresh();
    char* p = g.strBuf->ptr;
    if (flag & (IN_FILENAME | IN_COMMAND)) {
        SKIP_BLANKS(p);
    }
    if (g.use_hist && !(flag & IN_URL) && *p != '\0') {
        const char* q = lastHist(hist);
        if (!q || strcmp(q, p))
            pushHist(hist, p);
    }
    if (flag & IN_FILENAME)
        return expandPath(p);
    else
        return allocStr(p, -1);
}

char* inputAnswer(struct CmdArgs* args, const char* prompt)
{
    if (QuietMessage)
        return "n";

    if (fmInitialized) {
        term_raw();
        return inputChar(args, prompt);
    } else {
        printf("%s", prompt);
        fflush(stdout);
        return Strfgets(stdin)->ptr;
    }
}
