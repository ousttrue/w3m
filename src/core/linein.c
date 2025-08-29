#include "linein.h"
#include "history.h"
#include "screen.h"
#include "term_renderer.h"
#include "ctrlcode.h"
#include "display.h"
#include "tty.h"
#include "fm.h"
#include "local.h"
#include "event_poller.h"
#include "screen.h"
#include "putc.h"
#include "LineEditor.h"
#include <stdbool.h>
#include <wtf.h>

typedef void (*LineEditorFunc)(struct LineEditor* e);

LineEditorFunc InputKeymap[32] = {
    /*  C-@     C-a     C-b     C-c     C-d     C-e     C-f     C-g     */
    _compl,
    _mvB,
    _mvL,
    _inbrk,
    delC,
    _mvE,
    _mvR,
    _inbrk,
    /*  C-h     C-i     C-j     C-k     C-l     C-m     C-n     C-o     */
    _bs,
    iself,
    _enter,
    killn,
    iself,
    _enter,
    _next,
    _editor,
    /*  C-p     C-q     C-r     C-s     C-t     C-u     C-v     C-w     */
    _prev,
    _quo,
    _bsw,
    iself,
    _mvLw,
    killb,
    _quo,
    _bsw,
    /*  C-x     C-y     C-z     C-[     C-\     C-]     C-^     C-_     */
    _tcompl,
    _mvRw,
    iself,
    iself,
    iself,
    iself,
    iself,
    iself,
};

static struct LineEditor g_editor;

char* inputLineHistSearch(struct UI ui,
    const char* prompt, const char* def_str, enum InputLineFlags flag, struct Hist* hist, IncFunc incrfunc)
{
    le_initialize(&g_editor, hist, flag, def_str);

    int opos = get_strwidth(prompt);
    int epos = ui.rows - 2 - opos;
    if (epos < 0)
        epos = 0;
    int lpos = epos / 3;
    int rpos = epos * 2 / 3;

    unsigned char c;
    wc_char_conv_init(wc_guess_8bit_charset(DisplayCharset), InnerCharset);
    GetChFunc getch = event_begin_input(-1);

    while (g_editor.i_cont) {
        // update offset
        int x = calcPosition(g_editor.strBuf->ptr, g_editor.strProp, g_editor.CLen, g_editor.CPos, 0, CP_FORCE);
        if (x - rpos > g_editor.offset) {
            int y = calcPosition(g_editor.strBuf->ptr, g_editor.strProp, g_editor.CLen, g_editor.CLen, 0, CP_AUTO);
            if (y - epos > x - rpos)
                g_editor.offset = x - rpos;
            else if (y - epos > 0)
                g_editor.offset = y - epos;
        } else if (x - lpos < g_editor.offset) {
            if (x - lpos > 0)
                g_editor.offset = x - lpos;
            else
                g_editor.offset = 0;
        }

        // show prompt
        move(ui.vt, ui.rows - 1, 0);
        addstr(ui.vt, prompt);

        // show current
        if (g_editor.is_passwd)
            addPasswd(&g_editor,
                g_editor.strBuf->ptr, g_editor.strProp, g_editor.CLen, g_editor.offset, ui.cols - opos);
        else
            addStr(&g_editor,
                g_editor.strBuf->ptr, g_editor.strProp, g_editor.CLen, g_editor.offset, ui.cols - opos);

        // cursor
        clrtoeolx(ui.vt);
        move(ui.vt, ui.rows - 1, opos + x - g_editor.offset);

        // draw frame
        struct Frame* frame = screenToFrame(getScreen());
        wc_putc_init(InnerCharset, DisplayCharset);
        refreshFrame(ttyWriter(), frame);
        wc_putc_end(ttyWriter());

        MOVE(ttyWriter(), getScreen()->CurLine, getScreen()->CurColumn);
        flushWriter(ttyWriter());

    next_char:
        c = getch();
        g_editor.cm_clear = TRUE;
        g_editor.cm_disp_clear = TRUE;
        if (!g_editor.i_quote
            && ((
                    (g_editor.cm_mode & CPL_ALWAYS)
                    && (c == CTRL_I || (space_autocomplete && c == ' ')))
                || ((g_editor.cm_mode & CPL_ON) && (c == CTRL_I)))) {
            if (emacs_like_lineedit && g_editor.cm_next) {
                _dcompl(&g_editor);
                g_editor.need_redraw = TRUE;
            } else {
                _compl(&g_editor);
                g_editor.cm_disp_next = -1;
            }
        } else if (!g_editor.i_quote && g_editor.CLen == g_editor.CPos && (g_editor.cm_mode & CPL_ALWAYS || g_editor.cm_mode & CPL_ON) && c == CTRL_D) {
            if (!emacs_like_lineedit) {
                _dcompl(&g_editor);
                g_editor.need_redraw = TRUE;
            }
        } else if (!g_editor.i_quote && c == DEL_CODE) {
            _bs(&g_editor);
            g_editor.cm_next = FALSE;
            g_editor.cm_disp_next = -1;
        } else if (!g_editor.i_quote && c < 0x20) { /* Control code */
            if (incrfunc == NULL
                || (c = incrfunc((int)c, g_editor.strBuf, g_editor.strProp)) < 0x20)
                InputKeymap[(int)c](&g_editor);
            if (incrfunc && c != (unsigned char)-1 && c != CTRL_J)
                incrfunc(-1, g_editor.strBuf, g_editor.strProp);
            if (g_editor.cm_clear)
                g_editor.cm_next = FALSE;
            if (g_editor.cm_disp_clear)
                g_editor.cm_disp_next = -1;
        } else {
            Str tmp = wc_char_conv(c);
            if (tmp == NULL) {
                g_editor.i_quote = TRUE;
                goto next_char;
            }
            g_editor.i_quote = FALSE;
            g_editor.cm_next = FALSE;
            g_editor.cm_disp_next = -1;
            if (g_editor.CLen + tmp->length > STR_LEN || !tmp->length)
                goto next_char;
            ins_char(&g_editor, tmp);
            if (incrfunc)
                incrfunc(-1, g_editor.strBuf, g_editor.strProp);
        }
        if (g_editor.CLen && (flag & IN_CHAR))
            break;
    }
    event_end_input(getch);

    if (g_editor.i_broken)
        return NULL;

    move(getScreen(), ui.rows - 1, 0);
    struct Frame* frame = screenToFrame(getScreen());

    wc_putc_init(InnerCharset, DisplayCharset);
    refreshFrame(ttyWriter(), frame);
    wc_putc_end(ttyWriter());

    MOVE(ttyWriter(), getScreen()->CurLine, getScreen()->CurColumn);
    flushWriter(ttyWriter());

    char* p = g_editor.strBuf->ptr;
    if (flag & (IN_FILENAME | IN_COMMAND)) {
        SKIP_BLANKS(p);
    }
    if (g_editor.use_hist && !(flag & IN_URL) && *p != '\0') {
        char* q = lastHist(hist);
        if (!q || strcmp(q, p))
            pushHist(hist, p);
    }
    if (flag & IN_FILENAME)
        return expandPath(p);
    else
        return allocStr(p, -1);
}
