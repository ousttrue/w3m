#include "terms.h"
#include "myctype.h"
#include <assert.h>

#define M_SPACE (S_SCREENPROP | S_COLORED | S_BCOLORED | S_GRAPHICS)
#define M_CEOL (~(M_SPACE | C_WHICHCHAR))

static void screen_cell_set(struct ScreenCell* cell, const char* ch, size_t len, enum ScreenCellProperty prop)
{
    SET_CHAR(cell, ch, len);
    SET_PROP(cell, prop);
}

void screen_addmchz(const char* pc, size_t len, size_t width)
{
    if (pc == 0 || len == 0) {
        return;
    }
    if (screen_get()->x == screen_get()->col_count)
        screen_wrap();
    if (screen_get()->x >= screen_get()->col_count)
        return;

    char ch = pc[0];

    struct ScreenCell* line = screen_get()->lines[screen_get()->y].cells;
    if (line[screen_get()->x].prop & S_EOL) {
        if (ch == ' ' && !(screen_get()->mode & M_SPACE)) {
            // advnce cursor
            screen_get()->x++;
            return;
        }
        // drop tail space
        for (int i = screen_get()->x; i >= 0; i--) {
            if (!(line[i].prop & S_EOL)) {
                break;
            }
            screen_cell_set(&line[i], SCREEN_SPACE, 1, (line[i].prop & M_CEOL) | C_ASCII);
        }
    }

    if (ch == '\t' || ch == '\n' || ch == '\r' || ch == '\b')
        SET_CHAR_MODE(&screen_get()->mode, C_CTRL);
    else if (len > 1)
        SET_CHAR_MODE(&screen_get()->mode, C_WCHAR1);
    else if (!IS_CNTRL(ch))
        SET_CHAR_MODE(&screen_get()->mode, C_ASCII);
    else
        return;

    // Required to erase bold or underlined character for some terminal emulators.
    int i = screen_get()->x + width - 1;
    if (i < screen_get()->col_count
        && (((line[i].prop & S_BOLD) && screen_need_redraw(line[i].str, line[i].prop, (char*)pc, screen_get()->mode))
            || ((line[i].prop & S_UNDERLINE) && !(screen_get()->mode & S_UNDERLINE)))) {
        screen_touch_line();
        i++;
        if (i < screen_get()->col_count) {
            screen_touch_column(i);
            if (line[i].prop & S_EOL) {
                screen_cell_set(&line[i], SCREEN_SPACE, 1, (line[i].prop & M_CEOL) | C_ASCII);
            } else {
                for (i++; i < screen_get()->col_count && CHAR_MODE(line[i].prop) == C_WCHAR2; i++)
                    screen_touch_column(i);
            }
        }
    }

    if (screen_get()->x + width > screen_get()->col_count) {
        // 全角 身切れ
        screen_touch_line();
        for (i = screen_get()->x; i < screen_get()->col_count; i++) {
            screen_cell_set(&line[i], SCREEN_SPACE, 1, (line[i].prop & ~C_WHICHCHAR) | C_ASCII);
            screen_touch_column(i);
        }

        // new line and get first cell
        screen_wrap();
        if (screen_get()->x + width > screen_get()->col_count)
            return;
        line = screen_get()->lines[screen_get()->y].cells;
    }

    if (CHAR_MODE(line[screen_get()->x].prop) == C_WCHAR2) {
        // 全角文字の先頭以外。前の文字をクリア
        screen_touch_line();
        for (i = screen_get()->x - 1; i >= 0; i--) {
            enum ScreenCellProperty l = CHAR_MODE(line[i].prop);
            screen_cell_set(&line[i], SCREEN_SPACE, 1, (line[i].prop & ~C_WHICHCHAR) | C_ASCII);
            screen_touch_column(i);
            if (l != C_WCHAR2)
                break;
        }
    }

    if (CHAR_MODE(screen_get()->mode) != C_CTRL) {
        if (screen_need_redraw(line[screen_get()->x].str, line[screen_get()->x].prop, (char*)pc, screen_get()->mode)) {
            screen_cell_set(&line[screen_get()->x], pc, len, screen_get()->mode);
            screen_touch_line();
            screen_touch_column(screen_get()->x);
            SET_CHAR_MODE(&screen_get()->mode, C_WCHAR2);
            for (i = screen_get()->x + 1; i < screen_get()->x + width; i++) {
                // 全角文字の後続cell
                screen_cell_set(&line[i], SCREEN_SPACE, 1, (line[screen_get()->x].prop & ~C_WHICHCHAR) | C_WCHAR2);
                screen_touch_column(i);
            }
            for (; i < screen_get()->col_count && CHAR_MODE(line[i].prop) == C_WCHAR2; i++) {
                // 下にあった全角文字の後続を消す
                screen_cell_set(&line[i], SCREEN_SPACE, 1, (line[i].prop & ~C_WHICHCHAR) | C_ASCII);
                screen_touch_column(i);
            }
        }
        screen_get()->x += width;
    } else if (ch == '\t') {
        int dest = (screen_get()->x + screen_get()->tab_step) / screen_get()->tab_step * screen_get()->tab_step;
        if (dest >= screen_get()->col_count) {
            screen_wrap();
            screen_touch_line();
            dest = screen_get()->tab_step;
            line = screen_get()->lines[screen_get()->y].cells;
        }
        for (i = screen_get()->x; i < dest; i++) {
            if (screen_need_redraw(line[i].str, line[i].prop, SCREEN_SPACE, screen_get()->mode)) {
                screen_cell_set(&line[i], SCREEN_SPACE, 1, screen_get()->mode);
                screen_touch_line();
                screen_touch_column(i);
            }
        }
        screen_get()->x = i;
    } else if (ch == '\n') {
        screen_wrap();
    } else if (ch == '\r') { // Carriage return
        screen_get()->x = 0;
    } else if (ch == '\b' && screen_get()->x > 0) { // Backspace
        screen_get()->x--;
        while (screen_get()->x > 0 && CHAR_MODE(line[screen_get()->x].prop) == C_WCHAR2)
            screen_get()->x--;
    }
}
