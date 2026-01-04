#include "menu.h"
#include "etc.h"
#include "func.h"
#include "screen.h"
#include "alloc.h"
#include "symbol.h"
#include "message.h"
#include "linein.h"
#include "search.h"
#include "buffer.h"
#include "anchor.h"
#include "w3m_rc.h"
#include "tab.h"
#include "image.h"
#include "myctype.h"
#include "regex.h"
#include "funcheader.h"
#include "funcname1.h"

#include <libwc/ces.h>
#include <libwc/charset.h>
#include <libwc/status.h>
#include <libwc/wtf_len.h>
#include <string.h>

static char** FRAME;
static int FRAME_WIDTH;
static bool graph_mode = false;
#define G_start                  \
    {                            \
        if (graph_mode)          \
            screen_graphstart(); \
    }
#define G_end                  \
    {                          \
        if (graph_mode)        \
            screen_graphend(); \
    }

static struct Menu SelectMenu;
static int SelectV = 0;
static void initSelectMenu(void);
static void smChBuf(void);

/* --- SelectMenu (END) --- */

/* --- SelTabMenu --- */

static struct Menu SelTabMenu;
static int SelTabV = 0;
static void initSelTabMenu(void);
static void smChTab(void);

/* --- SelTabMenu (END) --- */

/* --- MainMenu --- */

static struct Menu MainMenu;
static enum wc_ces MainMenuCharset = WC_CES_US_ASCII; /* FIXME: charset of source code */
static int MainMenuEncode = false;

#define _(Text) Text
#define N_(Text) Text
#define gettext(Text) Text

static struct MenuItem MainMenuItem[] = {
    /* type        label           variable value func     popup keys data  */
    { MENU_FUNC, N_(" Back         (b) "), NULL, 0, backBf, NULL, "b", NULL },
    { MENU_POPUP, N_(" Select Buffer(s) "), NULL, 0, NULL, &SelectMenu, "s",
        NULL },
    { MENU_POPUP, N_(" Select Tab   (t) "), NULL, 0, NULL, &SelTabMenu, "tT",
        NULL },
    { MENU_FUNC, N_(" View Source  (v) "), NULL, 0, vwSrc, NULL, "vV", NULL },
    { MENU_FUNC, N_(" Edit Source  (e) "), NULL, 0, editBf, NULL, "eE", NULL },
    { MENU_FUNC, N_(" Save Source  (S) "), NULL, 0, svSrc, NULL, "S", NULL },
    { MENU_FUNC, N_(" Reload       (r) "), NULL, 0, reload, NULL, "rR", NULL },
    { MENU_NOP, N_(" ---------------- "), NULL, 0, nulcmd, NULL, "", NULL },
    { MENU_FUNC, N_(" Go Link      (a) "), NULL, 0, followA, NULL, "a", NULL },
    { MENU_FUNC, N_("   on New Tab (n) "), NULL, 0, tabA, NULL, "nN", NULL },
    { MENU_FUNC, N_(" Save Link    (A) "), NULL, 0, svA, NULL, "A", NULL },
    { MENU_FUNC, N_(" View Image   (i) "), NULL, 0, followI, NULL, "i", NULL },
    { MENU_FUNC, N_(" Save Image   (I) "), NULL, 0, svI, NULL, "I", NULL },
    { MENU_FUNC, N_(" View Frame   (f) "), NULL, 0, rFrame, NULL, "fF", NULL },
    { MENU_NOP, N_(" ---------------- "), NULL, 0, nulcmd, NULL, "", NULL },
    { MENU_FUNC, N_(" Bookmark     (B) "), NULL, 0, ldBmark, NULL, "B", NULL },
    { MENU_FUNC, N_(" Help         (h) "), NULL, 0, ldhelp, NULL, "hH", NULL },
    { MENU_FUNC, N_(" Option       (o) "), NULL, 0, ldOpt, NULL, "oO", NULL },
    { MENU_NOP, N_(" ---------------- "), NULL, 0, nulcmd, NULL, "", NULL },
    { MENU_FUNC, N_(" Quit         (q) "), NULL, 0, qquitfm, NULL, "qQ", NULL },
    { MENU_END, "", NULL, 0, nulcmd, NULL, "", NULL },
};

/* --- MainMenu (END) --- */

static MenuList* w3mMenuList;

static struct Menu* CurrentMenu = NULL;

void new_menu(struct Menu* menu, struct MenuItem* item)
{
    int i, l;

    menu->cursorX = 0;
    menu->cursorY = 0;
    menu->x = 0;
    menu->y = 0;
    menu->nitem = 0;
    menu->item = item;
    menu->initial = 0;
    menu->select = 0;
    menu->offset = 0;
    menu->active = 0;

    if (item == NULL)
        return;

    for (i = 0; item[i].type != MENU_END; i++)
        ;
    menu->nitem = i;
    menu->height = menu->nitem;
    for (i = 0; i < 128; i++)
        menu->keymap[i] = MenuKeymap[i];
    menu->width = 0;
    for (i = 0; i < menu->nitem; i++) {
        const char* p = item[i].keys;
        if (p != NULL) {
            while (*p) {
                if (IS_ASCII(*p)) {
                    menu->keymap[(int)*p] = mSelect;
                    menu->keyselect[(int)*p] = i;
                }
                p++;
            }
        }
        l = get_strwidth(item[i].label);
        if (l > menu->width)
            menu->width = l;
    }
}

void geom_menu(struct Menu* menu, int x, int y, int mselect)
{
    int win_x, win_y, win_w, win_h;

    menu->select = mselect;

    if (menu->width % FRAME_WIDTH)
        menu->width = (menu->width / FRAME_WIDTH + 1) * FRAME_WIDTH;
    win_x = menu->x - FRAME_WIDTH;
    win_w = menu->width + 2 * FRAME_WIDTH;
    if (win_x + win_w > TTY_COLS())
        win_x = TTY_COLS() - win_w;
    if (win_x < 0) {
        win_x = 0;
        if (win_w > TTY_COLS()) {
            menu->width = TTY_COLS() - 2 * FRAME_WIDTH;
            menu->width -= menu->width % FRAME_WIDTH;
        }
    }
    menu->x = win_x + FRAME_WIDTH;

    win_y = menu->y - mselect - 1;
    win_h = menu->height + 2;
    if (win_y + win_h > LASTLINE())
        win_y = LASTLINE() - win_h;
    if (win_y < 0) {
        win_y = 0;
        if (win_y + win_h > LASTLINE()) {
            win_h = LASTLINE() - win_y;
            menu->height = win_h - 2;
            if (menu->height <= mselect)
                menu->offset = mselect - menu->height + 1;
        }
    }
    menu->y = win_y + 1;
}

void draw_all_menu(struct Menu* menu)
{
    if (menu->parent != NULL)
        draw_all_menu(menu->parent);
    draw_menu(menu);
}

void draw_menu(struct Menu* menu)
{
    int x = menu->x - FRAME_WIDTH;
    int w = menu->width + 2 * FRAME_WIDTH;
    int y = menu->y - 1;

    if (menu->offset == 0) {
        G_start;
        mvaddstr((struct Vec2) { .y = y, .x = x }, FRAME[3]);
        int i = FRAME_WIDTH;
        for (; i < w - FRAME_WIDTH; i += FRAME_WIDTH)
            mvaddstr((struct Vec2) { .y = y, .x = x + i }, FRAME[10]);
        mvaddstr((struct Vec2) { .y = y, .x = x + i }, FRAME[6]);
        G_end;
    } else {
        G_start;
        mvaddstr((struct Vec2) { .y = y, .x = x }, FRAME[5]);
        G_end;
        int i = FRAME_WIDTH;
        for (; i < w - FRAME_WIDTH; i++)
            mvaddstr((struct Vec2) { .y = y, .x = x + i }, " ");
        G_start;
        mvaddstr((struct Vec2) { .y = y, .x = x + i }, FRAME[5]);
        G_end;
        i = (w / 2 - 1) / FRAME_WIDTH * FRAME_WIDTH;
        mvaddstr((struct Vec2) { .y = y, .x = x + i }, ":");
    }

    for (int j = 0; j < menu->height; j++) {
        y++;
        G_start;
        mvaddstr((struct Vec2) { .y = y, .x = x }, FRAME[5]);
        G_end;
        draw_menu_item(menu, menu->offset + j);
        G_start;
        mvaddstr((struct Vec2) { .y = y, .x = x + w - FRAME_WIDTH }, FRAME[5]);
        G_end;
    }
    y++;
    if (menu->offset + menu->height == menu->nitem) {
        G_start;
        mvaddstr((struct Vec2) { .y = y, .x = x }, FRAME[9]);
        int i = FRAME_WIDTH;
        for (; i < w - FRAME_WIDTH; i += FRAME_WIDTH)
            mvaddstr((struct Vec2) { .y = y, .x = x + i }, FRAME[10]);
        mvaddstr((struct Vec2) { .y = y, .x = x + i }, FRAME[12]);
        G_end;
    } else {
        G_start;
        mvaddstr((struct Vec2) { .y = y, .x = x }, FRAME[5]);
        G_end;
        int i = FRAME_WIDTH;
        for (; i < w - FRAME_WIDTH; i++)
            mvaddstr((struct Vec2) { .y = y, .x = x + i }, " ");
        G_start;
        mvaddstr((struct Vec2) { .y = y, .x = x + i }, FRAME[5]);
        G_end;
        i = (w / 2 - 1) / FRAME_WIDTH * FRAME_WIDTH;
        mvaddstr((struct Vec2) { .y = y, .x = x + i }, ":");
    }
}

void draw_menu_item(struct Menu* menu, int mselect)
{
    mvaddnstr((struct Vec2) { .y = menu->y + mselect - menu->offset, .x = menu->x },
        menu->item[mselect].label, menu->width);
}

int select_menu(struct Menu* menu, int mselect)
{
    if (mselect < 0 || mselect >= menu->nitem)
        return (MENU_NOTHING);
    if (mselect < menu->offset)
        up_menu(menu, menu->offset - mselect);
    else if (mselect >= menu->offset + menu->height)
        down_menu(menu, mselect - menu->offset - menu->height + 1);

    if (menu->select >= menu->offset && menu->select < menu->offset + menu->height)
        draw_menu_item(menu, menu->select);
    menu->select = mselect;
    screen_standout();
    draw_menu_item(menu, menu->select);
    screen_standend();
    /*
     * move(menu->cursorY, menu->doc.cursorX); */
    screen_move((struct Vec2) { .y = menu->y + mselect - menu->offset, .x = menu->x });
    screen_toggle_stand();
    tty_write_screen();

    return (menu->select);
}

void goto_menu(struct Menu* menu, int mselect, int down)
{
    int select_in;
    if (mselect >= menu->nitem)
        mselect = menu->nitem - 1;
    else if (mselect < 0)
        mselect = 0;
    select_in = mselect;
    while (menu->item[mselect].type == MENU_NOP) {
        if (down > 0) {
            if (++mselect >= menu->nitem) {
                down_menu(menu, select_in - menu->select);
                mselect = menu->select;
                break;
            }
        } else if (down < 0) {
            if (--mselect < 0) {
                up_menu(menu, menu->select - select_in);
                mselect = menu->select;
                break;
            }
        } else {
            return;
        }
    }
    select_menu(menu, mselect);
}

void up_menu(struct Menu* menu, int n)
{
    if (n < 0 || menu->offset == 0)
        return;
    menu->offset -= n;
    if (menu->offset < 0)
        menu->offset = 0;

    draw_menu(menu);
}

void down_menu(struct Menu* menu, int n)
{
    if (n < 0 || menu->offset + menu->height == menu->nitem)
        return;
    menu->offset += n;
    if (menu->offset + menu->height > menu->nitem)
        menu->offset = menu->nitem - menu->height;

    draw_menu(menu);
}

bool action_menu(struct Menu* menu)
{
    if (!menu->active) {
        if (menu->parent) {
            menu->parent->active = false;
        }
        return false;
    }

    draw_all_menu(menu);
    select_menu(menu, menu->select);

    enum MenuResult mselect = MENU_NOTHING;
    while (1) {
        int ch = getch();
        if (IS_ASCII(ch)) { /* Ascii */
            enum MenuResult mselect = (*menu->keymap[ch])(
                (struct DefunContext) {
                    .tab = getRuntime()->CurrentTab,
                    .buf = getRuntime()->CurrentTab->currentBuffer,
                },
                ch);
            if (mselect != MENU_NOTHING) {
                break;
            }
        }
    }

    // char c;
    // int mselect;

    if (mselect >= 0 && mselect < menu->nitem) {
        struct MenuItem item = menu->item[mselect];
        if (item.type & MENU_POPUP) {
            popup_menu(menu, item.popup);
            return (1);
        }
        if (menu->parent != NULL)
            menu->parent->active = 0;
        if (item.type & MENU_VALUE)
            *item.variable = item.value;
        if (item.type & MENU_FUNC) {
            getRuntime()->CurrentKey = -1;
            getRuntime()->CurrentKeyData = NULL;
            getRuntime()->CurrentCmdData = item.data;
            (*item.func)((struct DefunContext) {
                .tab = getRuntime()->CurrentTab,
                .buf = getRuntime()->CurrentTab->currentBuffer,
            });
            getRuntime()->CurrentCmdData = NULL;
        }
    } else if (mselect == MENU_CLOSE) {
        if (menu->parent != NULL)
            menu->parent->active = 0;
    }
    return (0);
}

void popup_menu(struct Menu* parent, struct Menu* menu)
{
    if (menu->item == NULL || menu->nitem == 0)
        return;
    if (menu->active)
        return;

    menu->parent = parent;
    menu->select = menu->initial;
    menu->offset = 0;
    menu->active = 1;
    if (parent != NULL) {
        menu->cursorX = parent->cursorX;
        menu->cursorY = parent->cursorY;
        guess_menu_xy(parent, menu->width, &menu->x, &menu->y);
    }
    geom_menu(menu, menu->x, menu->y, menu->select);

    CurrentMenu = menu;

    for (bool active = 1; active; active = action_menu(CurrentMenu)) {
        ;
    }

    menu->active = 0;
    CurrentMenu = parent;
}

void guess_menu_xy(struct Menu* parent, int width, int* x, int* y)
{
    *x = parent->x + parent->width + FRAME_WIDTH - 1;
    if (*x + width + FRAME_WIDTH > TTY_COLS()) {
        *x = TTY_COLS() - width - FRAME_WIDTH;
        if ((parent->x + parent->width / 2 > *x) && (parent->x + parent->width / 2 > TTY_COLS() / 2))
            *x = parent->x - width - FRAME_WIDTH + 1;
    }
    *y = parent->y + parent->select - parent->offset;
}

void new_option_menu(struct Menu* menu,
    const char** label, int* variable, void (*func)())
{
    int i, nitem;
    const char** p;
    struct MenuItem* item;

    if (label == NULL || *label == NULL)
        return;

    for (i = 0, p = label; *p != NULL; i++, p++)
        ;
    nitem = i;

    item = New_N(struct MenuItem, nitem + 1);

    for (i = 0, p = label; i < nitem; i++, p++) {
        if (func != NULL)
            item[i].type = MENU_VALUE | MENU_FUNC;
        else
            item[i].type = MENU_VALUE;
        item[i].label = *p;
        item[i].variable = variable;
        item[i].value = i;
        item[i].func = func;
        item[i].popup = NULL;
        item[i].keys = "";
    }
    item[nitem].type = MENU_END;

    new_menu(menu, item);
}

static void
set_menu_frame(void)
{
    if (graph_ok()) {
        graph_mode = TRUE;
        FRAME_WIDTH = 1;
        FRAME = graph_symbol;
    } else {
        graph_mode = FALSE;

        FRAME_WIDTH = 0;
        FRAME = get_symbol(getRuntime()->DisplayCharset, &FRAME_WIDTH);
        if (!WcOption.use_wide)
            FRAME_WIDTH = 1;
    }
}

/* --- MenuFunctions --- */

int mEsc(struct DefunContext ctx, char c)
{
    c = getch();
    return (MenuEscKeymap[(int)c](ctx, c));
}

int mEscB(struct DefunContext ctx, char c)
{
    c = getch();
    if (IS_DIGIT(c))
        return (mEscD(ctx, c));
    else
        return (MenuEscBKeymap[(int)c](ctx, c));
}

int mEscD(struct DefunContext ctx, char c)
{
    int d;

    d = (int)c - (int)'0';
    c = getch();
    if (IS_DIGIT(c)) {
        d = d * 10 + (int)c - (int)'0';
        c = getch();
    }
    if (c == '~')
        return (MenuEscDKeymap[d](ctx, c));
    else
        return (MENU_NOTHING);
}

enum MenuResult mNull(struct DefunContext ctx, char c)
{
    return (MENU_NOTHING);
}

enum MenuResult mSelect(struct DefunContext ctx, char c)
{
    if (IS_ASCII(c))
        return (select_menu(CurrentMenu, CurrentMenu->keyselect[(int)c]));
    else
        return (MENU_NOTHING);
}

enum MenuResult mDown(struct DefunContext ctx, char c)
{
    if (CurrentMenu->select >= CurrentMenu->nitem - 1)
        return (MENU_NOTHING);
    goto_menu(CurrentMenu, CurrentMenu->select + 1, 1);
    return (MENU_NOTHING);
}

int mUp(struct DefunContext ctx, char c)
{
    if (CurrentMenu->select <= 0)
        return (MENU_NOTHING);
    goto_menu(CurrentMenu, CurrentMenu->select - 1, -1);
    return (MENU_NOTHING);
}

int mLast(struct DefunContext ctx, char c)
{
    goto_menu(CurrentMenu, CurrentMenu->nitem - 1, -1);
    return (MENU_NOTHING);
}

int mTop(struct DefunContext ctx, char c)
{
    goto_menu(CurrentMenu, 0, 1);
    return (MENU_NOTHING);
}

int mNext(struct DefunContext ctx, char c)
{
    int mselect = CurrentMenu->select + CurrentMenu->height;

    if (mselect >= CurrentMenu->nitem)
        return mLast(ctx, c);
    down_menu(CurrentMenu, CurrentMenu->height);
    goto_menu(CurrentMenu, mselect, -1);
    return (MENU_NOTHING);
}

int mPrev(struct DefunContext ctx, char c)
{
    int mselect = CurrentMenu->select - CurrentMenu->height;

    if (mselect < 0)
        return mTop(ctx, c);
    up_menu(CurrentMenu, CurrentMenu->height);
    goto_menu(CurrentMenu, mselect, 1);
    return (MENU_NOTHING);
}

int mFore(struct DefunContext ctx, char c)
{
    if (CurrentMenu->select >= CurrentMenu->nitem - 1)
        return (MENU_NOTHING);
    goto_menu(CurrentMenu, (CurrentMenu->select + CurrentMenu->height - 1),
        (CurrentMenu->height + 1));
    return (MENU_NOTHING);
}

int mBack(struct DefunContext ctx, char c)
{
    if (CurrentMenu->select <= 0)
        return (MENU_NOTHING);
    goto_menu(CurrentMenu, (CurrentMenu->select - CurrentMenu->height + 1),
        (-1 - CurrentMenu->height));
    return (MENU_NOTHING);
}

int mLineU(struct DefunContext ctx, char c)
{
    int mselect = CurrentMenu->select;

    if (mselect >= CurrentMenu->nitem)
        return mLast(ctx, c);
    if (CurrentMenu->offset + CurrentMenu->height >= CurrentMenu->nitem)
        mselect++;
    else {
        down_menu(CurrentMenu, 1);
        if (mselect < CurrentMenu->offset)
            mselect++;
    }
    goto_menu(CurrentMenu, mselect, 1);
    return (MENU_NOTHING);
}

int mLineD(struct DefunContext ctx, char c)
{
    int mselect = CurrentMenu->select;

    if (mselect <= 0)
        return mTop(ctx, c);
    if (CurrentMenu->offset <= 0)
        mselect--;
    else {
        up_menu(CurrentMenu, 1);
        if (mselect >= CurrentMenu->offset + CurrentMenu->height)
            mselect--;
    }
    goto_menu(CurrentMenu, mselect, -1);
    return (MENU_NOTHING);
}

int mOk(struct DefunContext ctx, char c)
{
    int mselect = CurrentMenu->select;

    if (CurrentMenu->item[mselect].type == MENU_NOP)
        return (MENU_NOTHING);
    return (mselect);
}

int mCancel(struct DefunContext ctx, char c)
{
    return (MENU_CANCEL);
}

int mClose(struct DefunContext ctx, char c)
{
    return (MENU_CLOSE);
}

int mSusp(struct DefunContext ctx, char c)
{
    susp(ctx);
    draw_all_menu(CurrentMenu);
    select_menu(CurrentMenu, CurrentMenu->select);
    return (MENU_NOTHING);
}

static const char* SearchString = NULL;

int (*menuSearchRoutine)(struct Menu*, const char*, int);

static int
menuForwardSearch(struct Menu* menu, const char* str, int from)
{
    const char* p = regexCompile(str, getRuntime()->IgnoreCase);
    if (p) {
        message(p);
        return -1;
    }
    if (from < 0)
        from = 0;
    for (int i = from; i < menu->nitem; i++)
        if (menu->item[i].type != MENU_NOP && regexMatch(menu->item[i].label, -1, 1) == 1)
            return i;
    return -1;
}

static int
menu_search_forward(struct Document* doc, struct Menu* menu, int from)
{
    const char* str = inputStrHist("Forward: ", NULL, getRuntime()->TextHist);
    if (str != NULL && *str == '\0')
        str = SearchString;
    if (str == NULL || *str == '\0')
        return -1;
    SearchString = str;
    str = conv_search_string(str, getRuntime()->DisplayCharset, doc->charset);
    menuSearchRoutine = menuForwardSearch;
    int found = menuForwardSearch(menu, str, from + 1);
    if (getRuntime()->WrapSearch && found == -1)
        found = menuForwardSearch(menu, str, 0);
    if (found >= 0)
        return found;
    disp_message("Not found", TRUE);
    return -1;
}

int mSrchF(struct DefunContext ctx, char c)
{
    int mselect = menu_search_forward(&ctx.buf->doc, CurrentMenu, CurrentMenu->select);
    if (mselect >= 0)
        goto_menu(CurrentMenu, mselect, 1);
    return (MENU_NOTHING);
}

static int
menuBackwardSearch(struct Menu* menu, const char* str, int from)
{
    const char* p = regexCompile(str, getRuntime()->IgnoreCase);
    if (p) {
        message(p);
        return -1;
    }
    if (from >= menu->nitem)
        from = menu->nitem - 1;
    for (int i = from; i >= 0; i--)
        if (menu->item[i].type != MENU_NOP && regexMatch(menu->item[i].label, -1, 1) == 1)
            return i;
    return -1;
}

static int
menu_search_backward(struct Document *doc, struct Menu* menu, int from)
{
    const char* str = inputStrHist("Backward: ", NULL, getRuntime()->TextHist);
    if (str != NULL && *str == '\0')
        str = SearchString;
    if (str == NULL || *str == '\0')
        return -1;
    SearchString = str;
    str = conv_search_string(str, getRuntime()->DisplayCharset, doc->charset);
    menuSearchRoutine = menuBackwardSearch;
    int found = menuBackwardSearch(menu, str, from - 1);
    if (getRuntime()->WrapSearch && found == -1)
        found = menuBackwardSearch(menu, str, menu->nitem);
    if (found >= 0)
        return found;
    disp_message("Not found", TRUE);
    return -1;
}

int mSrchB(struct DefunContext ctx, char c)
{
    int mselect = menu_search_backward(&ctx.buf->doc, CurrentMenu, CurrentMenu->select);
    if (mselect >= 0)
        goto_menu(CurrentMenu, mselect, -1);
    return (MENU_NOTHING);
}

static int
menu_search_next_previous(struct Document *doc, struct Menu* menu, int from, int reverse)
{
    static int (*routine[2])(struct Menu*, const char*, int) = {
        menuForwardSearch, menuBackwardSearch
    };

    if (menuSearchRoutine == NULL) {
        disp_message("No previous regular expression", TRUE);
        return -1;
    }
    const char* str = conv_search_string(SearchString, getRuntime()->DisplayCharset, doc->charset);
    if (reverse != 0)
        reverse = 1;
    if (menuSearchRoutine == menuBackwardSearch)
        reverse ^= 1;
    from += reverse ? -1 : 1;
    int found = (*routine[reverse])(menu, str, from);
    if (getRuntime()->WrapSearch && found == -1)
        found = (*routine[reverse])(menu, str, reverse * menu->nitem);
    if (found >= 0)
        return found;
    disp_message("Not found", TRUE);
    return -1;
}

int mSrchN(struct DefunContext ctx, char c)
{
    int mselect = menu_search_next_previous(&ctx.buf->doc, CurrentMenu, CurrentMenu->select, 0);
    if (mselect >= 0)
        goto_menu(CurrentMenu, mselect, 1);
    return (MENU_NOTHING);
}

int mSrchP(struct DefunContext ctx, char c)
{
    int mselect = menu_search_next_previous(&ctx.buf->doc, CurrentMenu, CurrentMenu->select, 1);
    if (mselect >= 0)
        goto_menu(CurrentMenu, mselect, -1);
    return (MENU_NOTHING);
}

int mMouse(struct DefunContext ctx, char c)
{
    return (MENU_NOTHING);
}

int mSgrMouse(struct DefunContext ctx, char c)
{
    return (MENU_NOTHING);
}

/* --- MenuFunctions (END) --- */

/* --- MainMenu --- */

void popupMenu(int x, int y, struct Menu* menu)
{
    set_menu_frame();

    initSelectMenu();
    initSelTabMenu();

    menu->cursorX = Currentbuf->doc.cursorX + Currentbuf->doc.rootX;
    menu->cursorY = Currentbuf->doc.cursorY + Currentbuf->doc.rootY;
    menu->x = x + FRAME_WIDTH + 1;
    menu->y = y + 2;

    popup_menu(NULL, menu);
}

void mainMenu(int x, int y)
{
    popupMenu(x, y, &MainMenu);
}

DEFUN(mainMn, MAIN_MENU MENU, "Pop up menu")
{
    struct Menu* menu = &MainMenu;
    char* data;
    int n;
    int x = Currentbuf->doc.cursorX + Currentbuf->doc.rootX,
        y = Currentbuf->doc.cursorY + Currentbuf->doc.rootY;

    data = searchKeyData();
    if (data != NULL) {
        n = getMenuN(w3mMenuList, data);
        if (n < 0)
            return;
        menu = w3mMenuList[n].menu;
    }
    popupMenu(x, y, menu);
}

/* --- MainMenu (END) --- */

/* --- SelectMenu --- */

DEFUN(selMn, SELECT_MENU, "Pop up buffer-stack menu")
{
    int x = Currentbuf->doc.cursorX + Currentbuf->doc.rootX,
        y = Currentbuf->doc.cursorY + Currentbuf->doc.rootY;

    popupMenu(x, y, &SelectMenu);
}

// typedef enum MenuResult (*MenuKeyFunc)(struct DefunContext ctx, char ch);
static enum MenuResult
smDelBuf(struct DefunContext ctx, char c)
{
    int i, x, y, mselect;
    struct Buffer* buf;

    if (CurrentMenu->select < 0 || CurrentMenu->select >= SelectMenu.nitem)
        return (MENU_NOTHING);
    for (i = 0, buf = Firstbuf; i < CurrentMenu->select;
        i++, buf = buf->nextBuffer)
        ;
    if (Currentbuf == buf)
        Currentbuf = buf->nextBuffer;
    Firstbuf = deleteBuffer(Firstbuf, buf);
    if (!Currentbuf)
        Currentbuf = nthBuffer(Firstbuf, i - 1);
    ;
    if (Firstbuf == NULL) {
        Firstbuf = nullBuffer();
        Currentbuf = Firstbuf;
    }

    x = CurrentMenu->x;
    y = CurrentMenu->y;
    mselect = CurrentMenu->select;

    initSelectMenu();

    CurrentMenu->x = x;
    CurrentMenu->y = y;

    geom_menu(CurrentMenu, x, y, 0);

    CurrentMenu->select = (mselect <= CurrentMenu->nitem - 2) ? mselect
                                                              : (CurrentMenu->nitem - 2);
    draw_all_menu(CurrentMenu);
    select_menu(CurrentMenu, CurrentMenu->select);
    return (MENU_NOTHING);
}

static void
initSelectMenu(void)
{
    int i, nitem, len = 0, l;
    struct Buffer* buf;
    Str str;
    const char** label;
    char* p;
    static char* comment = " SPC for select / D for delete buffer ";

    SelectV = -1;
    for (i = 0, buf = Firstbuf; buf != NULL; i++, buf = buf->nextBuffer) {
        if (buf == Currentbuf)
            SelectV = i;
    }
    nitem = i;

    label = New_N(char*, nitem + 2);
    for (i = 0, buf = Firstbuf; i < nitem; i++, buf = buf->nextBuffer) {
        str = Sprintf("<%s>", buf->doc.title);
        if (buf->content.filename != NULL) {
            switch (buf->content.url.scheme) {
            case SCM_LOCAL:
                if (strcmp(buf->content.url.file, "-")) {
                    Strcat_char(str, ' ');
                    Strcat_charp(str,
                        conv_from_system(buf->content.url.real_file));
                }
                break;
                /* case SCM_UNKNOWN: */
            case SCM_MISSING:
                break;
            default:
                Strcat_char(str, ' ');
                p = url_decode2(parsedURL2Str(&buf->content.url)->ptr, NULL);
                Strcat_charp(str, p);
                break;
            }
        }
        label[i] = str->ptr;
        if (len < str->length)
            len = str->length;
    }
    l = get_strwidth(comment);
    if (len < l + 4)
        len = l + 4;
    if (len > TTY_COLS() - 2 * FRAME_WIDTH)
        len = TTY_COLS() - 2 * FRAME_WIDTH;
    len = (len > 1) ? ((len - l + 1) / 2) : 0;
    str = Strnew();
    for (i = 0; i < len; i++)
        Strcat_char(str, '-');
    Strcat_charp(str, comment);
    for (i = 0; i < len; i++)
        Strcat_char(str, '-');
    label[nitem] = str->ptr;
    label[nitem + 1] = NULL;

    new_option_menu(&SelectMenu, label, &SelectV, smChBuf);
    SelectMenu.initial = SelectV;
    SelectMenu.cursorX = Currentbuf->doc.cursorX + Currentbuf->doc.rootX;
    SelectMenu.cursorY = Currentbuf->doc.cursorY + Currentbuf->doc.rootY;
    SelectMenu.keymap['D'] = smDelBuf;
    SelectMenu.item[nitem].type = MENU_NOP;
}

static void
smChBuf(void)
{
    int i;
    struct Buffer* buf;

    if (SelectV < 0 || SelectV >= SelectMenu.nitem)
        return;
    for (i = 0, buf = Firstbuf; i < SelectV; i++, buf = buf->nextBuffer)
        ;
    Currentbuf = buf;
    for (buf = Firstbuf; buf != NULL; buf = buf->nextBuffer) {
        if (buf == Currentbuf)
            continue;
        deleteImage(buf);
        if (getRuntime()->clear_buffer)
            tmpClearBuffer(buf);
    }
}

/* --- SelectMenu (END) --- */

/* --- SelTabMenu --- */

DEFUN(tabMn, TAB_MENU, "Pop up tab selection menu")
{
    int x = Currentbuf->doc.cursorX + Currentbuf->doc.rootX,
        y = Currentbuf->doc.cursorY + Currentbuf->doc.rootY;

    popupMenu(x, y, &SelTabMenu);
}

static int
smDelTab(struct DefunContext ctx, char c)
{
    int i, x, y, mselect;
    struct TabBuffer* tab;

    if (CurrentMenu->select < 0 || CurrentMenu->select >= SelTabMenu.nitem)
        return (MENU_NOTHING);
    for (i = 0, tab = LastTab(); i < CurrentMenu->select && tab != NULL;
        i++, tab = tab->prevTab)
        ;
    deleteTab(tab);

    x = CurrentMenu->x;
    y = CurrentMenu->y;
    mselect = CurrentMenu->select;

    initSelTabMenu();

    CurrentMenu->x = x;
    CurrentMenu->y = y;

    geom_menu(CurrentMenu, x, y, 0);

    CurrentMenu->select = (mselect <= CurrentMenu->nitem - 2) ? mselect
                                                              : (CurrentMenu->nitem - 2);

    draw_all_menu(CurrentMenu);
    select_menu(CurrentMenu, CurrentMenu->select);
    return (MENU_NOTHING);
}

static void
initSelTabMenu(void)
{
    static char* comment = " SPC for select / D for delete tab ";
    SelTabV = -1;
    int i = 0;
    for (struct TabBuffer* tab = LastTab(); tab != NULL; i++, tab = tab->prevTab) {
        if (tab == CurrentTab())
            SelTabV = i;
    }
    int nitem = i;

    const char** label = New_N(char*, nitem + 2);
    int len = 0;
    i = 0;
    for (struct TabBuffer* tab = LastTab(); i < nitem; i++, tab = tab->prevTab) {
        struct Buffer* buf = tab->currentBuffer;
        Str str = Sprintf("<%s>", buf->doc.title);
        if (buf->content.filename != NULL) {
            switch (buf->content.url.scheme) {
            case SCM_LOCAL:
                if (strcmp(buf->content.url.file, "-")) {
                    Strcat_char(str, ' ');
                    Strcat_charp(str,
                        conv_from_system(buf->content.url.real_file));
                }
                break;
                /* case SCM_UNKNOWN: */
            case SCM_MISSING:
                break;
            default: {
                char* p = url_decode2(parsedURL2Str(&buf->content.url)->ptr, NULL);
                Strcat_charp(str, p);
                break;
            }
            }
        }
        label[i] = str->ptr;
        if (len < str->length)
            len = str->length;
    }
    int l = strlen(comment);
    if (len < l + 4)
        len = l + 4;
    if (len > TTY_COLS() - 2 * FRAME_WIDTH)
        len = TTY_COLS() - 2 * FRAME_WIDTH;
    len = (len > 1) ? ((len - l + 1) / 2) : 0;
    Str str = Strnew();
    for (i = 0; i < len; i++)
        Strcat_char(str, '-');
    Strcat_charp(str, comment);
    for (i = 0; i < len; i++)
        Strcat_char(str, '-');
    label[nitem] = str->ptr;
    label[nitem + 1] = NULL;

    new_option_menu(&SelTabMenu, label, &SelTabV, smChTab);
    SelTabMenu.initial = SelTabV;
    SelTabMenu.cursorX = Currentbuf->doc.cursorX + Currentbuf->doc.rootX;
    SelTabMenu.cursorY = Currentbuf->doc.cursorY + Currentbuf->doc.rootY;
    SelTabMenu.keymap['D'] = smDelTab;
    SelTabMenu.item[nitem].type = MENU_NOP;
}

static void
smChTab(void)
{
    int i;
    struct TabBuffer* tab;
    struct Buffer* buf;

    if (SelTabV < 0 || SelTabV >= SelTabMenu.nitem)
        return;
    for (i = 0, tab = LastTab(); i < SelTabV && tab != NULL;
        i++, tab = tab->prevTab)
        ;
    getRuntime()->CurrentTab = tab;
    for (tab = LastTab(); tab != NULL; tab = tab->prevTab) {
        if (tab == CurrentTab())
            continue;
        buf = tab->currentBuffer;
        deleteImage(buf);
        if (getRuntime()->clear_buffer)
            tmpClearBuffer(buf);
    }
}

/* --- SelectMenu (END) --- */

/* --- OptionMenu --- */

void optionMenu(int x, int y, const char** label, int* variable, int initial,
    void (*func)())
{
    struct Menu menu;

    set_menu_frame();

    new_option_menu(&menu, label, variable, func);
    menu.cursorX = TTY_COLS() - 1;
    menu.cursorY = LASTLINE();
    menu.x = x;
    menu.y = y;
    menu.initial = initial;

    popup_menu(NULL, &menu);
}

/* --- OptionMenu (END) --- */

/* --- InitMenu --- */

static void
interpret_menu(FILE* mf)
{
    Str line;
    int in_menu = 0, nmenu = 0, nitem = 0, type;
    struct MenuItem* item = NULL;

    enum wc_ces charset = getRuntime()->SystemCharset;

    while (!feof(mf)) {
        line = Strfgets(mf);
        Strchop(line);
        Strremovefirstspaces(line);
        if (line->length == 0)
            continue;

        line = wc_Str_conv(line, charset, getRuntime()->InnerCharset);

        const char* p = line->ptr;
        const char* s = getWord(&p);
        if (*s == '#') /* comment */
            continue;
        if (in_menu) {
            type = setMenuItem(&item[nitem], s, p);
            if (type == -1)
                continue; /* error */
            if (type == MENU_END)
                in_menu = 0;
            else {
                nitem++;
                item = New_Reuse(struct MenuItem, item, (nitem + 1));
                w3mMenuList[nmenu].item = item;
                item[nitem].type = MENU_END;
            }
        } else if (!strcmp(s, "menu")) {
            const char* s = getQWord(&p);
            if (*s == '\0') /* error */
                continue;
            in_menu = 1;
            if ((nmenu = getMenuN(w3mMenuList, s)) != -1)
                w3mMenuList[nmenu].item = New(struct MenuItem);
            else
                nmenu = addMenuList(&w3mMenuList, s);
            item = w3mMenuList[nmenu].item;
            nitem = 0;
            item[nitem].type = MENU_END;
        } else if (!strcmp(s, "charset") || !strcmp(s, "encoding")) {
            s = getQWord(&p);
            if (*s == '\0') /* error */
                continue;
            charset = wc_guess_charset(s, charset);
        }
    }
}

void initMenu(void)
{
    FILE* mf;
    MenuList* list;

    w3mMenuList = New_N(MenuList, 4);
    w3mMenuList[0].id = "Main";
    w3mMenuList[0].menu = &MainMenu;
    w3mMenuList[0].item = MainMenuItem;
    w3mMenuList[1].id = "Select";
    w3mMenuList[1].menu = &SelectMenu;
    w3mMenuList[1].item = NULL;
    w3mMenuList[2].id = "SelectTab";
    w3mMenuList[2].menu = &SelTabMenu;
    w3mMenuList[2].item = NULL;
    w3mMenuList[3].id = NULL;

    if (!MainMenuEncode) {
        struct MenuItem* item;

        /* FIXME: charset that gettext(3) returns */
        MainMenuCharset = getRuntime()->SystemCharset;

        for (item = MainMenuItem; item->type != MENU_END; item++)
            item->label = wc_conv(_(item->label), MainMenuCharset,
                getRuntime()->InnerCharset)
                              ->ptr;
        MainMenuEncode = TRUE;
    }

    if ((mf = fopen(confFile(MENU_FILE), "rt")) != NULL) {
        interpret_menu(mf);
        fclose(mf);
    }
    if ((mf = fopen(rcFile(MENU_FILE), "rt")) != NULL) {
        interpret_menu(mf);
        fclose(mf);
    }

    for (list = w3mMenuList; list->id != NULL; list++) {
        if (list->item == NULL)
            continue;
        new_menu(list->menu, list->item);
    }
}

int setMenuItem(struct MenuItem* item, const char* type, const char* line)
{
    char *label, *func, *popup, *keys, *data;
    int f;
    int n;

    if (type == NULL || *type == '\0') /* error */
        return -1;
    if (strcmp(type, "end") == 0) {
        item->type = MENU_END;
        return MENU_END;
    } else if (strcmp(type, "nop") == 0) {
        item->type = MENU_NOP;
        item->label = getQWord(&line);
        return MENU_NOP;
    } else if (strcmp(type, "func") == 0) {
        label = getQWord(&line);
        func = getWord(&line);
        keys = getQWord(&line);
        data = getQWord(&line);
        if (*func == '\0') /* error */
            return -1;
        item->type = MENU_FUNC;
        item->label = label;
        f = getFuncList(func);
        item->func = w3mFuncList[(f >= 0) ? f : FUNCNAME_nulcmd].func;
        item->keys = keys;
        item->data = data;
        return MENU_FUNC;
    } else if (strcmp(type, "popup") == 0) {
        label = getQWord(&line);
        popup = getQWord(&line);
        keys = getQWord(&line);
        if (*popup == '\0') /* error */
            return -1;
        item->type = MENU_POPUP;
        item->label = label;
        if ((n = getMenuN(w3mMenuList, popup)) == -1)
            n = addMenuList(&w3mMenuList, popup);
        item->popup = w3mMenuList[n].menu;
        item->keys = keys;
        return MENU_POPUP;
    }
    return -1; /* error */
}

int addMenuList(MenuList** mlist, const char* id)
{
    int n;
    MenuList* list = *mlist;

    for (n = 0; list->id != NULL; list++, n++)
        ;
    *mlist = New_Reuse(MenuList, *mlist, (n + 2));
    list = *mlist + n;
    list->id = id;
    list->menu = New(struct Menu);
    list->item = New(struct MenuItem);
    (list + 1)->id = NULL;
    return n;
}

int getMenuN(MenuList* list, const char* id)
{
    int n;

    for (n = 0; list->id != NULL; list++, n++) {
        if (strcmp(id, list->id) == 0)
            return n;
    }
    return -1;
}

/* --- InitMenu (END) --- */

struct LinkList*
link_menu(struct Buffer* buf)
{
    struct Menu menu;
    struct LinkList* l;
    int i, nitem, len = 0, linkV = -1;
    const char** label;
    Str str;
    char* p;

    if (!buf->doc.linklist)
        return NULL;

    for (i = 0, l = buf->doc.linklist; l; i++, l = l->next)
        ;
    nitem = i;

    label = New_N(char*, nitem + 1);
    for (i = 0, l = buf->doc.linklist; l; i++, l = l->next) {
        str = Strnew_charp(l->title ? l->title : "(empty)");
        if (l->type == LINK_TYPE_REL)
            Strcat_charp(str, " [Rel] ");
        else if (l->type == LINK_TYPE_REV)
            Strcat_charp(str, " [Rev] ");
        else
            Strcat_charp(str, " ");
        if (!l->url)
            p = "";
        else
            p = url_decode2(l->url, buf);
        Strcat_charp(str, p);
        label[i] = str->ptr;
        if (len < str->length)
            len = str->length;
    }
    label[nitem] = NULL;

    set_menu_frame();
    new_option_menu(&menu, label, &linkV, NULL);

    menu.initial = 0;
    menu.cursorX = buf->doc.cursorX + buf->doc.rootX;
    menu.cursorY = buf->doc.cursorY + buf->doc.rootY;
    menu.x = menu.cursorX + FRAME_WIDTH + 1;
    menu.y = menu.cursorY + 2;

    popup_menu(NULL, &menu);

    if (linkV < 0)
        return NULL;
    for (i = 0, l = buf->doc.linklist; l; i++, l = l->next) {
        if (i == linkV)
            return l;
    }
    return NULL;
}

/* --- LinkMenu (END) --- */

struct Anchor*
accesskey_menu(struct Buffer* buf)
{
    struct Menu menu;
    struct AnchorList* al = buf->doc.href;
    struct Anchor* a;
    struct Anchor** ap;
    int i, n, nitem = 0, key = -1;
    const char** label;
    char* t;
    unsigned char c;

    if (!al)
        return NULL;
    for (i = 0; i < al->nanchor; i++) {
        a = &al->anchors[i];
        if (!a->slave && a->accesskey && IS_ASCII(a->accesskey))
            nitem++;
    }
    if (!nitem)
        return NULL;

    label = New_N(char*, nitem + 1);
    ap = New_N(struct Anchor*, nitem);
    for (i = 0, n = 0; i < al->nanchor; i++) {
        a = &al->anchors[i];
        if (!a->slave && a->accesskey && IS_ASCII(a->accesskey)) {
            t = getAnchorText(buf, al, a);
            label[n] = Sprintf("%c: %s", a->accesskey, t ? t : "")->ptr;
            ap[n] = a;
            n++;
        }
    }
    label[nitem] = NULL;

    set_menu_frame();
    new_option_menu(&menu, label, &key, NULL);

    menu.initial = 0;
    menu.cursorX = buf->doc.cursorX + buf->doc.rootX;
    menu.cursorY = buf->doc.cursorY + buf->doc.rootY;
    menu.x = menu.cursorX + FRAME_WIDTH + 1;
    menu.y = menu.cursorY + 2;
    for (i = 0; i < 128; i++)
        menu.keyselect[i] = -1;
    for (i = 0; i < nitem; i++) {
        c = ap[i]->accesskey;
        menu.keymap[(int)c] = mSelect;
        menu.keyselect[(int)c] = i;
    }
    for (i = 0; i < nitem; i++) {
        c = ap[i]->accesskey;
        if (!IS_ALPHA(c) || menu.keyselect[n] >= 0)
            continue;
        c = TOLOWER(c);
        menu.keymap[(int)c] = mSelect;
        menu.keyselect[(int)c] = i;
        c = TOUPPER(c);
        menu.keymap[(int)c] = mSelect;
        menu.keyselect[(int)c] = i;
    }

    a = retrieveCurrentAnchor(buf);
    if (a && a->accesskey && IS_ASCII(a->accesskey)) {
        for (i = 0; i < nitem; i++) {
            if (a->hseq == ap[i]->hseq) {
                menu.initial = i;
                break;
            }
        }
    }

    popup_menu(NULL, &menu);

    return (key >= 0) ? ap[key] : NULL;
}

static char lmKeys[] = "abcdefgimopqrstuvwxyz";
static char lmKeys2[] = "1234567890ABCDEFGHILMOPQRSTUVWXYZ";
#define nlmKeys (sizeof(lmKeys) - 1)
#define nlmKeys2 (sizeof(lmKeys2) - 1)

static int
lmGoto(struct DefunContext ctx, char c)
{
    if (IS_ASCII(c) && CurrentMenu->keyselect[(int)c] >= 0) {
        goto_menu(CurrentMenu, CurrentMenu->nitem - 1, -1);
        goto_menu(CurrentMenu, CurrentMenu->keyselect[(int)c] * nlmKeys, 1);
    }
    return (MENU_NOTHING);
}

static int
lmSelect(struct DefunContext ctx, char c)
{
    if (IS_ASCII(c))
        return select_menu(CurrentMenu, (CurrentMenu->select / nlmKeys) * nlmKeys + CurrentMenu->keyselect[(int)c]);
    else
        return (MENU_NOTHING);
}

struct Anchor*
list_menu(struct Buffer* buf)
{
    struct AnchorList* al = buf->doc.href;
    if (!al)
        return NULL;

    int nitem = 0;
    for (int i = 0; i < al->nanchor; i++) {
        struct Anchor* a = &al->anchors[i];
        if (!a->slave)
            nitem++;
    }
    if (!nitem)
        return NULL;

    bool two = FALSE;
    if (nitem >= nlmKeys)
        two = TRUE;

    const char** label = New_N(char*, nitem + 1);
    struct Anchor** ap = New_N(struct Anchor*, nitem);
    for (int i = 0, n = 0; i < al->nanchor; i++) {
        struct Anchor* a = &al->anchors[i];
        if (!a->slave) {
            const char* t = getAnchorText(buf, al, a);
            if (!t)
                t = "";
            if (two && n >= nlmKeys2 * nlmKeys)
                label[n] = Sprintf("  : %s", t)->ptr;
            else if (two)
                label[n] = Sprintf("%c%c: %s", lmKeys2[n / nlmKeys],
                    lmKeys[n % nlmKeys], t)
                               ->ptr;
            else
                label[n] = Sprintf("%c: %s", lmKeys[n], t)->ptr;
            ap[n] = a;
            n++;
        }
    }
    label[nitem] = NULL;

    set_menu_frame();
    struct Menu menu;
    int key = -1;
    new_option_menu(&menu, label, &key, NULL);

    menu.initial = 0;
    menu.cursorX = buf->doc.cursorX + buf->doc.rootX;
    menu.cursorY = buf->doc.cursorY + buf->doc.rootY;
    menu.x = menu.cursorX + FRAME_WIDTH + 1;
    menu.y = menu.cursorY + 2;
    for (int i = 0; i < 128; i++)
        menu.keyselect[i] = -1;
    if (two) {
        for (int i = 0; i < nlmKeys2; i++) {
            char c = lmKeys2[i];
            menu.keymap[(int)c] = lmGoto;
            menu.keyselect[(int)c] = i;
        }
        for (int i = 0; i < nlmKeys; i++) {
            char c = lmKeys[i];
            menu.keymap[(int)c] = lmSelect;
            menu.keyselect[(int)c] = i;
        }
    } else {
        for (int i = 0; i < nitem; i++) {
            char c = lmKeys[i];
            menu.keymap[(int)c] = mSelect;
            menu.keyselect[(int)c] = i;
        }
    }

    struct Anchor* a = retrieveCurrentAnchor(buf);
    if (a) {
        for (int i = 0; i < nitem; i++) {
            if (a->hseq == ap[i]->hseq) {
                menu.initial = i;
                break;
            }
        }
    }

    popup_menu(NULL, &menu);

    return (key >= 0) ? ap[key] : NULL;
}
