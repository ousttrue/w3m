#include "terms.h"
#include "alloc.h"
#include "Str.h"
#include "myctype.h"

#define M_SPACE (S_SCREENPROP | S_COLORED | S_BCOLORED | S_GRAPHICS)
#define M_CEOL (~(M_SPACE | C_WHICHCHAR))

struct Screen g_screen = {
    .cells = NULL,
    .lines = 0,
    .lines_capacity = 0,
    .cols = 0,
    .cols_capacity = 0,
    .tab_step = 8,
    .y = 0,
    .x = 0,
    .mode = 0,
};
struct Screen* screen_get()
{
    return &g_screen;
}

bool screen_need_redraw(char* c1, l_prop pr1, char* c2, l_prop pr2)
{
    if (!c1 || !c2 || strcmp(c1, c2))
        return 1;
    if (*c1 == ' ')
        return (pr1 ^ pr2) & M_SPACE & ~S_DIRTY;

    if ((pr1 ^ pr2) & ~S_DIRTY)
        return 1;

    return 0;
}

void screen_setup(int lines, int cols)
{
    if (lines + 1 > g_screen.lines_capacity) {
        g_screen.lines_capacity = lines + 1;
        g_screen.cols_capacity = 0;
        g_screen.cells = New_N(struct ScreenLine, g_screen.lines_capacity);
    }
    g_screen.lines = lines;

    if (cols + 1 > g_screen.cols_capacity) {
        g_screen.cols_capacity = cols + 1;
        for (int i = 0; i < g_screen.lines_capacity; i++) {
            g_screen.cells[i].lineimage = New_N(char*, g_screen.cols_capacity);
            memset(g_screen.cells[i].lineimage, 0, g_screen.cols_capacity * sizeof(char*));
            g_screen.cells[i].lineprop = New_N(l_prop, g_screen.cols_capacity);
        }
    }
    g_screen.cols = cols;

    {
        int i = 0;
        for (; i < lines; i++) {
            g_screen.cells[i].lineprop[0] = S_EOL;
            g_screen.cells[i].isdirty = 0;
        }
        for (; i < g_screen.lines_capacity; i++) {
            g_screen.cells[i].isdirty = L_UNUSED;
        }
    }

    screen_clear();
}

void screen_move(int line, int column)
{
    if (line >= 0 && line < g_screen.lines)
        g_screen.y = line;
    if (column >= 0 && column < g_screen.cols)
        g_screen.x = column;
}

void screen_addmch(const char* pc, size_t len, int width)
{
    static Str tmp = NULL;
    if (tmp == NULL)
        tmp = Strnew();
    Strcopy_charp_n(tmp, pc, len);
    pc = tmp->ptr;

    if (g_screen.x == g_screen.cols)
        screen_wrap();
    if (g_screen.x >= g_screen.cols)
        return;
    char** p = g_screen.cells[g_screen.y].lineimage;
    l_prop* pr = g_screen.cells[g_screen.y].lineprop;

    char c = *pc;
    if (pr[g_screen.x] & S_EOL) {
        if (c == ' ' && !(g_screen.mode & M_SPACE)) {
            g_screen.x++;
            return;
        }
        for (int i = g_screen.x; i >= 0 && (pr[i] & S_EOL); i--) {
            SETCH(p[i], SCREEN_SPACE, 1);
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
    // int width = wtf_width(pc);
    int i = g_screen.x + width - 1;
    if (i < g_screen.cols
        && (((pr[i] & S_BOLD) && screen_need_redraw(p[i], pr[i], pc, g_screen.mode))
            || ((pr[i] & S_UNDERLINE) && !(g_screen.mode & S_UNDERLINE)))) {
        screen_touch_line();
        i++;
        if (i < g_screen.cols) {
            screen_touch_column(i);
            if (pr[i] & S_EOL) {
                SETCH(p[i], SCREEN_SPACE, 1);
                SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
            } else {
                for (i++; i < g_screen.cols && CHMODE(pr[i]) == C_WCHAR2; i++)
                    screen_touch_column(i);
            }
        }
    }

    if (g_screen.x + width > g_screen.cols) {
        screen_touch_line();
        for (i = g_screen.x; i < g_screen.cols; i++) {
            SETCH(p[i], SCREEN_SPACE, 1);
            SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
            screen_touch_column(i);
        }
        screen_wrap();
        if (g_screen.x + width > g_screen.cols)
            return;
        p = g_screen.cells[g_screen.y].lineimage;
        pr = g_screen.cells[g_screen.y].lineprop;
    }
    if (CHMODE(pr[g_screen.x]) == C_WCHAR2) {
        screen_touch_line();
        for (i = g_screen.x - 1; i >= 0; i--) {
            l_prop l = CHMODE(pr[i]);
            SETCH(p[i], SCREEN_SPACE, 1);
            SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
            screen_touch_column(i);
            if (l != C_WCHAR2)
                break;
        }
    }
    if (CHMODE(g_screen.mode) != C_CTRL) {
        if (screen_need_redraw(p[g_screen.x], pr[g_screen.x], pc, g_screen.mode)) {
            SETCH(p[g_screen.x], pc, len);
            SETPROP(pr[g_screen.x], g_screen.mode);
            screen_touch_line();
            screen_touch_column(g_screen.x);
            SETCHMODE(g_screen.mode, C_WCHAR2);
            for (i = g_screen.x + 1; i < g_screen.x + width; i++) {
                SETCH(p[i], SCREEN_SPACE, 1);
                SETPROP(pr[i], (pr[g_screen.x] & ~C_WHICHCHAR) | C_WCHAR2);
                screen_touch_column(i);
            }
            for (; i < g_screen.cols && CHMODE(pr[i]) == C_WCHAR2; i++) {
                SETCH(p[i], SCREEN_SPACE, 1);
                SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
                screen_touch_column(i);
            }
        }
        g_screen.x += width;
    } else if (c == '\t') {
        int dest = (g_screen.x + g_screen.tab_step) / g_screen.tab_step * g_screen.tab_step;
        if (dest >= g_screen.cols) {
            screen_wrap();
            screen_touch_line();
            dest = g_screen.tab_step;
            p = g_screen.cells[g_screen.y].lineimage;
            pr = g_screen.cells[g_screen.y].lineprop;
        }
        for (i = g_screen.x; i < dest; i++) {
            if (screen_need_redraw(p[i], pr[i], SCREEN_SPACE, g_screen.mode)) {
                SETCH(p[i], SCREEN_SPACE, 1);
                SETPROP(pr[i], g_screen.mode);
                screen_touch_line();
                screen_touch_column(i);
            }
        }
        g_screen.x = i;
    } else if (c == '\n') {
        screen_wrap();
    } else if (c == '\r') { /* Carriage return */
        g_screen.x = 0;
    } else if (c == '\b' && g_screen.x > 0) { /* Backspace */
        g_screen.x--;
        while (g_screen.x > 0 && CHMODE(pr[g_screen.x]) == C_WCHAR2)
            g_screen.x--;
    }
}

void screen_add_tab()
{
    screen_addmch("\t", 1, g_screen.tab_step);
}

void screen_wrap(void)
{
    if (g_screen.y == g_screen.lines - 1)
        return;
    g_screen.y++;
    g_screen.x = 0;
}

void screen_touch_column(int col)
{
    if (col >= 0 && col < g_screen.cols)
        g_screen.cells[g_screen.y].lineprop[col] |= S_DIRTY;
}

void screen_touch_line(void)
{
    if (!(g_screen.cells[g_screen.y].isdirty & L_DIRTY)) {
        for (int i = 0; i < g_screen.cols; i++)
            g_screen.cells[g_screen.y].lineprop[i] &= ~S_DIRTY;
        g_screen.cells[g_screen.y].isdirty |= L_DIRTY;
    }
}

void screen_standout(void)
{
    g_screen.mode |= S_STANDOUT;
}

void screen_standend(void)
{
    g_screen.mode &= ~S_STANDOUT;
}

void screen_toggle_stand(void)
{
    l_prop* pr = g_screen.cells[g_screen.y].lineprop;
    pr[g_screen.x] ^= S_STANDOUT;
    if (CHMODE(pr[g_screen.x]) != C_WCHAR2) {
        for (int i = g_screen.x + 1; CHMODE(pr[i]) == C_WCHAR2; i++)
            pr[i] ^= S_STANDOUT;
    }
}

void screen_bold(void)
{
    g_screen.mode |= S_BOLD;
}

void screen_boldend(void)
{
    g_screen.mode &= ~S_BOLD;
}

void screen_underline(void)
{
    g_screen.mode |= S_UNDERLINE;
}

void screen_underlineend(void)
{
    g_screen.mode &= ~S_UNDERLINE;
}

void screen_graphstart(void)
{
    g_screen.mode |= S_GRAPHICS;
}

void screen_graphend(void)
{
    g_screen.mode &= ~S_GRAPHICS;
}

void screen_setfcolor(int color)
{
    g_screen.mode &= ~COL_FCOLOR;
    if ((color & 0xf) <= 7)
        g_screen.mode |= (((color & 7) | 8) << 8);
}

void screen_setbcolor(int color)
{
    g_screen.mode &= ~COL_BCOLOR;
    if ((color & 0xf) <= 7)
        g_screen.mode |= (((color & 7) | 8) << 12);
}

void screen_clear(void)
{
    screen_move(0, 0);
    for (int i = 0; i < g_screen.lines; i++) {
        g_screen.cells[i].isdirty = 0;
        l_prop* p = g_screen.cells[i].lineprop;
        for (int j = 0; j < g_screen.cols; j++) {
            p[j] = S_EOL;
        }
    }
    g_screen.mode = C_ASCII;
}

/* XXX: conflicts with curses's clrtoeol(3) ? */
/* Clear to the end of line */
void screen_clrtoeol(void)
{
    l_prop* lprop = g_screen.cells[g_screen.y].lineprop;

    if (lprop[g_screen.x] & S_EOL)
        return;

    if (!(g_screen.cells[g_screen.y].isdirty & (L_NEED_CE | L_CLRTOEOL)) || g_screen.cells[g_screen.y].eol > g_screen.x)
        g_screen.cells[g_screen.y].eol = g_screen.x;

    g_screen.cells[g_screen.y].isdirty |= L_CLRTOEOL;
    screen_touch_line();
    for (int i = g_screen.x; i < g_screen.cols && !(lprop[i] & S_EOL); i++) {
        lprop[i] = S_EOL | S_DIRTY;
    }
}

static void
screen_clrtoeol_with_bcolor(void)
{
    if (!(g_screen.mode & S_BCOLORED)) {
        screen_clrtoeol();
        return;
    }
    int cli = g_screen.y;
    int cco = g_screen.x;
    l_prop pr = g_screen.mode;
    g_screen.mode = (g_screen.mode & (M_CEOL | S_BCOLORED)) | C_ASCII;
    for (int i = g_screen.x; i < g_screen.cols; i++)
        screen_add_whitespace();
    screen_move(cli, cco);
    g_screen.mode = pr;
}

void screen_clrtoeolx(void)
{
    screen_clrtoeol_with_bcolor();
}

static void
screen_clrtobot_eol(void (*clrtoeol)())
{
    int l = g_screen.y;
    int c = g_screen.x;
    (*clrtoeol)();
    g_screen.x = 0;
    g_screen.y++;
    for (; g_screen.y < g_screen.lines; g_screen.y++)
        (*clrtoeol)();
    g_screen.y = l;
    g_screen.x = c;
}

void screen_clrtobotx(void)
{
    screen_clrtobot_eol(screen_clrtoeolx);
}

void screen_touch_cursor(void)
{
    int i;
    screen_touch_line();
    for (i = g_screen.x; i >= 0; i--) {
        screen_touch_column(i);
        if (CHMODE(g_screen.cells[g_screen.y].lineprop[i]) != C_WCHAR2)
            break;
    }
    for (i = g_screen.x + 1; i < g_screen.cols; i++) {
        if (CHMODE(g_screen.cells[g_screen.y].lineprop[i]) != C_WCHAR2)
            break;
        screen_touch_column(i);
    }
}
