#include "menu.h"
#include "buffer_list.h"
#include "AnchorList.h"
#include "Anchor.h"
#include "runtime.h"
#include "defun_macro.h"
#include "alloc.h"
#include "display.h"
#include "rc.h"
#include "quote.h"
#include "linein.h"
#include "defun.h"
#include "search.h"
#include "history.h"
#include "buffer.h"
#include "ui.h"
#include "symbol.h"
#include "w3m.h"
#include "image.h"
#include "screen.h"
#include "screen_effects.h"
#include "TermEntry.h"
#include "graphicchar.h"
#include "keymap.h"
#include "myctype.h"
#include "regex.h"
#include "event_poller.h"
#include <stdio.h>
#include <string.h>
#include <wtf.h>

#define MENU_NOTHING -1
#define MENU_CANCEL -2
#define MENU_CLOSE -3

#define MENU_FILE "menu"

static const char** FRAME;
int FRAME_WIDTH;
#define G_start                \
    {                          \
        if (graph_mode)        \
            vt_graphstart(vt); \
    }
#define G_end                \
    {                        \
        if (graph_mode)      \
            vt_graphend(vt); \
    }

static int mNull(struct UI ui, char c);
static int mSelect(struct UI ui, char c);
static int mDown(struct UI ui, char c);
static int mUp(struct UI ui, char c);
static int mLast(struct UI ui, char c);
static int mTop(struct UI ui, char c);
static int mNext(struct UI ui, char c);
static int mPrev(struct UI ui, char c);
static int mFore(struct UI ui, char c);
static int mBack(struct UI ui, char c);
static int mLineU(struct UI ui, char c);
static int mLineD(struct UI ui, char c);
static int mOk(struct UI ui, char c);
static int mCancel(struct UI ui, char c);
static int mClose(struct UI ui, char c);
static int mMouse(struct UI ui, char c);
static int mSgrMouse(struct UI ui, char c);
static int mSrchF(struct UI ui, char c);
static int mSrchB(struct UI ui, char c);
static int mSrchN(struct UI ui, char c);
static int mSrchP(struct UI ui, char c);

static MenuFunc MenuKeymap[128] = {
    /*  C-@     C-a     C-b     C-c     C-d     C-e     C-f     C-g      */
    mNull,
    mTop,
    mPrev,
    mClose,
    mNull,
    mLast,
    mNext,
    mNull,
    /*  C-h     C-i     C-j     C-k     C-l     C-m     C-n     C-o      */
    mCancel,
    mNull,
    mOk,
    mNull,
    mNull,
    mOk,
    mDown,
    mNull,
    /*  C-p     C-q     C-r     C-s     C-t     C-u     C-v     C-w      */
    mUp,
    mNull,
    mSrchB,
    mSrchF,
    mNull,
    mNull,
    mNext,
    mNull,
    /*  C-x     C-y     C-z     C-[     C-\     C-]     C-^     C-_      */
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    /*  SPC     !       "       #       $       %       &       '        */
    mOk,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    /*  (       )       *       +       ,       -       .       /        */
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mSrchF,
    /*  0       1       2       3       4       5       6       7        */
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    /*  8       9       :       ;       <       =       >       ?        */
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mSrchB,
    /*  @       A       B       C       D       E       F       G        */
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    /*  H       I       J       K       L       M       N       O        */
    mNull,
    mNull,
    mLineU,
    mLineD,
    mNull,
    mNull,
    mSrchP,
    mNull,
    /*  P       Q       R       S       T       U       V       W        */
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    /*  X       Y       Z       [       \       ]       ^       _        */
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    /*  `       a       b       c       d       e       f       g        */
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    /*  h       i       j       k       l       m       n       o        */
    mCancel,
    mNull,
    mDown,
    mUp,
    mOk,
    mNull,
    mSrchN,
    mNull,
    /*  p       q       r       s       t       u       v       w        */
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    /*  x       y       z       {       |       }       ~       DEL      */
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mCancel,
};
// static int (*MenuEscKeymap[128])(char c) = {
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     /*                                                          O     */
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     /*                          [                                     */
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     /*                                                  v             */
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mPrev,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
//     mNull,
// };
static MenuFunc MenuEscBKeymap[128] = {
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,

    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    /*  8       9       :       ;       <       =       >       ?     */
    mNull,
    mNull,
    mNull,
    mNull,
    mSgrMouse,
    mNull,
    mNull,
    mNull,
    /*          A       B       C       D       E                     */
    mNull,
    mUp,
    mDown,
    mOk,
    mCancel,
    mClose,
    mNull,
    mNull,
    /*                                  L       M                     */
    mNull,
    mNull,
    mNull,
    mNull,
    mClose,
    mMouse,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,

    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
};
static MenuFunc MenuEscDKeymap[128] = {
    /*  0       1       INS     3       4       PgUp,   PgDn    7     */
    mNull,
    mNull,
    mClose,
    mNull,
    mNull,
    mBack,
    mFore,
    mNull,
    /*  8       9       10      F1      F2      F3      F4      F5       */
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    /*  16      F6      F7      F8      F9      F10     22      23       */
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    /*  24      25      26      27      HELP    29      30      31       */
    mNull,
    mNull,
    mNull,
    mNull,
    mClose,
    mNull,
    mNull,
    mNull,

    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,

    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,

    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
    mNull,
};

static struct Menu SelectMenu;
static int SelectV = 0;
static void initSelectMenu(struct UI ui);
static void smChBuf(struct UI ui);
static int smDelBuf(struct UI ui, char c);

/* --- MainMenu --- */

static struct Menu MainMenu;
static wc_ces MainMenuCharset = WC_CES_US_ASCII;
static int MainMenuEncode = false;

#include <libintl.h>
#define _(String) gettext(String)
#define N_(String) (String)

static struct MenuItem MainMenuItem[] = {
    /* type        label           variable value func     popup keys data  */
    { MENU_FUNC, N_(" Back         (b) "), NULL, 0, backBf, NULL, "b", NULL },
    { MENU_POPUP, N_(" Select struct Buffer(s) "), NULL, 0, NULL, &SelectMenu, "s",
        NULL },
    { MENU_FUNC, N_(" View Source  (v) "), NULL, 0, vwSrc, NULL, "vV", NULL },
    { MENU_FUNC, N_(" Edit Source  (e) "), NULL, 0, editBf, NULL, "eE", NULL },
    { MENU_FUNC, N_(" Save Source  (S) "), NULL, 0, svSrc, NULL, "S", NULL },
    { MENU_FUNC, N_(" Reload       (r) "), NULL, 0, reload, NULL, "rR", NULL },
    { MENU_NOP, N_(" ---------------- "), NULL, 0, nulcmd, NULL, "", NULL },
    { MENU_FUNC, N_(" Go Link      (a) "), NULL, 0, followA, NULL, "a", NULL },
    { MENU_FUNC, N_(" Save Link    (A) "), NULL, 0, svA, NULL, "A", NULL },
    { MENU_FUNC, N_(" View Image   (i) "), NULL, 0, followI, NULL, "i", NULL },
    { MENU_FUNC, N_(" Save Image   (I) "), NULL, 0, svI, NULL, "I", NULL },
    { MENU_NOP, N_(" ---------------- "), NULL, 0, nulcmd, NULL, "", NULL },
    { MENU_FUNC, N_(" Bookmark     (B) "), NULL, 0, ldBmark, NULL, "B", NULL },
    { MENU_FUNC, N_(" Help         (h) "), NULL, 0, ldhelp, NULL, "hH", NULL },
    { MENU_FUNC, N_(" Option       (o) "), NULL, 0, ldOpt, NULL, "oO", NULL },
    { MENU_NOP, N_(" ---------------- "), NULL, 0, nulcmd, NULL, "", NULL },
    { MENU_FUNC, N_(" Quit         (q) "), NULL, 0, qquitfm, NULL, "qQ", NULL },
    { MENU_END, "", NULL, 0, nulcmd, NULL, "", NULL },
};

/* --- MainMenu (END) --- */

static struct MenuList* w3mMenuList;

static struct Menu* CurrentMenu = NULL;

void new_menu(struct Menu* menu, struct MenuItem* item)
{

    // menu->cursorX = 0;
    // menu->cursorY = 0;
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

    int i;
    for (i = 0; item[i].type != MENU_END; i++)
        ;
    menu->nitem = i;
    menu->height = menu->nitem;
    for (i = 0; i < 128; i++)
        menu->keymap[i] = MenuKeymap[i];
    menu->width = 0;
    const char* p;
    for (i = 0; i < menu->nitem; i++) {
        if ((p = item[i].keys) != NULL) {
            while (*p) {
                if (IS_ASCII(*p)) {
                    menu->keymap[(int)*p] = mSelect;
                    menu->keyselect[(int)*p] = i;
                }
                p++;
            }
        }
        int l = get_strwidth(item[i].label);
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
    if (win_x + win_w > getScreen()->COLS)
        win_x = getScreen()->COLS - win_w;
    if (win_x < 0) {
        win_x = 0;
        if (win_w > getScreen()->COLS) {
            menu->width = getScreen()->COLS - 2 * FRAME_WIDTH;
            menu->width -= menu->width % FRAME_WIDTH;
        }
    }
    menu->x = win_x + FRAME_WIDTH;

    win_y = menu->y - mselect - 1;
    win_h = menu->height + 2;
    if (win_y + win_h > getScreen()->ROWS - 1)
        win_y = getScreen()->ROWS - 1 - win_h;
    if (win_y < 0) {
        win_y = 0;
        if (win_y + win_h > getScreen()->ROWS - 1) {
            win_h = getScreen()->ROWS - 1 - win_y;
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
    struct VirtualTerm* vt = getScreen();
    int x, y, w;
    int i, j;

    x = menu->x - FRAME_WIDTH;
    w = menu->width + 2 * FRAME_WIDTH;
    y = menu->y - 1;

    if (menu->offset == 0) {
        G_start;
        vt_mvaddstr(vt, y, x, FRAME[3]);
        for (i = FRAME_WIDTH; i < w - FRAME_WIDTH; i += FRAME_WIDTH)
            vt_mvaddstr(vt, y, x + i, FRAME[10]);
        vt_mvaddstr(vt, y, x + i, FRAME[6]);
        G_end;
    } else {
        G_start;
        vt_mvaddstr(vt, y, x, FRAME[5]);
        G_end;
        for (i = FRAME_WIDTH; i < w - FRAME_WIDTH; i++)
            vt_mvaddstr(vt, y, x + i, " ");
        G_start;
        vt_mvaddstr(vt, y, x + i, FRAME[5]);
        G_end;
        i = (w / 2 - 1) / FRAME_WIDTH * FRAME_WIDTH;
        vt_mvaddstr(vt, y, x + i, ":");
    }

    for (j = 0; j < menu->height; j++) {
        y++;
        G_start;
        vt_mvaddstr(vt, y, x, FRAME[5]);
        G_end;
        draw_menu_item(menu, menu->offset + j);
        G_start;
        vt_mvaddstr(vt, y, x + w - FRAME_WIDTH, FRAME[5]);
        G_end;
    }
    y++;
    if (menu->offset + menu->height == menu->nitem) {
        G_start;
        vt_mvaddstr(vt, y, x, FRAME[9]);
        for (i = FRAME_WIDTH; i < w - FRAME_WIDTH; i += FRAME_WIDTH)
            vt_mvaddstr(vt, y, x + i, FRAME[10]);
        vt_mvaddstr(vt, y, x + i, FRAME[12]);
        G_end;
    } else {
        G_start;
        vt_mvaddstr(vt, y, x, FRAME[5]);
        G_end;
        for (i = FRAME_WIDTH; i < w - FRAME_WIDTH; i++)
            vt_mvaddstr(vt, y, x + i, " ");
        G_start;
        vt_mvaddstr(vt, y, x + i, FRAME[5]);
        G_end;
        i = (w / 2 - 1) / FRAME_WIDTH * FRAME_WIDTH;
        vt_mvaddstr(vt, y, x + i, ":");
    }
}

void draw_menu_item(struct Menu* menu, int mselect)
{
    struct VirtualTerm* vt = getScreen();
    vt_mvaddnstr(vt, menu->y + mselect - menu->offset, menu->x,
        menu->item[mselect].label, menu->width);
}

int select_menu(struct Menu* menu, int mselect)
{
    struct VirtualTerm* vt = getScreen();
    if (mselect < 0 || mselect >= menu->nitem)
        return (MENU_NOTHING);
    if (mselect < menu->offset)
        up_menu(menu, menu->offset - mselect);
    else if (mselect >= menu->offset + menu->height)
        down_menu(menu, mselect - menu->offset - menu->height + 1);

    if (menu->select >= menu->offset && menu->select < menu->offset + menu->height)
        draw_menu_item(menu, menu->select);
    menu->select = mselect;
    vt_standout(vt);
    draw_menu_item(menu, menu->select);
    vt_standend(vt);
    /*
     * move(menu->cursorY, menu->cursorX); */
    vt_move(vt, menu->y + mselect - menu->offset, menu->x);
    vt_toggle_stand(vt);
    // refresh(ttyWriter());
    renderFrame(getUI());

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

int action_menu(struct UI ui, struct Menu* menu)
{
    char c;
    int mselect;
    struct MenuItem item;

    if (menu->active == 0) {
        if (menu->parent != NULL)
            menu->parent->active = 0;
        return (0);
    }
    draw_all_menu(menu);
    select_menu(menu, menu->select);

    GetChFunc getch = event_begin_input(-1);
    while (1) {
        c = getch();
        if (IS_ASCII(c)) { /* Ascii */
            mselect = (*menu->keymap[(int)c])(ui, c);
            if (mselect != MENU_NOTHING)
                break;
        }
    }
    event_end_input(getch);

    if (mselect >= 0 && mselect < menu->nitem) {
        item = menu->item[mselect];
        if (item.type & MENU_POPUP) {
            popup_menu(ui, menu, item.popup);
            return (1);
        }
        if (menu->parent != NULL)
            menu->parent->active = 0;
        if (item.type & MENU_VALUE)
            *item.variable = item.value;
        if (item.type & MENU_FUNC) {
            CurrentKey = -1;
            CurrentKeyData = NULL;
            CurrentCmdData = item.data;
            (*item.func)(getUI());
            CurrentCmdData = NULL;
        }
    } else if (mselect == MENU_CLOSE) {
        if (menu->parent != NULL)
            menu->parent->active = 0;
    }
    return (0);
}

void popup_menu(struct UI ui, struct Menu* parent, struct Menu* menu)
{
    int active = 1;

    if (menu->item == NULL || menu->nitem == 0)
        return;
    if (menu->active)
        return;

    menu->parent = parent;
    menu->select = menu->initial;
    menu->offset = 0;
    menu->active = 1;
    if (parent != NULL) {
        // menu->cursorX = parent->cursorX;
        // menu->cursorY = parent->cursorY;
        guess_menu_xy(parent, menu->width, &menu->x, &menu->y);
    }
    geom_menu(menu, menu->x, menu->y, menu->select);

    CurrentMenu = menu;
    while (active) {
        active = action_menu(ui, CurrentMenu);
    }
    menu->active = 0;
    CurrentMenu = parent;
}

void guess_menu_xy(struct Menu* parent, int width, int* x, int* y)
{
    *x = parent->x + parent->width + FRAME_WIDTH - 1;
    if (*x + width + FRAME_WIDTH > getScreen()->COLS) {
        *x = getScreen()->COLS - width - FRAME_WIDTH;
        if ((parent->x + parent->width / 2 > *x) && (parent->x + parent->width / 2 > getScreen()->COLS / 2))
            *x = parent->x - width - FRAME_WIDTH + 1;
    }
    *y = parent->y + parent->select - parent->offset;
}

void new_option_menu(struct Menu* menu, const char** label, int* variable, CommandFunc func)
{
    int i, nitem;
    struct MenuItem* item;

    if (label == NULL || *label == NULL)
        return;

    const char** p;
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

void set_menu_frame(void)
{
    struct TermEntry* t = getTermEntry();
    if (graph_ok(t)) {
        graph_mode = true;
        FRAME_WIDTH = 1;
        FRAME = graph_symbol;
    } else {
        graph_mode = false;
        FRAME_WIDTH = 0;
        FRAME = get_symbol(DisplayCharset, &FRAME_WIDTH);
        if (!WcOption.use_wide)
            FRAME_WIDTH = 1;
    }
}

/* --- MenuFunctions --- */

static int
mNull(struct UI ui, char c)
{
    return (MENU_NOTHING);
}

static int
mSelect(struct UI ui, char c)
{
    if (IS_ASCII(c))
        return (select_menu(CurrentMenu, CurrentMenu->keyselect[(int)c]));
    else
        return (MENU_NOTHING);
}

static int
mDown(struct UI ui, char c)
{
    if (CurrentMenu->select >= CurrentMenu->nitem - 1)
        return (MENU_NOTHING);
    goto_menu(CurrentMenu, CurrentMenu->select + 1, 1);
    return (MENU_NOTHING);
}

static int
mUp(struct UI ui, char c)
{
    if (CurrentMenu->select <= 0)
        return (MENU_NOTHING);
    goto_menu(CurrentMenu, CurrentMenu->select - 1, -1);
    return (MENU_NOTHING);
}

static int
mLast(struct UI ui, char c)
{
    goto_menu(CurrentMenu, CurrentMenu->nitem - 1, -1);
    return (MENU_NOTHING);
}

static int
mTop(struct UI ui, char c)
{
    goto_menu(CurrentMenu, 0, 1);
    return (MENU_NOTHING);
}

static int
mNext(struct UI ui, char c)
{
    int mselect = CurrentMenu->select + CurrentMenu->height;

    if (mselect >= CurrentMenu->nitem)
        return mLast(ui, c);
    down_menu(CurrentMenu, CurrentMenu->height);
    goto_menu(CurrentMenu, mselect, -1);
    return (MENU_NOTHING);
}

static int
mPrev(struct UI ui, char c)
{
    int mselect = CurrentMenu->select - CurrentMenu->height;

    if (mselect < 0)
        return mTop(ui, c);
    up_menu(CurrentMenu, CurrentMenu->height);
    goto_menu(CurrentMenu, mselect, 1);
    return (MENU_NOTHING);
}

static int
mFore(struct UI ui, char c)
{
    if (CurrentMenu->select >= CurrentMenu->nitem - 1)
        return (MENU_NOTHING);
    goto_menu(CurrentMenu, (CurrentMenu->select + CurrentMenu->height - 1),
        (CurrentMenu->height + 1));
    return (MENU_NOTHING);
}

static int
mBack(struct UI ui, char c)
{
    if (CurrentMenu->select <= 0)
        return (MENU_NOTHING);
    goto_menu(CurrentMenu, (CurrentMenu->select - CurrentMenu->height + 1),
        (-1 - CurrentMenu->height));
    return (MENU_NOTHING);
}

static int
mLineU(struct UI ui, char c)
{
    int mselect = CurrentMenu->select;

    if (mselect >= CurrentMenu->nitem)
        return mLast(ui, c);
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

static int
mLineD(struct UI ui, char c)
{
    int mselect = CurrentMenu->select;

    if (mselect <= 0)
        return mTop(ui, c);
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

static int
mOk(struct UI ui, char c)
{
    int mselect = CurrentMenu->select;

    if (CurrentMenu->item[mselect].type == MENU_NOP)
        return (MENU_NOTHING);
    return (mselect);
}

static int
mCancel(struct UI ui, char c)
{
    return (MENU_CANCEL);
}

static int
mClose(struct UI ui, char c)
{
    return (MENU_CLOSE);
}

static const char* SearchString = NULL;

typedef int (*MenuSearchRoutineFunc)(struct Menu*, const char*, int);
MenuSearchRoutineFunc menuSearchRoutine;

static int
menuForwardSearch(struct Menu* menu, const char* str, int from)
{
    int i;
    char* p;
    if ((p = regexCompile(str, IgnoreCase)) != NULL) {
        message(getUI(), MSG_INFO, p);
        return -1;
    }
    if (from < 0)
        from = 0;
    for (i = from; i < menu->nitem; i++)
        if (menu->item[i].type != MENU_NOP && regexMatch(menu->item[i].label, -1, 1) == 1)
            return i;
    return -1;
}

static int
menu_search_forward(struct UI ui, struct Menu* menu, int from)
{
    const char* str = inputStrHist(ui, "Forward: ", NULL, TextHist);
    if (str != NULL && *str == '\0')
        str = SearchString;
    if (str == NULL || *str == '\0')
        return -1;
    SearchString = str;
    str = conv_search_string(ui, str, DisplayCharset);
    menuSearchRoutine = menuForwardSearch;
    int found = menuForwardSearch(menu, str, from + 1);
    if (WrapSearch && found == -1)
        found = menuForwardSearch(menu, str, 0);
    if (found >= 0)
        return found;
    message(getUI(), MSG_INFO, "Not found");
    return -1;
}

static int
mSrchF(struct UI ui, char c)
{
    int mselect;
    mselect = menu_search_forward(ui, CurrentMenu, CurrentMenu->select);
    if (mselect >= 0)
        goto_menu(CurrentMenu, mselect, 1);
    return (MENU_NOTHING);
}

static int
menuBackwardSearch(struct Menu* menu, const char* str, int from)
{
    int i;
    char* p;
    if ((p = regexCompile(str, IgnoreCase)) != NULL) {
        message(getUI(), MSG_INFO, p);
        return -1;
    }
    if (from >= menu->nitem)
        from = menu->nitem - 1;
    for (i = from; i >= 0; i--)
        if (menu->item[i].type != MENU_NOP && regexMatch(menu->item[i].label, -1, 1) == 1)
            return i;
    return -1;
}

static int
menu_search_backward(struct UI ui, struct Menu* menu, int from)
{
    const char* str = inputStrHist(ui, "Backward: ", NULL, TextHist);
    if (str != NULL && *str == '\0')
        str = SearchString;
    if (str == NULL || *str == '\0')
        return -1;
    SearchString = str;
    str = conv_search_string(ui, str, DisplayCharset);
    menuSearchRoutine = menuBackwardSearch;
    int found = menuBackwardSearch(menu, str, from - 1);
    if (WrapSearch && found == -1)
        found = menuBackwardSearch(menu, str, menu->nitem);
    if (found >= 0)
        return found;
    message(getUI(), MSG_INFO, "Not found");
    return -1;
}

static int
mSrchB(struct UI ui, char c)
{
    int mselect;
    mselect = menu_search_backward(ui, CurrentMenu, CurrentMenu->select);
    if (mselect >= 0)
        goto_menu(CurrentMenu, mselect, -1);
    return (MENU_NOTHING);
}

static int
menu_search_next_previous(struct UI ui, struct Menu* menu, int from, int reverse)
{
    static MenuSearchRoutineFunc routine[2] = {
        menuForwardSearch, menuBackwardSearch
    };

    if (menuSearchRoutine == NULL) {
        message(getUI(), MSG_INFO, "No previous regular expression");
        return -1;
    }
    const char* str = conv_search_string(ui, SearchString, DisplayCharset);
    if (reverse != 0)
        reverse = 1;
    if (menuSearchRoutine == menuBackwardSearch)
        reverse ^= 1;
    from += reverse ? -1 : 1;
    int found = (*routine[reverse])(menu, str, from);
    if (WrapSearch && found == -1)
        found = (*routine[reverse])(menu, str, reverse * menu->nitem);
    if (found >= 0)
        return found;
    message(getUI(), MSG_INFO, "Not found");
    return -1;
}

static int
mSrchN(struct UI ui, char c)
{
    int mselect;
    mselect = menu_search_next_previous(ui, CurrentMenu, CurrentMenu->select, 0);
    if (mselect >= 0)
        goto_menu(CurrentMenu, mselect, 1);
    return (MENU_NOTHING);
}

static int
mSrchP(struct UI ui, char c)
{
    int mselect;
    mselect = menu_search_next_previous(ui, CurrentMenu, CurrentMenu->select, 1);
    if (mselect >= 0)
        goto_menu(CurrentMenu, mselect, -1);
    return (MENU_NOTHING);
}

static int
mMouse(struct UI ui, char c)
{
    return (MENU_NOTHING);
}

static int
mSgrMouse(struct UI ui, char c)
{
    return (MENU_NOTHING);
}

/* --- MenuFunctions (END) --- */

/* --- MainMenu --- */

void popupMenu(struct UI ui, struct Menu* menu)
{
    set_menu_frame();

    initSelectMenu(ui);

    // menu->cursorX = ui.current_buffer->cursorX;
    // menu->cursorY = ui.current_buffer->cursorY;
    menu->x = ui.term_cursor.x + FRAME_WIDTH + 1;
    menu->y = ui.term_cursor.y + 2;

    popup_menu(ui, NULL, menu);
}

DEFUN(mainMn, MAIN_MENU MENU, "Pop up menu")
{
    struct Menu* menu = &MainMenu;
    const char* data = searchKeyData();
    if (data != NULL) {
        int n = getMenuN(w3mMenuList, data);
        if (n < 0)
            return;
        menu = w3mMenuList[n].menu;
    }

    popupMenu(ui, menu);
}

/* --- MainMenu (END) --- */

/* --- SelectMenu --- */

DEFUN(selMn, SELECT_MENU, "Pop up buffer-stack menu")
{
    popupMenu(ui, &SelectMenu);
}

static void
initSelectMenu(struct UI ui)
{
    int i, nitem, len = 0, l;
    struct Buffer* buf;
    Str str;
    const char** label;
    static char* comment = " SPC for select / D for delete buffer ";

    SelectV = -1;
    for (i = 0, buf = Firstbuf; buf != NULL; i++, buf = buf->nextBuffer) {
        if (buf == ui.current_buffer)
            SelectV = i;
    }
    nitem = i;

    label = New_N(char*, nitem + 2);
    for (i = 0, buf = Firstbuf; i < nitem; i++, buf = buf->nextBuffer) {
        str = Sprintf("<%s>", buf->document.title);
        if (buf->filename != NULL) {
            switch (buf->content.url.scheme) {
            case SCM_LOCAL:
                // if (strcmp(buf->content.url.file, "-")) {
                //     Strcat_char(str, ' ');
                //     Strcat_charp(str, conv_from_system(buf->content.url.real_file));
                // }
                break;
                /* case SCM_UNKNOWN: */
            case SCM_MISSING:
                break;
            default: {
                Strcat_char(str, ' ');
                const char* p = url_decode2(parsedURL2Str(&buf->content.url)->ptr, 0);
                Strcat_charp(str, p);
                break;
            }
            }
        }
        label[i] = str->ptr;
        if (len < str->length)
            len = str->length;
    }
    l = get_strwidth(comment);
    if (len < l + 4)
        len = l + 4;
    if (len > getScreen()->COLS - 2 * FRAME_WIDTH)
        len = getScreen()->COLS - 2 * FRAME_WIDTH;
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
    // SelectMenu.cursorX = ui.current_buffer->cursorX;
    // SelectMenu.cursorY = ui.current_buffer->cursorY;
    SelectMenu.keymap['D'] = smDelBuf;
    SelectMenu.item[nitem].type = MENU_NOP;
}

static void
smChBuf(struct UI ui)
{
    if (SelectV < 0 || SelectV >= SelectMenu.nitem)
        return;

    int i;
    struct Buffer* buf;
    for (i = 0, buf = Firstbuf; i < SelectV; i++, buf = buf->nextBuffer)
        ;

    setCurrentBuffer(buf);
}

static int
smDelBuf(struct UI ui, char c)
{
    if (CurrentMenu->select < 0 || CurrentMenu->select >= SelectMenu.nitem)
        return (MENU_NOTHING);

    struct Buffer* buf = Firstbuf;
    for (int i = 0; i < CurrentMenu->select; i++, buf = buf->nextBuffer)
        ;

    delBuffer(ui, buf);

    int x = CurrentMenu->x;
    int y = CurrentMenu->y;
    int mselect = CurrentMenu->select;

    initSelectMenu(ui);

    CurrentMenu->x = x;
    CurrentMenu->y = y;

    geom_menu(CurrentMenu, x, y, 0);

    CurrentMenu->select = (mselect <= CurrentMenu->nitem - 2) ? mselect
                                                              : (CurrentMenu->nitem - 2);

    draw_all_menu(CurrentMenu);
    select_menu(CurrentMenu, CurrentMenu->select);
    return (MENU_NOTHING);
}

/* --- SelectMenu (END) --- */

/* --- OptionMenu --- */

void optionMenu(struct UI ui, int x, int y, const char** label, int* variable, int initial, CommandFunc func)
{
    set_menu_frame();

    struct Menu menu;
    new_option_menu(&menu, label, variable, func);
    // menu.cursorX = getScreen()->COLS - 1;
    // menu.cursorY = getScreen()->ROWS - 1;
    menu.x = x;
    menu.y = y;
    menu.initial = initial;

    popup_menu(ui, NULL, &menu);
}

/* --- OptionMenu (END) --- */

/* --- InitMenu --- */

static void
interpret_menu(FILE* mf)
{
    Str line;
    int in_menu = 0, nmenu = 0, nitem = 0, type;
    struct MenuItem* item = NULL;
    wc_ces charset = SystemCharset;

    while (!feof(mf)) {
        line = Strfgets(mf);
        Strchop(line);
        Strremovefirstspaces(line);
        if (line->length == 0)
            continue;
        line = wc_Str_conv(line, charset, InnerCharset);
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
            s = getQWord(&p);
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
    struct MenuList* list;

    w3mMenuList = New_N(struct MenuList, 3);
    w3mMenuList[0].id = "Main";
    w3mMenuList[0].menu = &MainMenu;
    w3mMenuList[0].item = MainMenuItem;
    w3mMenuList[1].id = "Select";
    w3mMenuList[1].menu = &SelectMenu;
    w3mMenuList[1].item = NULL;
    w3mMenuList[2].id = NULL;

    if (!MainMenuEncode) {
        struct MenuItem* item;
        /* FIXME: charset that gettext(3) returns */
        MainMenuCharset = SystemCharset;
        for (item = MainMenuItem; item->type != MENU_END; item++)
            item->label = wc_conv(_(item->label), MainMenuCharset,
                InnerCharset)
                              ->ptr;
        MainMenuEncode = true;
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
        const char* label = getQWord(&line);
        const char* func = getWord(&line);
        const char* keys = getQWord(&line);
        const char* data = getQWord(&line);
        if (*func == '\0') /* error */
            return -1;
        item->type = MENU_FUNC;
        item->label = label;
        item->func = getFunc(func);
        item->keys = keys;
        item->data = data;
        return MENU_FUNC;
    } else if (strcmp(type, "popup") == 0) {
        const char* label = getQWord(&line);
        const char* popup = getQWord(&line);
        const char* keys = getQWord(&line);
        if (*popup == '\0') /* error */
            return -1;
        item->type = MENU_POPUP;
        item->label = label;
        int n;
        if ((n = getMenuN(w3mMenuList, popup)) == -1)
            n = addMenuList(&w3mMenuList, popup);
        item->popup = w3mMenuList[n].menu;
        item->keys = keys;
        return MENU_POPUP;
    }
    return -1; /* error */
}

int addMenuList(struct MenuList** mlist, const char* id)
{
    int n;
    struct MenuList* list = *mlist;

    for (n = 0; list->id != NULL; list++, n++)
        ;
    *mlist = New_Reuse(struct MenuList, *mlist, (n + 2));
    list = *mlist + n;
    list->id = id;
    list->menu = New(struct Menu);
    list->item = New(struct MenuItem);
    (list + 1)->id = NULL;
    return n;
}

int getMenuN(struct MenuList* list, const char* id)
{
    for (int n = 0; list->id != NULL; list++, n++) {
        if (strcmp(id, list->id) == 0)
            return n;
    }
    return -1;
}

/* --- InitMenu (END) --- */

/* --- LinkMenu (END) --- */

struct Anchor*
accesskey_menu(struct UI ui, struct Buffer* buf)
{
    struct AnchorList* al = buf->document.href;
    struct Anchor* a;
    struct Anchor** ap;
    int i, n, nitem = 0, key = -1;
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

    const char** label = New_N(char*, nitem + 1);
    ap = New_N(struct Anchor*, nitem);
    for (i = 0, n = 0; i < al->nanchor; i++) {
        a = &al->anchors[i];
        if (!a->slave && a->accesskey && IS_ASCII(a->accesskey)) {
            const char* t = getAnchorText(buf, al, a);
            label[n] = Sprintf("%c: %s", a->accesskey, t ? t : "")->ptr;
            ap[n] = a;
            n++;
        }
    }
    label[nitem] = NULL;

    set_menu_frame();
    struct Menu menu;
    new_option_menu(&menu, label, &key, NULL);

    menu.initial = 0;
    // menu.cursorX = buf->cursorX;
    // menu.cursorY = buf->cursorY;
    menu.x = /*menu.cursorX +*/ FRAME_WIDTH + 1;
    menu.y = /*menu.cursorY +*/ 2;
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

    popup_menu(ui, NULL, &menu);

    return (key >= 0) ? ap[key] : NULL;
}

static char lmKeys[] = "abcdefgimopqrstuvwxyz";
static char lmKeys2[] = "1234567890ABCDEFGHILMOPQRSTUVWXYZ";
#define nlmKeys (sizeof(lmKeys) - 1)
#define nlmKeys2 (sizeof(lmKeys2) - 1)

static int
lmGoto(struct UI ui, char c)
{
    if (IS_ASCII(c) && CurrentMenu->keyselect[(int)c] >= 0) {
        goto_menu(CurrentMenu, CurrentMenu->nitem - 1, -1);
        goto_menu(CurrentMenu, CurrentMenu->keyselect[(int)c] * nlmKeys, 1);
    }
    return (MENU_NOTHING);
}

static int
lmSelect(struct UI ui, char c)
{
    if (IS_ASCII(c))
        return select_menu(CurrentMenu, (CurrentMenu->select / nlmKeys) * nlmKeys + CurrentMenu->keyselect[(int)c]);
    else
        return (MENU_NOTHING);
}

struct Anchor*
list_menu(struct UI ui, struct Buffer* buf)
{
    struct Menu menu;
    struct AnchorList* al = buf->document.href;
    struct Anchor* a;
    struct Anchor** ap;
    int i, n, nitem = 0, key = -1, two = false;
    const char* t;
    unsigned char c;

    if (!al)
        return NULL;
    for (i = 0; i < al->nanchor; i++) {
        a = &al->anchors[i];
        if (!a->slave)
            nitem++;
    }
    if (!nitem)
        return NULL;

    if (nitem >= nlmKeys)
        two = true;

    char** label;
    label = New_N(char*, nitem + 1);
    ap = New_N(struct Anchor*, nitem);
    for (i = 0, n = 0; i < al->nanchor; i++) {
        a = &al->anchors[i];
        if (!a->slave) {
            t = getAnchorText(buf, al, a);
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
    new_option_menu(&menu, (const char**)label, &key, NULL);

    menu.initial = 0;
    // menu.cursorX = buf->cursorX;
    // menu.cursorY = buf->cursorY;
    menu.x = /*menu.cursorX +*/ FRAME_WIDTH + 1;
    menu.y = /*menu.cursorY +*/ 2;
    for (i = 0; i < 128; i++)
        menu.keyselect[i] = -1;
    if (two) {
        for (i = 0; i < nlmKeys2; i++) {
            c = lmKeys2[i];
            menu.keymap[(int)c] = lmGoto;
            menu.keyselect[(int)c] = i;
        }
        for (i = 0; i < nlmKeys; i++) {
            c = lmKeys[i];
            menu.keymap[(int)c] = lmSelect;
            menu.keyselect[(int)c] = i;
        }
    } else {
        for (i = 0; i < nitem; i++) {
            c = lmKeys[i];
            menu.keymap[(int)c] = mSelect;
            menu.keyselect[(int)c] = i;
        }
    }

    a = retrieveCurrentAnchor(buf);
    if (a) {
        for (i = 0; i < nitem; i++) {
            if (a->hseq == ap[i]->hseq) {
                menu.initial = i;
                break;
            }
        }
    }

    popup_menu(ui, NULL, &menu);

    return (key >= 0) ? ap[key] : NULL;
}
