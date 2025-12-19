#include "terms.h"
#include "alloc.h"
#include "Str.h"
#include "myctype.h"

#define M_SPACE (S_SCREENPROP | S_COLORED | S_BCOLORED | S_GRAPHICS)
#define M_CEOL (~(M_SPACE | C_WHICHCHAR))

void SET_CHAR(struct ScreenCell* p, const char* ch, size_t len)
{
    strncpy(p->str, ch, len + 1);
}

enum ScreenCellProperty CHAR_MODE(enum ScreenCellProperty c) { return ((c)&C_WHICHCHAR); }

void SET_CHAR_MODE(enum ScreenCellProperty* var, enum ScreenCellProperty mode)
{
    *var = (*var & ~C_WHICHCHAR) | mode;
}
void SET_PROP(struct ScreenCell* p, enum ScreenCellProperty prop)
{
    p->prop = (p->prop & S_DIRTY) | prop;
}

bool screen_need_redraw(char* c1, enum ScreenCellProperty pr1, char* c2, enum ScreenCellProperty pr2)
{
    if (!c1 || !c2 || strcmp(c1, c2))
        return 1;
    if (*c1 == ' ')
        return (pr1 ^ pr2) & M_SPACE & ~S_DIRTY;

    if ((pr1 ^ pr2) & ~S_DIRTY)
        return 1;

    return 0;
}

void screen_setup(int line_count, int col_count)
{
    if (line_count + 1 > screen_get()->line_capacity) {
        screen_get()->line_capacity = line_count + 1;
        screen_get()->col_capacity = 0;
        screen_get()->lines = New_N(struct ScreenLine, screen_get()->line_capacity);
    }
    screen_get()->line_count = line_count;

    if (col_count + 1 > screen_get()->col_capacity) {
        screen_get()->col_capacity = col_count + 1;
        for (int i = 0; i < screen_get()->line_capacity; i++) {
            screen_get()->lines[i].cells = New_N(struct ScreenCell, screen_get()->col_capacity);
            memset(screen_get()->lines[i].cells, 0, screen_get()->col_capacity * sizeof(struct ScreenCell));
        }
    }
    screen_get()->col_count = col_count;

    {
        int i = 0;
        for (; i < line_count; i++) {
            screen_get()->lines[i].cells[0].prop = S_EOL;
            screen_get()->lines[i].isdirty = 0;
        }
        for (; i < screen_get()->line_capacity; i++) {
            screen_get()->lines[i].isdirty = L_UNUSED;
        }
    }

    screen_clear();
}

void screen_move(int line, int column)
{
    if (line >= 0 && line < screen_get()->line_count)
        screen_get()->y = line;
    if (column >= 0 && column < screen_get()->col_count)
        screen_get()->x = column;
}

void screen_addmch(const char* pc, size_t len, int width)
{
    static Str tmp = NULL;
    if (tmp == NULL)
        tmp = Strnew();
    Strcopy_charp_n(tmp, pc, len);
    pc = tmp->ptr;

    if (screen_get()->x == screen_get()->col_count)
        screen_wrap();
    if (screen_get()->x >= screen_get()->col_count)
        return;
    struct ScreenCell* p = screen_get()->lines[screen_get()->y].cells;

    char c = *pc;
    if (p[screen_get()->x].prop & S_EOL) {
        if (c == ' ' && !(screen_get()->mode & M_SPACE)) {
            screen_get()->x++;
            return;
        }
        for (int i = screen_get()->x; i >= 0 && (p[i].prop & S_EOL); i--) {
            SET_CHAR(&p[i], SCREEN_SPACE, 1);
            SET_PROP(&p[i], (p[i].prop & M_CEOL) | C_ASCII);
        }
    }

    if (c == '\t' || c == '\n' || c == '\r' || c == '\b')
        SET_CHAR_MODE(&screen_get()->mode, C_CTRL);
    else if (len > 1)
        SET_CHAR_MODE(&screen_get()->mode, C_WCHAR1);
    else if (!IS_CNTRL(c))
        SET_CHAR_MODE(&screen_get()->mode, C_ASCII);
    else
        return;

    // Required to erase bold or underlined character for some * terminal
    // emulators.
    // int width = wtf_width(pc);
    int i = screen_get()->x + width - 1;
    if (i < screen_get()->col_count
        && (((p[i].prop & S_BOLD) && screen_need_redraw(p[i].str, p[i].prop, (char*)pc, screen_get()->mode))
            || ((p[i].prop & S_UNDERLINE) && !(screen_get()->mode & S_UNDERLINE)))) {
        screen_touch_line();
        i++;
        if (i < screen_get()->col_count) {
            screen_touch_column(i);
            if (p[i].prop & S_EOL) {
                SET_CHAR(&p[i], SCREEN_SPACE, 1);
                SET_PROP(&p[i], (p[i].prop & M_CEOL) | C_ASCII);
            } else {
                for (i++; i < screen_get()->col_count && CHAR_MODE(p[i].prop) == C_WCHAR2; i++)
                    screen_touch_column(i);
            }
        }
    }

    if (screen_get()->x + width > screen_get()->col_count) {
        screen_touch_line();
        for (i = screen_get()->x; i < screen_get()->col_count; i++) {
            SET_CHAR(&p[i], SCREEN_SPACE, 1);
            SET_PROP(&p[i], (p[i].prop & ~C_WHICHCHAR) | C_ASCII);
            screen_touch_column(i);
        }
        screen_wrap();
        if (screen_get()->x + width > screen_get()->col_count)
            return;
        p = screen_get()->lines[screen_get()->y].cells;
    }
    if (CHAR_MODE(p[screen_get()->x].prop) == C_WCHAR2) {
        screen_touch_line();
        for (i = screen_get()->x - 1; i >= 0; i--) {
            enum ScreenCellProperty l = CHAR_MODE(p[i].prop);
            SET_CHAR(&p[i], SCREEN_SPACE, 1);
            SET_PROP(&p[i], (p[i].prop & ~C_WHICHCHAR) | C_ASCII);
            screen_touch_column(i);
            if (l != C_WCHAR2)
                break;
        }
    }
    if (CHAR_MODE(screen_get()->mode) != C_CTRL) {
        if (screen_need_redraw(p[screen_get()->x].str, p[screen_get()->x].prop, (char*)pc, screen_get()->mode)) {
            SET_CHAR(&p[screen_get()->x], pc, len);
            SET_PROP(&p[screen_get()->x], screen_get()->mode);
            screen_touch_line();
            screen_touch_column(screen_get()->x);
            SET_CHAR_MODE(&screen_get()->mode, C_WCHAR2);
            for (i = screen_get()->x + 1; i < screen_get()->x + width; i++) {
                SET_CHAR(&p[i], SCREEN_SPACE, 1);
                SET_PROP(&p[i], (p[screen_get()->x].prop & ~C_WHICHCHAR) | C_WCHAR2);
                screen_touch_column(i);
            }
            for (; i < screen_get()->col_count && CHAR_MODE(p[i].prop) == C_WCHAR2; i++) {
                SET_CHAR(&p[i], SCREEN_SPACE, 1);
                SET_PROP(&p[i], (p[i].prop & ~C_WHICHCHAR) | C_ASCII);
                screen_touch_column(i);
            }
        }
        screen_get()->x += width;
    } else if (c == '\t') {
        int dest = (screen_get()->x + screen_get()->tab_step) / screen_get()->tab_step * screen_get()->tab_step;
        if (dest >= screen_get()->col_count) {
            screen_wrap();
            screen_touch_line();
            dest = screen_get()->tab_step;
            p = screen_get()->lines[screen_get()->y].cells;
        }
        for (i = screen_get()->x; i < dest; i++) {
            if (screen_need_redraw(p[i].str, p[i].prop, SCREEN_SPACE, screen_get()->mode)) {
                SET_CHAR(&p[i], SCREEN_SPACE, 1);
                SET_PROP(&p[i], screen_get()->mode);
                screen_touch_line();
                screen_touch_column(i);
            }
        }
        screen_get()->x = i;
    } else if (c == '\n') {
        screen_wrap();
    } else if (c == '\r') { // Carriage return
        screen_get()->x = 0;
    } else if (c == '\b' && screen_get()->x > 0) { // Backspace
        screen_get()->x--;
        while (screen_get()->x > 0 && CHAR_MODE(p[screen_get()->x].prop) == C_WCHAR2)
            screen_get()->x--;
    }
}

void screen_add_tab()
{
    screen_addmch("\t", 1, screen_get()->tab_step);
}

void screen_wrap(void)
{
    if (screen_get()->y == screen_get()->line_count - 1)
        return;
    screen_get()->y++;
    screen_get()->x = 0;
}

void screen_touch_column(int col)
{
    if (col >= 0 && col < screen_get()->col_count)
        screen_get()->lines[screen_get()->y].cells[col].prop |= S_DIRTY;
}

void screen_touch_line(void)
{
    if (!(screen_get()->lines[screen_get()->y].isdirty & L_DIRTY)) {
        for (int i = 0; i < screen_get()->col_count; i++)
            screen_get()->lines[screen_get()->y].cells[i].prop &= ~S_DIRTY;
        screen_get()->lines[screen_get()->y].isdirty |= L_DIRTY;
    }
}

void screen_standout(void)
{
    screen_get()->mode |= S_STANDOUT;
}

void screen_standend(void)
{
    screen_get()->mode &= ~S_STANDOUT;
}

void screen_toggle_stand(void)
{
    struct ScreenCell* p = screen_get()->lines[screen_get()->y].cells;
    p[screen_get()->x].prop ^= S_STANDOUT;
    if (CHAR_MODE(p[screen_get()->x].prop) != C_WCHAR2) {
        for (int i = screen_get()->x + 1; CHAR_MODE(p[i].prop) == C_WCHAR2; i++)
            p[i].prop ^= S_STANDOUT;
    }
}

void screen_bold(void)
{
    screen_get()->mode |= S_BOLD;
}

void screen_boldend(void)
{
    screen_get()->mode &= ~S_BOLD;
}

void screen_underline(void)
{
    screen_get()->mode |= S_UNDERLINE;
}

void screen_underlineend(void)
{
    screen_get()->mode &= ~S_UNDERLINE;
}

void screen_graphstart(void)
{
    screen_get()->mode |= S_GRAPHICS;
}

void screen_graphend(void)
{
    screen_get()->mode &= ~S_GRAPHICS;
}

void screen_setfcolor(int color)
{
    screen_get()->mode &= ~COL_FCOLOR;
    if ((color & 0xf) <= 7)
        screen_get()->mode |= (((color & 7) | 8) << 8);
}

void screen_setbcolor(int color)
{
    screen_get()->mode &= ~COL_BCOLOR;
    if ((color & 0xf) <= 7)
        screen_get()->mode |= (((color & 7) | 8) << 12);
}

void screen_clear(void)
{
    screen_move(0, 0);
    for (int i = 0; i < screen_get()->line_count; i++) {
        screen_get()->lines[i].isdirty = 0;
        struct ScreenCell* p = screen_get()->lines[i].cells;
        for (int j = 0; j < screen_get()->col_count; j++) {
            p[j].prop = S_EOL;
        }
    }
    screen_get()->mode = C_ASCII;
}

// XXX: conflicts with curses's clrtoeol(3) ?
// Clear to the end of line
void screen_clrtoeol(void)
{
    struct ScreenCell* p = screen_get()->lines[screen_get()->y].cells;

    if (p[screen_get()->x].prop & S_EOL)
        return;

    if (!(screen_get()->lines[screen_get()->y].isdirty & (L_NEED_CE | L_CLRTOEOL)) || screen_get()->lines[screen_get()->y].eol > screen_get()->x)
        screen_get()->lines[screen_get()->y].eol = screen_get()->x;

    screen_get()->lines[screen_get()->y].isdirty |= L_CLRTOEOL;
    screen_touch_line();
    for (int i = screen_get()->x; i < screen_get()->col_count && !(p[i].prop & S_EOL); i++) {
        p[i].prop = S_EOL | S_DIRTY;
    }
}

static void
screen_clrtoeol_with_bcolor(void)
{
    if (!(screen_get()->mode & S_BCOLORED)) {
        screen_clrtoeol();
        return;
    }
    int cli = screen_get()->y;
    int cco = screen_get()->x;
    enum ScreenCellProperty pr = screen_get()->mode;
    screen_get()->mode = (screen_get()->mode & (M_CEOL | S_BCOLORED)) | C_ASCII;
    for (int i = screen_get()->x; i < screen_get()->col_count; i++)
        screen_add_whitespace();
    screen_move(cli, cco);
    screen_get()->mode = pr;
}

void screen_clrtoeolx(void)
{
    screen_clrtoeol_with_bcolor();
}

static void
screen_clrtobot_eol(void (*clrtoeol)())
{
    int l = screen_get()->y;
    int c = screen_get()->x;
    (*clrtoeol)();
    screen_get()->x = 0;
    screen_get()->y++;
    for (; screen_get()->y < screen_get()->line_count; screen_get()->y++)
        (*clrtoeol)();
    screen_get()->y = l;
    screen_get()->x = c;
}

void screen_clrtobotx(void)
{
    screen_clrtobot_eol(screen_clrtoeolx);
}

void screen_touch_cursor(void)
{
    int i;
    screen_touch_line();
    for (i = screen_get()->x; i >= 0; i--) {
        screen_touch_column(i);
        if (CHAR_MODE(screen_get()->lines[screen_get()->y].cells[i].prop) != C_WCHAR2)
            break;
    }
    for (i = screen_get()->x + 1; i < screen_get()->col_count; i++) {
        if (CHAR_MODE(screen_get()->lines[screen_get()->y].cells[i].prop) != C_WCHAR2)
            break;
        screen_touch_column(i);
    }
}
