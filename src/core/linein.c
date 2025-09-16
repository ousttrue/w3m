#include "linein.h"
#include "w3m.h"
#include "runtime.h"
#include "history.h"
#include "screen.h"
#include "ctrlcode.h"
#include "display.h"
#include "screen.h"
#include "LineEditor.h"
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <wtf.h>

int space_autocomplete = false;
int emacs_like_lineedit = false;

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
    _nop,
    _enter,
    killn,
    _nop,
    _enter,
    _next,
    _editor,
    /*  C-p     C-q     C-r     C-s     C-t     C-u     C-v     C-w     */
    _prev,
    _quo,
    _bsw,
    _nop,
    _mvLw,
    killb,
    _quo,
    _bsw,
    /*  C-x     C-y     C-z     C-[     C-\     C-]     C-^     C-_     */
    _tcompl,
    _mvRw,
    _nop,
    _nop,
    _nop,
    _nop,
    _nop,
    _nop,
};

static struct LineEditor g_editor;

const char* inputLineHistSearch(struct UI ui,
    const char* prompt, const char* def_str, enum InputLineFlags flag, struct Hist* hist, IncFunc incrfunc)
{
    le_initialize(&g_editor, ui, hist, flag, def_str);

    int opos = get_strwidth(prompt);
    int epos = ui.vt->ROWS - 2 - opos;
    if (epos < 0)
        epos = 0;
    int lpos = epos / 3;
    int rpos = epos * 2 / 3;

    unsigned char c;
    wc_char_conv_init(wc_guess_8bit_charset(DisplayCharset), InnerCharset);

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
        vt_move(ui.vt, ui.vt->ROWS - 1, 0);
        vt_addstr(ui.vt, prompt);

        // show current
        if (g_editor.is_passwd)
            le_addPasswd(&g_editor,
                g_editor.strBuf->ptr, g_editor.strProp, g_editor.CLen, g_editor.offset, ui.vt->COLS - opos);
        else
            le_addStr(&g_editor,
                g_editor.strBuf->ptr, g_editor.strProp, g_editor.CLen, g_editor.offset, ui.vt->COLS - opos);

        // cursor
        vt_clrtoeolx(ui.vt);
        vt_move(ui.vt, ui.vt->ROWS - 1, opos + x - g_editor.offset);

        // draw frame
        renderFrame(ui);

    next_char:
        c = ui.vtable.getCh(ui.co);
        g_editor.cm_clear = true;
        g_editor.cm_disp_clear = true;
        if (!g_editor.i_quote
            && ((
                    (g_editor.cm_mode & CPL_ALWAYS)
                    && (c == CTRL_I || (space_autocomplete && c == ' ')))
                || ((g_editor.cm_mode & CPL_ON) && (c == CTRL_I)))) {
            if (emacs_like_lineedit && g_editor.cm_next) {
                _dcompl(&g_editor);
                g_editor.need_redraw = true;
            } else {
                _compl(&g_editor);
                g_editor.cm_disp_next = -1;
            }
        } else if (!g_editor.i_quote && g_editor.CLen == g_editor.CPos && (g_editor.cm_mode & CPL_ALWAYS || g_editor.cm_mode & CPL_ON) && c == CTRL_D) {
            if (!emacs_like_lineedit) {
                _dcompl(&g_editor);
                g_editor.need_redraw = true;
            }
        } else if (!g_editor.i_quote && c == DEL_CODE) {
            _bs(&g_editor);
            g_editor.cm_next = false;
            g_editor.cm_disp_next = -1;
        } else if (!g_editor.i_quote && c < 0x20) { /* Control code */
            if (incrfunc == NULL
                || (c = incrfunc(ui, (int)c, g_editor.strBuf, g_editor.strProp)) < 0x20)
                InputKeymap[(int)c](&g_editor);
            if (incrfunc && c != (unsigned char)-1 && c != CTRL_J)
                incrfunc(ui, -1, g_editor.strBuf, g_editor.strProp);
            if (g_editor.cm_clear)
                g_editor.cm_next = false;
            if (g_editor.cm_disp_clear)
                g_editor.cm_disp_next = -1;
        } else {
            Str tmp = wc_char_conv(c);
            if (tmp == NULL) {
                g_editor.i_quote = true;
                goto next_char;
            }
            g_editor.i_quote = false;
            g_editor.cm_next = false;
            g_editor.cm_disp_next = -1;
            if (g_editor.CLen + tmp->length > STR_LEN || !tmp->length)
                goto next_char;
            le_ins_char(&g_editor, tmp);
            if (incrfunc)
                incrfunc(ui, -1, g_editor.strBuf, g_editor.strProp);
        }
        if (g_editor.CLen && (flag & IN_CHAR))
            break;
    }

    if (g_editor.i_broken)
        return NULL;

    vt_move(getScreen(), ui.vt->ROWS - 1, 0);
    renderFrame(ui);

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

const char* inputAnswer(struct UI ui, const char* prompt)
{
    if (QuietMessage)
        return "n";

    const char* ans;
    // if (fmInitialized)
    {
        // term_raw();
        ans = inputChar(ui, prompt);
    }
    // else {
    //     printf("%s", prompt);
    //     fflush(stdout);
    //     ans = Strfgets(stdin)->ptr;
    // }
    return ans;
}

bool notExistsOrOverWrite(struct UI ui, const char* path)
{
    struct stat st;
    if (stat(path, &st) < 0) {
        // not exists
        return true;
    }

    const char* ans = inputAnswer(ui, "File exists. Overwrite? (y/n)");
    if (ans && TOLOWER(*ans) == 'y') {
        // can overwrite
        return true;
    }

    return false;
}
