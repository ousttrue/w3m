#include "screen.h"
#include "symbol.h"
#include "term_entry.h"
#include "w3m_runtime.h"
#include <gcstr/gcstr.h>
#include <stdlib.h>
#include <wc.h>
#include <wtf.h>
#include <string.h>

#define SPACE " "

int useColor = true;
int useActiveColor = false;
int basic_color = (8); /* don't change */
int anchor_color = (4); /* blue  */
int image_color = (2); /* green */
int form_color = (1); /* red   */
int bg_color = (8); /* don't change */
int mark_color = (6); /* cyan */
int active_color = (6); /* cyan */
int useVisitedColor = (false);
int visited_color = (5); /* magenta  */

static int tab_step = 8;
static int max_LINES = 0;
static int max_COLS = 0;
static struct ScreenLine* ScreenElem = 0;

static struct Screen g_screen = {
    .lines = 0,
    .cols = 0,
    .ScreenImage = 0,
    .CurLine = 0,
    .CurColumn = 0,
    .mode = 0,
};

struct Screen scr_get()
{
    return g_screen;
}

#define EFFECT_ANCHOR_START effect_anchor_start()
#define EFFECT_ANCHOR_END effect_anchor_end()
#define EFFECT_IMAGE_START effect_image_start()
#define EFFECT_IMAGE_END effect_image_end()
#define EFFECT_FORM_START effect_form_start()
#define EFFECT_FORM_END effect_form_end()
#define EFFECT_ACTIVE_START effect_active_start()
#define EFFECT_ACTIVE_END effect_active_end()
#define EFFECT_VISITED_START effect_visited_start()
#define EFFECT_VISITED_END effect_visited_end()
#define EFFECT_MARK_START effect_mark_start()
#define EFFECT_MARK_END effect_mark_end()

/*-
 * color:
 *     0  black
 *     1  red
 *     2  green
 *     3  yellow
 *     4  blue
 *     5  magenta
 *     6  cyan
 *     7  white
 */

#define EFFECT_ANCHOR_START_C scr_setfcolor(anchor_color)
#define EFFECT_IMAGE_START_C scr_setfcolor(image_color)
#define EFFECT_FORM_START_C scr_setfcolor(form_color)
#define EFFECT_ACTIVE_START_C (scr_setfcolor(active_color), scr_underline())
#define EFFECT_VISITED_START_C scr_setfcolor(visited_color)
#define EFFECT_MARK_START_C scr_setbcolor(mark_color)

#define EFFECT_IMAGE_END_C scr_setfcolor(basic_color)
#define EFFECT_ANCHOR_END_C scr_setfcolor(basic_color)
#define EFFECT_FORM_END_C scr_setfcolor(basic_color)
#define EFFECT_ACTIVE_END_C (scr_setfcolor(basic_color), scr_underlineend())
#define EFFECT_VISITED_END_C scr_setfcolor(basic_color)
#define EFFECT_MARK_END_C scr_setbcolor(bg_color)

#define EFFECT_ANCHOR_START_M scr_underline()
#define EFFECT_ANCHOR_END_M scr_underlineend()
#define EFFECT_IMAGE_START_M scr_standout()
#define EFFECT_IMAGE_END_M scr_standend()
#define EFFECT_FORM_START_M scr_standout()
#define EFFECT_FORM_END_M scr_standend()
#define EFFECT_ACTIVE_START_NC scr_underline()
#define EFFECT_ACTIVE_END_NC scr_underlineend()
#define EFFECT_ACTIVE_START_M scr_bold()
#define EFFECT_ACTIVE_END_M scr_boldend()
#define EFFECT_VISITED_START_M /**/
#define EFFECT_VISITED_END_M /**/
#define EFFECT_MARK_START_M scr_standout()
#define EFFECT_MARK_END_M scr_standend()
#define define_effect(name_start, name_end, color_start, color_end, mono_start, mono_end) \
    static void name_start                                                                \
    {                                                                                     \
        if (useColor) {                                                                   \
            color_start;                                                                  \
        } else {                                                                          \
            mono_start;                                                                   \
        }                                                                                 \
    }                                                                                     \
    static void name_end                                                                  \
    {                                                                                     \
        if (useColor) {                                                                   \
            color_end;                                                                    \
        } else {                                                                          \
            mono_end;                                                                     \
        }                                                                                 \
    }

define_effect(EFFECT_ANCHOR_START, EFFECT_ANCHOR_END, EFFECT_ANCHOR_START_C,
    EFFECT_ANCHOR_END_C, EFFECT_ANCHOR_START_M, EFFECT_ANCHOR_END_M)
    define_effect(EFFECT_IMAGE_START, EFFECT_IMAGE_END, EFFECT_IMAGE_START_C,
        EFFECT_IMAGE_END_C, EFFECT_IMAGE_START_M, EFFECT_IMAGE_END_M)
        define_effect(EFFECT_FORM_START, EFFECT_FORM_END, EFFECT_FORM_START_C,
            EFFECT_FORM_END_C, EFFECT_FORM_START_M, EFFECT_FORM_END_M)
            define_effect(EFFECT_MARK_START, EFFECT_MARK_END, EFFECT_MARK_START_C,
                EFFECT_MARK_END_C, EFFECT_MARK_START_M, EFFECT_MARK_END_M)

    /*****************/
    static void EFFECT_ACTIVE_START
{
    if (useColor) {
        if (useActiveColor) {
            {
                EFFECT_ACTIVE_START_C;
            }
        } else {
            EFFECT_ACTIVE_START_NC;
        }
    } else {
        EFFECT_ACTIVE_START_M;
    }
}

static void EFFECT_ACTIVE_END
{
    if (useColor) {
        if (useActiveColor) {
            EFFECT_ACTIVE_END_C;
        } else {
            EFFECT_ACTIVE_END_NC;
        }
    } else {
        EFFECT_ACTIVE_END_M;
    }
}

static void EFFECT_VISITED_START
{
    if (useVisitedColor) {
        if (useColor) {
            EFFECT_VISITED_START_C;
        } else {
            EFFECT_VISITED_START_M;
        }
    }
}

static void EFFECT_VISITED_END
{
    if (useVisitedColor) {
        if (useColor) {
            EFFECT_VISITED_END_C;
        } else {
            EFFECT_VISITED_END_M;
        }
    }
}

/*
 * Display some lines.
 */
static int ulmode = 0, somode = 0, bomode = 0;
static int anch_mode = 0, emph_mode = 0, imag_mode = 0, form_mode = 0,
           active_mode = 0, visited_mode = 0, mark_mode = 0, graph_mode = 0;
static Linecolor color_mode = 0;

void scr_init_color()
{
    if (useColor) {
        EFFECT_ANCHOR_END_C;
        scr_setbcolor(bg_color);
    }
}

void scr_active_start()
{
    EFFECT_ACTIVE_START;
}
void scr_active_end()
{
    EFFECT_ACTIVE_END;
}

void do_color(Linecolor c)
{
    if (c & 0x8)
        scr_setfcolor(c & 0x7);
    else if (color_mode & 0x8)
        scr_setfcolor(basic_color);
    if (c & 0x80)
        scr_setbcolor((c >> 4) & 0x7);
    else if (color_mode & 0x80)
        scr_setbcolor(bg_color);
    color_mode = c;
}

void scr_line_finalize()
{
    if (somode) {
        somode = false;
        scr_standend();
    }
    if (ulmode) {
        ulmode = false;
        scr_underlineend();
    }
    if (bomode) {
        bomode = false;
        scr_boldend();
    }
    if (emph_mode) {
        emph_mode = false;
        scr_boldend();
    }

    if (anch_mode) {
        anch_mode = false;
        EFFECT_ANCHOR_END;
    }
    if (imag_mode) {
        imag_mode = false;
        EFFECT_IMAGE_END;
    }
    if (form_mode) {
        form_mode = false;
        EFFECT_FORM_END;
    }
    if (visited_mode) {
        visited_mode = false;
        EFFECT_VISITED_END;
    }
    if (active_mode) {
        active_mode = false;
        EFFECT_ACTIVE_END;
    }
    if (mark_mode) {
        mark_mode = false;
        EFFECT_MARK_END;
    }
    if (graph_mode) {
        graph_mode = false;
        scr_graphend();
    }
    if (color_mode)
        do_color(0);
}

void scr_setup(int lines, int cols)
{
    if (lines + 1 > max_LINES) {
        max_LINES = lines + 1;
        max_COLS = 0;
        ScreenElem = New_N(struct ScreenLine, max_LINES);
        g_screen.ScreenImage = New_N(struct ScreenLine*, max_LINES);
    }
    g_screen.lines = lines;

    if (cols + 1 > max_COLS) {
        max_COLS = cols + 1;
        int i;
        for (i = 0; i < max_LINES; i++) {
            ScreenElem[i].lineimage = New_N(char*, max_COLS);
            memset(ScreenElem[i].lineimage, 0, max_COLS * sizeof(char*));
            ScreenElem[i].lineprop = New_N(uint16_t, max_COLS);
        }
    }
    g_screen.cols = cols;

    int i = 0;
    for (; i < g_screen.lines; i++) {
        g_screen.ScreenImage[i] = &ScreenElem[i];
        g_screen.ScreenImage[i]->lineprop[0] = S_EOL;
        g_screen.ScreenImage[i]->isdirty = 0;
    }
    for (; i < max_LINES; i++) {
        ScreenElem[i].isdirty = L_UNUSED;
    }

    scr_clear();
}

void scr_clear(void)
{
    // writestr(T_cl);
    scr_move(0, 0);
    for (int i = 0; i < g_screen.lines; i++) {
        g_screen.ScreenImage[i]->isdirty = 0;
        uint16_t* p = g_screen.ScreenImage[i]->lineprop;
        for (int j = 0; j < g_screen.cols; j++) {
            p[j] = S_EOL;
        }
    }
    g_screen.mode = C_ASCII;
}

void scr_move(int line, int column)
{
    if (line >= 0 && line < g_screen.lines)
        g_screen.CurLine = line;
    if (column >= 0 && column < g_screen.cols)
        g_screen.CurColumn = column;
}

void scr_wrap(void)
{
    if (g_screen.CurLine == g_screen.lines - 1)
        return;
    g_screen.CurLine++;
    g_screen.CurColumn = 0;
}

void scr_touch_cursor(void)
{
    int i;
    scr_touch_line();
    for (i = g_screen.CurColumn; i >= 0; i--) {
        scr_touch_column(i);
        if (CHMODE(g_screen.ScreenImage[g_screen.CurLine]->lineprop[i]) != C_WCHAR2)
            break;
    }
    for (i = g_screen.CurColumn + 1; i < g_screen.cols; i++) {
        if (CHMODE(g_screen.ScreenImage[g_screen.CurLine]->lineprop[i]) != C_WCHAR2)
            break;
        scr_touch_column(i);
    }
}

void scr_touch_column(int col)
{
    if (col >= 0 && col < g_screen.cols)
        g_screen.ScreenImage[g_screen.CurLine]->lineprop[col] |= S_DIRTY;
}

void scr_touch_line(void)
{
    if (!(g_screen.ScreenImage[g_screen.CurLine]->isdirty & L_DIRTY)) {
        int i;
        for (i = 0; i < g_screen.cols; i++)
            g_screen.ScreenImage[g_screen.CurLine]->lineprop[i] &= ~S_DIRTY;
        g_screen.ScreenImage[g_screen.CurLine]->isdirty |= L_DIRTY;
    }
}

/* XXX: conflicts with curses's clrtoeol(3) ? */
void scr_clrtoeol(void)
{ /* Clear to the end of line */
    int i;
    uint16_t* lprop = g_screen.ScreenImage[g_screen.CurLine]->lineprop;

    if (lprop[g_screen.CurColumn] & S_EOL)
        return;

    if (!(g_screen.ScreenImage[g_screen.CurLine]->isdirty & (L_NEED_CE | L_CLRTOEOL)) || g_screen.ScreenImage[g_screen.CurLine]->eol > g_screen.CurColumn)
        g_screen.ScreenImage[g_screen.CurLine]->eol = g_screen.CurColumn;

    g_screen.ScreenImage[g_screen.CurLine]->isdirty |= L_CLRTOEOL;
    scr_touch_line();
    for (i = g_screen.CurColumn; i < g_screen.cols && !(lprop[i] & S_EOL); i++) {
        lprop[i] = S_EOL | S_DIRTY;
    }
}

static void
clrtoeol_with_bcolor(void)
{
    int i, cli, cco;
    uint16_t pr;

    if (!(g_screen.mode & S_BCOLORED)) {
        scr_clrtoeol();
        return;
    }
    cli = g_screen.CurLine;
    cco = g_screen.CurColumn;
    pr = g_screen.mode;
    g_screen.mode = (g_screen.mode & (M_CEOL | S_BCOLORED)) | C_ASCII;
    for (i = g_screen.CurColumn; i < g_screen.cols; i++)
        scr_addch(' ');
    scr_move(cli, cco);
    g_screen.mode = pr;
}

void scr_clrtoeolx(void)
{
    clrtoeol_with_bcolor();
}

static void
clrtobot_eol(void (*clrtoeol)())
{
    int l, c;

    l = g_screen.CurLine;
    c = g_screen.CurColumn;
    (*clrtoeol)();
    g_screen.CurColumn = 0;
    g_screen.CurLine++;
    for (; g_screen.CurLine < g_screen.lines; g_screen.CurLine++)
        (*clrtoeol)();
    g_screen.CurLine = l;
    g_screen.CurColumn = c;
}

void scr_clrtobot(void)
{
    clrtobot_eol(scr_clrtoeol);
}

void scr_clrtobotx(void)
{
    clrtobot_eol(scr_clrtoeolx);
}

bool scr_is_need_redraw(const char* c1, uint16_t pr1, const char* c2, uint16_t pr2)
{
    if (!c1 || !c2 || strcmp(c1, c2))
        return 1;
    if (*c1 == ' ')
        return (pr1 ^ pr2) & M_SPACE & ~S_DIRTY;

    if ((pr1 ^ pr2) & ~S_DIRTY)
        return 1;

    return 0;
}

void scr_addmch(const char* pc, size_t len)
{
    uint16_t* pr;
    int dest, i;
    static Str tmp = NULL;
    char** p;
    char c = *pc;
    int width = wtf_width((wc_uchar*)pc);

    if (tmp == NULL)
        tmp = Strnew();
    Strcopy_charp_n(tmp, pc, len);
    pc = tmp->ptr;

    if (g_screen.CurColumn == g_screen.cols)
        scr_wrap();
    if (g_screen.CurColumn >= g_screen.cols)
        return;
    p = g_screen.ScreenImage[g_screen.CurLine]->lineimage;
    pr = g_screen.ScreenImage[g_screen.CurLine]->lineprop;

    if (pr[g_screen.CurColumn] & S_EOL) {
        if (c == ' ' && !(g_screen.mode & M_SPACE)) {
            g_screen.CurColumn++;
            return;
        }
        for (i = g_screen.CurColumn; i >= 0 && (pr[i] & S_EOL); i--) {
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
        }
    }

    if (c == '\t' || c == '\n' || c == '\r' || c == '\b')
        SETCHMODE(g_screen.mode, C_CTRL);
    else if (len > 1)
        SETCHMODE(g_screen.mode, C_WCHAR1);
    else if (!IS_CNTRL(c))
        SETCHMODE(g_screen.mode, C_ASCII);
    else
        return;

    /* Required to erase bold or underlined character for some * terminal
     * emulators. */
    i = g_screen.CurColumn + width - 1;
    if (i < g_screen.cols && (((pr[i] & S_BOLD) && scr_is_need_redraw(p[i], pr[i], pc, g_screen.mode)) || ((pr[i] & S_UNDERLINE) && !(g_screen.mode & S_UNDERLINE)))) {
        scr_touch_line();
        i++;
        if (i < g_screen.cols) {
            scr_touch_column(i);
            if (pr[i] & S_EOL) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
            } else {
                for (i++; i < g_screen.cols && CHMODE(pr[i]) == C_WCHAR2; i++)
                    scr_touch_column(i);
            }
        }
    }

    if (g_screen.CurColumn + width > g_screen.cols) {
        scr_touch_line();
        for (i = g_screen.CurColumn; i < g_screen.cols; i++) {
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
            scr_touch_column(i);
        }
        scr_wrap();
        if (g_screen.CurColumn + width > g_screen.cols)
            return;
        p = g_screen.ScreenImage[g_screen.CurLine]->lineimage;
        pr = g_screen.ScreenImage[g_screen.CurLine]->lineprop;
    }
    if (CHMODE(pr[g_screen.CurColumn]) == C_WCHAR2) {
        scr_touch_line();
        for (i = g_screen.CurColumn - 1; i >= 0; i--) {
            uint16_t l = CHMODE(pr[i]);
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
            scr_touch_column(i);
            if (l != C_WCHAR2)
                break;
        }
    }
    if (CHMODE(g_screen.mode) != C_CTRL) {
        if (scr_is_need_redraw(p[g_screen.CurColumn], pr[g_screen.CurColumn], pc, g_screen.mode)) {
            SETCH(p[g_screen.CurColumn], pc, len);
            SETPROP(pr[g_screen.CurColumn], g_screen.mode);
            scr_touch_line();
            scr_touch_column(g_screen.CurColumn);
            SETCHMODE(g_screen.mode, C_WCHAR2);
            for (i = g_screen.CurColumn + 1; i < g_screen.CurColumn + width; i++) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[g_screen.CurColumn] & ~C_WHICHCHAR) | C_WCHAR2);
                scr_touch_column(i);
            }
            for (; i < g_screen.cols && CHMODE(pr[i]) == C_WCHAR2; i++) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
                scr_touch_column(i);
            }
        }
        g_screen.CurColumn += width;
    } else if (c == '\t') {
        dest = (g_screen.CurColumn + tab_step) / tab_step * tab_step;
        if (dest >= g_screen.cols) {
            scr_wrap();
            scr_touch_line();
            dest = tab_step;
            p = g_screen.ScreenImage[g_screen.CurLine]->lineimage;
            pr = g_screen.ScreenImage[g_screen.CurLine]->lineprop;
        }
        for (i = g_screen.CurColumn; i < dest; i++) {
            if (scr_is_need_redraw(p[i], pr[i], SPACE, g_screen.mode)) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], g_screen.mode);
                scr_touch_line();
                scr_touch_column(i);
            }
        }
        g_screen.CurColumn = i;
    } else if (c == '\n') {
        scr_wrap();
    } else if (c == '\r') { /* Carriage return */
        g_screen.CurColumn = 0;
    } else if (c == '\b' && g_screen.CurColumn > 0) { /* Backspace */
        g_screen.CurColumn--;
        while (g_screen.CurColumn > 0 && CHMODE(pr[g_screen.CurColumn]) == C_WCHAR2)
            g_screen.CurColumn--;
    }
}

void scr_addstr(const char* s)
{
    while (*s != '\0') {
        int len = wtf_len((wc_uchar*)s);
        scr_addmch(s, len);
        s += len;
    }
}

void scr_addnstr(const char* s, int n)
{
    int i;
    for (i = 0; *s != '\0';) {
        int width = wtf_width((wc_uchar*)s);
        if (i + width > n)
            break;
        int len = wtf_len((wc_uchar*)s);
        scr_addmch(s, len);
        s += len;
        i += width;
    }
}

/// space padding
void scr_addnstr_sup(const char* s, int n)
{
    int i;
    for (i = 0; *s != '\0';) {
        int width = wtf_width((wc_uchar*)s);
        if (i + width > n)
            break;
        int len = wtf_len((wc_uchar*)s);
        scr_addmch(s, len);
        s += len;
        i += width;
    }
    for (; i < n; i++)
        scr_addch(' ');
}

#define do_effect1(effect, modeflag, action_start, action_end) \
    if (m & effect) {                                          \
        if (!modeflag) {                                       \
            action_start;                                      \
            modeflag = true;                                   \
        }                                                      \
    }

#define do_effect2(effect, modeflag, action_start, action_end) \
    if (modeflag) {                                            \
        action_end;                                            \
        modeflag = false;                                      \
    }

static void
do_effects(Lineprop m)
{
    /* effect end */
    do_effect2(PE_UNDER, ulmode, scr_underline(), scr_underlineend());
    do_effect2(PE_STAND, somode, scr_standout(), scr_standend());
    do_effect2(PE_BOLD, bomode, scr_bold(), scr_boldend());
    do_effect2(PE_EMPH, emph_mode, scr_bold(), scr_boldend());
    do_effect2(PE_ANCHOR, anch_mode, EFFECT_ANCHOR_START, EFFECT_ANCHOR_END);
    do_effect2(PE_IMAGE, imag_mode, EFFECT_IMAGE_START, EFFECT_IMAGE_END);
    do_effect2(PE_FORM, form_mode, EFFECT_FORM_START, EFFECT_FORM_END);
    do_effect2(PE_VISITED, visited_mode, EFFECT_VISITED_START,
        EFFECT_VISITED_END);
    do_effect2(PE_ACTIVE, active_mode, EFFECT_ACTIVE_START, EFFECT_ACTIVE_END);
    do_effect2(PE_MARK, mark_mode, EFFECT_MARK_START, EFFECT_MARK_END);
    if (graph_mode) {
        scr_graphend();
        graph_mode = false;
    }

    /* effect start */
    do_effect1(PE_UNDER, ulmode, scr_underline(), scr_underlineend());
    do_effect1(PE_STAND, somode, scr_standout(), scr_standend());
    do_effect1(PE_BOLD, bomode, scr_bold(), scr_boldend());
    do_effect1(PE_EMPH, emph_mode, scr_bold(), scr_boldend());
    do_effect1(PE_ANCHOR, anch_mode, EFFECT_ANCHOR_START, EFFECT_ANCHOR_END);
    do_effect1(PE_IMAGE, imag_mode, EFFECT_IMAGE_START, EFFECT_IMAGE_END);
    do_effect1(PE_FORM, form_mode, EFFECT_FORM_START, EFFECT_FORM_END);
    do_effect1(PE_VISITED, visited_mode, EFFECT_VISITED_START,
        EFFECT_VISITED_END);
    do_effect1(PE_ACTIVE, active_mode, EFFECT_ACTIVE_START, EFFECT_ACTIVE_END);
    do_effect1(PE_MARK, mark_mode, EFFECT_MARK_START, EFFECT_MARK_END);
}

void scr_addMChar(char* p, Lineprop mode, size_t len)
{
    Lineprop m = CharEffect(mode);
    char c = *p;

    if (mode & PC_WCHAR2)
        return;
    do_effects(m);
    if (mode & PC_SYMBOL) {
        char** symbol;
        int w = (mode & PC_KANJI) ? 2 : 1;

        c = ((char)wtf_get_code((wc_uchar*)p) & 0x7f) - SYMBOL_BASE;
        if (graph_ok() && c < N_GRAPH_SYMBOL) {
            if (!graph_mode) {
                scr_graphstart();
                graph_mode = true;
            }
            if (w == 2 && WcOption.use_wide)
                scr_addstr(graph2_symbol[(unsigned char)c % N_GRAPH_SYMBOL]);
            else
                scr_addch(*graph_symbol[(unsigned char)c % N_GRAPH_SYMBOL]);
        } else {
            symbol = get_symbol(DisplayCharset, &w);
            scr_addstr(symbol[(unsigned char)c % N_SYMBOL]);
        }
    } else if (mode & PC_CTRL) {
        switch (c) {
        case '\t':
            scr_addch(c);
            break;
        case '\n':
            scr_addch(' ');
            break;
        case '\r':
            break;
        case DEL_CODE:
            scr_addstr("^?");
            break;
        default:
            scr_addch('^');
            scr_addch(c + '@');
            break;
        }
    } else if (mode & PC_UNKNOWN) {
        char buf[5];
        sprintf(buf, "[%.2X]",
            (unsigned char)wtf_get_code((wc_uchar*)p) | 0x80);
        scr_addstr(buf);
    } else
        scr_addmch(p, len);
}

void scr_standout(void)
{
    g_screen.mode |= S_STANDOUT;
}

void scr_standend(void)
{
    g_screen.mode &= ~S_STANDOUT;
}

void scr_toggle_stand(void)
{
    int i;
    uint16_t* pr = g_screen.ScreenImage[g_screen.CurLine]->lineprop;
    pr[g_screen.CurColumn] ^= S_STANDOUT;
    if (CHMODE(pr[g_screen.CurColumn]) != C_WCHAR2) {
        for (i = g_screen.CurColumn + 1; CHMODE(pr[i]) == C_WCHAR2; i++)
            pr[i] ^= S_STANDOUT;
    }
}

void scr_bold(void)
{
    g_screen.mode |= S_BOLD;
}

void scr_boldend(void)
{
    g_screen.mode &= ~S_BOLD;
}

void scr_underline(void)
{
    g_screen.mode |= S_UNDERLINE;
}

void scr_underlineend(void)
{
    g_screen.mode &= ~S_UNDERLINE;
}

void scr_graphstart(void)
{
    g_screen.mode |= S_GRAPHICS;
}

void scr_graphend(void)
{
    g_screen.mode &= ~S_GRAPHICS;
}

void scr_setfcolor(int color)
{
    g_screen.mode &= ~COL_FCOLOR;
    if ((color & 0xf) <= 7)
        g_screen.mode |= (((color & 7) | 8) << 8);
}

void scr_setbcolor(int color)
{
    g_screen.mode &= ~COL_BCOLOR;
    if ((color & 0xf) <= 7)
        g_screen.mode |= (((color & 7) | 8) << 12);
}
