#include "global.h"
#include "term_tty.h"
#include "terms.h"
#include "alloc.h"
#include "anchor.h"
#include "search.h"
#include "wc_util.h"
#include "rc.h"
#include "buffer.h"
#include "url.h"
#include "display.h"
#include "symbol.h"
#include "defun_impl.h"
#include "history.h"
#include "line_input.h"
#include "menu.h"
#include "func.h"
#include "myctype.h"
#include "regex.h"

#include <libwc/charset.h>

#include <stdio.h>

#define MENU_FILE "menu"

static char** FRAME;
static int FRAME_WIDTH;
static int graph_mode = false;
#define G_start           \
    {                     \
        if (graph_mode)   \
            graphstart(); \
    }
#define G_end           \
    {                   \
        if (graph_mode) \
            graphend(); \
    }

static int mEsc(struct CmdArgs *args);
static int mEscB(struct CmdArgs *args);
static int mEscD(struct CmdArgs *args);
static int mNull(struct CmdArgs *args);
static int mSelect(struct CmdArgs *args);
static int mDown(struct CmdArgs *args);
static int mUp(struct CmdArgs *args);
static int mLast(struct CmdArgs *args);
static int mTop(struct CmdArgs *args);
static int mNext(struct CmdArgs *args);
static int mPrev(struct CmdArgs *args);
static int mFore(struct CmdArgs *args);
static int mBack(struct CmdArgs *args);
static int mLineU(struct CmdArgs *args);
static int mLineD(struct CmdArgs *args);
static int mOk(struct CmdArgs *args);
static int mCancel(struct CmdArgs *args);
static int mClose(struct CmdArgs *args);
static int mSusp(struct CmdArgs *args);
static int mMouse(struct CmdArgs *args);
static int mSgrMouse(struct CmdArgs *args);
static int mSrchF(struct CmdArgs *args);
static int mSrchB(struct CmdArgs *args);
static int mSrchN(struct CmdArgs *args);
static int mSrchP(struct CmdArgs *args);

// clang-format off
static MenuFunc MenuKeymap[128] = {
    /*  C-@     C-a     C-b     C-c     C-d     C-e     C-f     C-g      */
    mNull, mTop, mPrev, mClose, mNull, mLast, mNext, mNull,
    /*  C-h     C-i     C-j     C-k     C-l     C-m     C-n     C-o      */
    mCancel, mNull, mOk, mNull, mNull, mOk, mDown, mNull,
    /*  C-p     C-q     C-r     C-s     C-t     C-u     C-v     C-w      */
    mUp, mNull, mSrchB, mSrchF, mNull, mNull, mNext, mNull,
    /*  C-x     C-y     C-z     C-[     C-\     C-]     C-^     C-_      */
    mNull, mNull, mSusp, mEsc, mNull, mNull, mNull, mNull,
    /*  SPC     !       "       #       $       %       &       '        */
    mOk, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    /*  (       )       *       +       ,       -       .       /        */
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mSrchF,
    /*  0       1       2       3       4       5       6       7        */
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    /*  8       9       :       ;       <       =       >       ?        */
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mSrchB,
    /*  @       A       B       C       D       E       F       G        */
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    /*  H       I       J       K       L       M       N       O        */
    mNull, mNull, mLineU, mLineD, mNull, mNull, mSrchP, mNull,
    /*  P       Q       R       S       T       U       V       W        */
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    /*  X       Y       Z       [       \       ]       ^       _        */
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    /*  `       a       b       c       d       e       f       g        */
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    /*  h       i       j       k       l       m       n       o        */
    mCancel, mNull, mDown, mUp, mOk, mNull, mSrchN, mNull,
    /*  p       q       r       s       t       u       v       w        */
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    /*  x       y       z       {       |       }       ~       DEL      */
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mCancel,
};

static MenuFunc MenuEscKeymap[128] = {
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,

    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,

    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    /*                                                          O     */
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mEscB,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    /*                          [                                     */
    mNull, mNull, mNull, mEscB, mNull, mNull, mNull, mNull,

    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    /*                                                  v             */
    mNull, mNull, mNull, mNull, mNull, mNull, mPrev, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
};

static MenuFunc MenuEscBKeymap[128] = {
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,

    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    /*  8       9       :       ;       <       =       >       ?     */
    mNull, mNull, mNull, mNull, mSgrMouse, mNull, mNull, mNull,
    /*          A       B       C       D       E                     */
    mNull, mUp, mDown, mOk, mCancel, mClose, mNull, mNull,
    /*                                  L       M                     */
    mNull, mNull, mNull, mNull, mClose, mMouse, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,

    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
};

static MenuFunc MenuEscDKeymap[128] = {
    /*  0       1       INS     3       4       PgUp,   PgDn    7     */
    mNull, mNull, mClose, mNull, mNull, mBack, mFore, mNull,
    /*  8       9       10      F1      F2      F3      F4      F5       */
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    /*  16      F6      F7      F8      F9      F10     22      23       */
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    /*  24      25      26      27      HELP    29      30      31       */
    mNull, mNull, mNull, mNull, mClose, mNull, mNull, mNull,

    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,

    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,

    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
};
// clang-format on

/* --- SelectMenu --- */

Menu SelectMenu;
static int SelectV = 0;
static void initSelectMenu(void);
static void smChBuf(void);
static int smDelBuf(struct CmdArgs *args);

/* --- SelectMenu (END) --- */

/* --- SelTabMenu --- */

Menu SelTabMenu;
static int SelTabV = 0;
static void initSelTabMenu(void);
static void smChTab(void);
static int smDelTab(struct CmdArgs *args);

/* --- SelTabMenu (END) --- */

/* --- MainMenu --- */

Menu MainMenu;
/* FIXME: gettextize here */
static wc_ces MainMenuCharset = WC_CES_US_ASCII; /* FIXME: charset of source code */
static int MainMenuEncode = false;

static MenuItem MainMenuItem[] = {
    /* type        label           variable value func     popup keys data  */
    { MENU_FUNC, " Back         (b) ", NULL, 0, "BACK", NULL, "b", NULL },
    { MENU_POPUP, " Select struct Buffer(s) ", NULL, 0, NULL, &SelectMenu, "s",
        NULL },
    { MENU_POPUP, " Select Tab   (t) ", NULL, 0, NULL, &SelTabMenu, "tT",
        NULL },
    { MENU_FUNC, " View Source  (v) ", NULL, 0, "SOURCE", NULL, "vV", NULL },
    { MENU_FUNC, " Edit Source  (e) ", NULL, 0, "EDIT", NULL, "eE", NULL },
    { MENU_FUNC, " Save Source  (S) ", NULL, 0, "SOURCE", NULL, "S", NULL },
    { MENU_FUNC, " Reload       (r) ", NULL, 0, "RELOAD", NULL, "rR", NULL },
    { MENU_NOP, " ---------------- ", NULL, 0, "NOTHING", NULL, "", NULL },
    { MENU_FUNC, " Go Link      (a) ", NULL, 0, "GOTO_LINK", NULL, "a", NULL },
    { MENU_FUNC, "   on New Tab (n) ", NULL, 0, "TAB_LINK", NULL, "nN", NULL },
    { MENU_FUNC, " Save Link    (A) ", NULL, 0, "SAVE_LINK", NULL, "A", NULL },
    { MENU_FUNC, " View Image   (i) ", NULL, 0, "VIEW_IMAGE", NULL, "i", NULL },
    { MENU_FUNC, " Save Image   (I) ", NULL, 0, "SAVE_IMAGE", NULL, "I", NULL },
    { MENU_FUNC, " View Frame   (f) ", NULL, 0, "FRAME", NULL, "fF", NULL },
    { MENU_NOP, " ---------------- ", NULL, 0, "NOTHING", NULL, "", NULL },
    { MENU_FUNC, " Bookmark     (B) ", NULL, 0, "BOOKMARK", NULL, "B", NULL },
    { MENU_FUNC, " Help         (h) ", NULL, 0, "HELP", NULL, "hH", NULL },
    { MENU_FUNC, " Option       (o) ", NULL, 0, "OPTIONS", NULL, "oO", NULL },
    { MENU_NOP, " ---------------- ", NULL, 0, "NOTHING", NULL, "", NULL },
    { MENU_FUNC, " Quit         (q) ", NULL, 0, "QUIT", NULL, "qQ", NULL },
    { MENU_END, "", NULL, 0, "NOTHING", NULL, "", NULL },
};

/* --- MainMenu (END) --- */

MenuList* w3mMenuList;

static Menu* CurrentMenu = NULL;

#define mvaddch(y, x, c) (move(y, x), addch(c))
#define mvaddstr(y, x, str) (move(y, x), addstr(str))
#define mvaddnstr(y, x, str, n) (move(y, x), addnstr_sup(str, n))

void new_menu(Menu* menu, MenuItem* item)
{
    int i, l;
    char* p;

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
        if ((p = item[i].keys) != NULL) {
            while (*p) {
                if (IS_ASCII(*p)) {
                    menu->keymap[(int)*p] = mSelect;
                    menu->keyselect[(int)*p] = i;
                }
                p++;
            }
        }
        l = get_strwidth(WcOption, item[i].label);
        if (l > menu->width)
            menu->width = l;
    }
}

void geom_menu(Menu* menu, int x, int y, int mselect)
{
    int win_x, win_y, win_w, win_h;

    menu->select = mselect;

    if (menu->width % FRAME_WIDTH)
        menu->width = (menu->width / FRAME_WIDTH + 1) * FRAME_WIDTH;
    win_x = menu->x - FRAME_WIDTH;
    win_w = menu->width + 2 * FRAME_WIDTH;
    if (win_x + win_w > COLS)
        win_x = COLS - win_w;
    if (win_x < 0) {
        win_x = 0;
        if (win_w > COLS) {
            menu->width = COLS - 2 * FRAME_WIDTH;
            menu->width -= menu->width % FRAME_WIDTH;
        }
    }
    menu->x = win_x + FRAME_WIDTH;

    win_y = menu->y - mselect - 1;
    win_h = menu->height + 2;
    if (win_y + win_h > (LINES - 1))
        win_y = (LINES - 1) - win_h;
    if (win_y < 0) {
        win_y = 0;
        if (win_y + win_h > (LINES - 1)) {
            win_h = (LINES - 1) - win_y;
            menu->height = win_h - 2;
            if (menu->height <= mselect)
                menu->offset = mselect - menu->height + 1;
        }
    }
    menu->y = win_y + 1;
}

void draw_all_menu(Menu* menu)
{
    if (menu->parent != NULL)
        draw_all_menu(menu->parent);
    draw_menu(menu);
}

void draw_menu(Menu* menu)
{
    int x, y, w;
    int i, j;

    x = menu->x - FRAME_WIDTH;
    w = menu->width + 2 * FRAME_WIDTH;
    y = menu->y - 1;

    if (menu->offset == 0) {
        G_start;
        mvaddstr(y, x, FRAME[3]);
        for (i = FRAME_WIDTH; i < w - FRAME_WIDTH; i += FRAME_WIDTH)
            mvaddstr(y, x + i, FRAME[10]);
        mvaddstr(y, x + i, FRAME[6]);
        G_end;
    } else {
        G_start;
        mvaddstr(y, x, FRAME[5]);
        G_end;
        for (i = FRAME_WIDTH; i < w - FRAME_WIDTH; i++)
            mvaddstr(y, x + i, " ");
        G_start;
        mvaddstr(y, x + i, FRAME[5]);
        G_end;
        i = (w / 2 - 1) / FRAME_WIDTH * FRAME_WIDTH;
        mvaddstr(y, x + i, ":");
    }

    for (j = 0; j < menu->height; j++) {
        y++;
        G_start;
        mvaddstr(y, x, FRAME[5]);
        G_end;
        draw_menu_item(menu, menu->offset + j);
        G_start;
        mvaddstr(y, x + w - FRAME_WIDTH, FRAME[5]);
        G_end;
    }
    y++;
    if (menu->offset + menu->height == menu->nitem) {
        G_start;
        mvaddstr(y, x, FRAME[9]);
        for (i = FRAME_WIDTH; i < w - FRAME_WIDTH; i += FRAME_WIDTH)
            mvaddstr(y, x + i, FRAME[10]);
        mvaddstr(y, x + i, FRAME[12]);
        G_end;
    } else {
        G_start;
        mvaddstr(y, x, FRAME[5]);
        G_end;
        for (i = FRAME_WIDTH; i < w - FRAME_WIDTH; i++)
            mvaddstr(y, x + i, " ");
        G_start;
        mvaddstr(y, x + i, FRAME[5]);
        G_end;
        i = (w / 2 - 1) / FRAME_WIDTH * FRAME_WIDTH;
        mvaddstr(y, x + i, ":");
    }
}

void draw_menu_item(Menu* menu, int mselect)
{
    mvaddnstr(menu->y + mselect - menu->offset, menu->x,
        menu->item[mselect].label, menu->width);
}

int select_menu(Menu* menu, int mselect)
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
    standout();
    draw_menu_item(menu, menu->select);
    standend();
    /*
     * move(menu->cursorY, menu->cursorX); */
    move(menu->y + mselect - menu->offset, menu->x);
    toggle_stand();
    refresh();

    return (menu->select);
}

void goto_menu(Menu* menu, int mselect, int down)
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

void up_menu(Menu* menu, int n)
{
    if (n < 0 || menu->offset == 0)
        return;
    menu->offset -= n;
    if (menu->offset < 0)
        menu->offset = 0;

    draw_menu(menu);
}

void down_menu(Menu* menu, int n)
{
    if (n < 0 || menu->offset + menu->height == menu->nitem)
        return;
    menu->offset += n;
    if (menu->offset + menu->height > menu->nitem)
        menu->offset = menu->nitem - menu->height;

    draw_menu(menu);
}

int action_menu(struct CmdArgs *args, Menu* menu)
{
    int mselect;
    MenuItem item;

    if (menu->active == 0) {
        if (menu->parent != NULL)
            menu->parent->active = 0;
        return (0);
    }
    draw_all_menu(menu);
    select_menu(menu, menu->select);

    while (1) {
        args->ch = getch(args);
        if (IS_ASCII(args->ch)) { /* Ascii */
            mselect = (*menu->keymap[args->ch])(args);
            if (mselect != MENU_NOTHING)
                break;
        }
    }
    if (mselect >= 0 && mselect < menu->nitem) {
        item = menu->item[mselect];
        if (item.type & MENU_POPUP) {
            popup_menu(args, menu, item.popup);
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
            w3mFunc(item.cmd);
            CurrentCmdData = NULL;
        }
    } else if (mselect == MENU_CLOSE) {
        if (menu->parent != NULL)
            menu->parent->active = 0;
    }
    return (0);
}

void popup_menu(struct CmdArgs *args, Menu* parent, Menu* menu)
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
        menu->cursorX = parent->cursorX;
        menu->cursorY = parent->cursorY;
        guess_menu_xy(parent, menu->width, &menu->x, &menu->y);
    }
    geom_menu(menu, menu->x, menu->y, menu->select);

    CurrentMenu = menu;
    while (active) {
        active = action_menu(args, CurrentMenu);
        displayBuffer(args, B_FORCE_REDRAW);
    }
    menu->active = 0;
    CurrentMenu = parent;
}

void guess_menu_xy(Menu* parent, int width, int* x, int* y)
{
    *x = parent->x + parent->width + FRAME_WIDTH - 1;
    if (*x + width + FRAME_WIDTH > COLS) {
        *x = COLS - width - FRAME_WIDTH;
        if ((parent->x + parent->width / 2 > *x) && (parent->x + parent->width / 2 > COLS / 2))
            *x = parent->x - width - FRAME_WIDTH + 1;
    }
    *y = parent->y + parent->select - parent->offset;
}

void new_option_menu(Menu* menu, const char** label, int* variable, const char* cmd)
{
    int i, nitem;
    MenuItem* item;

    if (label == NULL || *label == NULL)
        return;

    const char** p = label;
    for (i = 0; *p != NULL; i++, p++)
        ;
    nitem = i;

    item = New_N(MenuItem, nitem + 1);

    for (i = 0, p = label; i < nitem; i++, p++) {
        if (cmd != NULL)
            item[i].type = MENU_VALUE | MENU_FUNC;
        else
            item[i].type = MENU_VALUE;
        item[i].label = *p;
        item[i].variable = variable;
        item[i].value = i;
        item[i].cmd = cmd;
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

struct LinkList* link_menu(struct CmdArgs *args, struct Buffer* buf)
{
    Menu menu;
    struct LinkList* l;
    int i, nitem, len = 0, linkV = -1;
    const char** label;
    Str str;
    char* p;

    if (!buf->linklist)
        return NULL;

    for (i = 0, l = buf->linklist; l; i++, l = l->next)
        ;
    nitem = i;

    label = New_N(char*, nitem + 1);
    for (i = 0, l = buf->linklist; l; i++, l = l->next) {
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
    menu.cursorX = buf->cursorX + buf->rootX;
    menu.cursorY = buf->cursorY + buf->rootY;
    menu.x = menu.cursorX + FRAME_WIDTH + 1;
    menu.y = menu.cursorY + 2;

    popup_menu(args, NULL, &menu);

    if (linkV < 0)
        return NULL;
    for (i = 0, l = buf->linklist; l; i++, l = l->next) {
        if (i == linkV)
            return l;
    }
    return NULL;
}

/* --- MenuFunctions --- */

static int
mEsc(struct CmdArgs *args)
{
    args->ch = getch(args);
    return (MenuEscKeymap[args->ch](args));
}

static int
mEscB(struct CmdArgs *args)
{
    args->ch = getch(args);
    if (IS_DIGIT(args->ch))
        return (mEscD(args));
    else
        return (MenuEscBKeymap[args->ch](args));
}

static int
mEscD(struct CmdArgs *args)
{
    int d = args->ch - (int)'0';
    args->ch = getch(args);
    if (IS_DIGIT(args->ch)) {
        d = d * 10 + args->ch - (int)'0';
        args->ch = getch(args);
    }
    if (args->ch == '~')
        return (MenuEscDKeymap[d](args));
    else
        return (MENU_NOTHING);
}

static int
mNull(struct CmdArgs *args)
{
    return (MENU_NOTHING);
}

static int
mSelect(struct CmdArgs *args)
{
    if (IS_ASCII(args->ch))
        return (select_menu(CurrentMenu, CurrentMenu->keyselect[args->ch]));
    else
        return (MENU_NOTHING);
}

static int
mDown(struct CmdArgs *args)
{
    if (CurrentMenu->select >= CurrentMenu->nitem - 1)
        return (MENU_NOTHING);
    goto_menu(CurrentMenu, CurrentMenu->select + 1, 1);
    return (MENU_NOTHING);
}

static int
mUp(struct CmdArgs *args)
{
    if (CurrentMenu->select <= 0)
        return (MENU_NOTHING);
    goto_menu(CurrentMenu, CurrentMenu->select - 1, -1);
    return (MENU_NOTHING);
}

static int
mLast(struct CmdArgs *args)
{
    goto_menu(CurrentMenu, CurrentMenu->nitem - 1, -1);
    return (MENU_NOTHING);
}

static int
mTop(struct CmdArgs *args)
{
    goto_menu(CurrentMenu, 0, 1);
    return (MENU_NOTHING);
}

static int
mNext(struct CmdArgs *args)
{
    int mselect = CurrentMenu->select + CurrentMenu->height;

    if (mselect >= CurrentMenu->nitem)
        return mLast(args);
    down_menu(CurrentMenu, CurrentMenu->height);
    goto_menu(CurrentMenu, mselect, -1);
    return (MENU_NOTHING);
}

static int
mPrev(struct CmdArgs *args)
{
    int mselect = CurrentMenu->select - CurrentMenu->height;

    if (mselect < 0)
        return mTop(args);
    up_menu(CurrentMenu, CurrentMenu->height);
    goto_menu(CurrentMenu, mselect, 1);
    return (MENU_NOTHING);
}

static int
mFore(struct CmdArgs *args)
{
    if (CurrentMenu->select >= CurrentMenu->nitem - 1)
        return (MENU_NOTHING);
    goto_menu(CurrentMenu, (CurrentMenu->select + CurrentMenu->height - 1),
        (CurrentMenu->height + 1));
    return (MENU_NOTHING);
}

static int
mBack(struct CmdArgs *args)
{
    if (CurrentMenu->select <= 0)
        return (MENU_NOTHING);
    goto_menu(CurrentMenu, (CurrentMenu->select - CurrentMenu->height + 1),
        (-1 - CurrentMenu->height));
    return (MENU_NOTHING);
}

static int
mLineU(struct CmdArgs *args)
{
    int mselect = CurrentMenu->select;

    if (mselect >= CurrentMenu->nitem)
        return mLast(args);
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
mLineD(struct CmdArgs *args)
{
    int mselect = CurrentMenu->select;

    if (mselect <= 0)
        return mTop(args);
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
mOk(struct CmdArgs *args)
{
    int mselect = CurrentMenu->select;

    if (CurrentMenu->item[mselect].type == MENU_NOP)
        return (MENU_NOTHING);
    return (mselect);
}

static int
mCancel(struct CmdArgs *args)
{
    return (MENU_CANCEL);
}

static int
mClose(struct CmdArgs *args)
{
    return (MENU_CLOSE);
}

static int
mSusp(struct CmdArgs *args)
{
    susp(args);
    draw_all_menu(CurrentMenu);
    select_menu(CurrentMenu, CurrentMenu->select);
    return (MENU_NOTHING);
}

int (*menuSearchRoutine)(Menu*, const char*, int);

static int
menuForwardSearch(Menu* menu, const char* str, int from)
{
    int i;
    const char* p;
    if ((p = regexCompile(str, IgnoreCase)) != NULL) {
        message(p, 0, 0);
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
menu_search_forward(struct CmdArgs *args, Menu* menu, int from)
{
    const char* str = inputStrHist(args, "Forward: ", NULL, HistoryText);
    if (str != NULL && *str == '\0')
        str = SearchString;
    if (str == NULL || *str == '\0')
        return -1;
    SearchString = str;
    str = conv_search_string(str, DisplayCharset);
    menuSearchRoutine = menuForwardSearch;
    int found = menuForwardSearch(menu, str, from + 1);
    if (WrapSearch && found == -1)
        found = menuForwardSearch(menu, str, 0);
    if (found >= 0)
        return found;
    disp_message(args, "Not found", true);
    return -1;
}

static int
mSrchF(struct CmdArgs *args)
{
    int mselect = menu_search_forward(args, CurrentMenu, CurrentMenu->select);
    if (mselect >= 0)
        goto_menu(CurrentMenu, mselect, 1);
    return (MENU_NOTHING);
}

static int
menuBackwardSearch(Menu* menu, const char* str, int from)
{
    int i;
    const char* p;
    if ((p = regexCompile(str, IgnoreCase)) != NULL) {
        message(p, 0, 0);
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
menu_search_backward(struct CmdArgs *args, Menu* menu, int from)
{
    const char* str = inputStrHist(args, "Backward: ", NULL, HistoryText);
    if (str != NULL && *str == '\0')
        str = SearchString;
    if (str == NULL || *str == '\0')
        return -1;
    SearchString = str;
    str = conv_search_string(str, DisplayCharset);
    menuSearchRoutine = menuBackwardSearch;
    int found = menuBackwardSearch(menu, str, from - 1);
    if (WrapSearch && found == -1)
        found = menuBackwardSearch(menu, str, menu->nitem);
    if (found >= 0)
        return found;
    disp_message(args, "Not found", true);
    return -1;
}

static int
mSrchB(struct CmdArgs *args)
{
    int mselect = menu_search_backward(args, CurrentMenu, CurrentMenu->select);
    if (mselect >= 0)
        goto_menu(CurrentMenu, mselect, -1);
    return (MENU_NOTHING);
}

static int
menu_search_next_previous(struct CmdArgs *args, Menu* menu, int from, int reverse)
{
    int found;
    static int (*routine[2])(Menu*, const char*, int) = {
        menuForwardSearch, menuBackwardSearch
    };

    if (menuSearchRoutine == NULL) {
        disp_message(args, "No previous regular expression", true);
        return -1;
    }

    const char* str = conv_search_string(SearchString, DisplayCharset);
    if (reverse != 0)
        reverse = 1;
    if (menuSearchRoutine == menuBackwardSearch)
        reverse ^= 1;
    from += reverse ? -1 : 1;
    found = (*routine[reverse])(menu, str, from);
    if (WrapSearch && found == -1)
        found = (*routine[reverse])(menu, str, reverse * menu->nitem);
    if (found >= 0)
        return found;
    disp_message(args, "Not found", true);
    return -1;
}

static int
mSrchN(struct CmdArgs *args)
{
    int mselect;
    mselect = menu_search_next_previous(args, CurrentMenu, CurrentMenu->select, 0);
    if (mselect >= 0)
        goto_menu(CurrentMenu, mselect, 1);
    return (MENU_NOTHING);
}

static int
mSrchP(struct CmdArgs *args)
{
    int mselect = menu_search_next_previous(args, CurrentMenu, CurrentMenu->select, 1);
    if (mselect >= 0)
        goto_menu(CurrentMenu, mselect, -1);
    return (MENU_NOTHING);
}

static int
mMouse(struct CmdArgs *args)
{
    return (MENU_NOTHING);
}

static int
mSgrMouse(struct CmdArgs *args)
{
    return (MENU_NOTHING);
}

/* --- MenuFunctions (END) --- */

/* --- MainMenu --- */

void popupMenu(struct CmdArgs *args, int x, int y, Menu* menu)
{
    set_menu_frame();

    initSelectMenu();
    initSelTabMenu();

    menu->cursorX = Currentbuf->cursorX + Currentbuf->rootX;
    menu->cursorY = Currentbuf->cursorY + Currentbuf->rootY;
    menu->x = x + FRAME_WIDTH + 1;
    menu->y = y + 2;

    popup_menu(args, NULL, menu);
}

void mainMenu(struct CmdArgs *args, int x, int y)
{
    popupMenu(args, x, y, &MainMenu);
}

/* --- MainMenu (END) --- */

/* --- SelectMenu --- */

static void
initSelectMenu(void)
{
    int i, nitem, len = 0, l;
    struct Buffer* buf;
    Str str;
    char** label;
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
        str = Sprintf("<%s>", buf->buffername);
        if (buf->filename != NULL) {
            switch (buf->currentURL.scheme) {
            case SCM_LOCAL:
                if (strcmp(buf->currentURL.file, "-")) {
                    Strcat_char(str, ' ');
                    Strcat_charp(str,
                        conv_from_system(buf->currentURL.real_file));
                }
                break;
                /* case SCM_UNKNOWN: */
            case SCM_MISSING:
                break;
            default:
                Strcat_char(str, ' ');
                p = url_decode2(parsedURL2Str(&buf->currentURL)->ptr, NULL);
                Strcat_charp(str, p);
                break;
            }
        }
        label[i] = str->ptr;
        if (len < str->length)
            len = str->length;
    }
    l = get_strwidth(WcOption, comment);
    if (len < l + 4)
        len = l + 4;
    if (len > COLS - 2 * FRAME_WIDTH)
        len = COLS - 2 * FRAME_WIDTH;
    len = (len > 1) ? ((len - l + 1) / 2) : 0;
    str = Strnew();
    for (i = 0; i < len; i++)
        Strcat_char(str, '-');
    Strcat_charp(str, comment);
    for (i = 0; i < len; i++)
        Strcat_char(str, '-');
    label[nitem] = str->ptr;
    label[nitem + 1] = NULL;

    // new_option_menu(&SelectMenu, label, &SelectV, smChBuf);
    SelectMenu.initial = SelectV;
    SelectMenu.cursorX = Currentbuf->cursorX + Currentbuf->rootX;
    SelectMenu.cursorY = Currentbuf->cursorY + Currentbuf->rootY;
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
        if (clear_buffer)
            tmpClearBuffer(buf);
    }
}

static int
smDelBuf(struct CmdArgs *args)
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

    displayBuffer(args, B_FORCE_REDRAW);
    draw_all_menu(CurrentMenu);
    select_menu(CurrentMenu, CurrentMenu->select);
    return (MENU_NOTHING);
}

/* --- SelectMenu (END) --- */

/* --- SelTabMenu --- */

static void
initSelTabMenu(void)
{
    int i, nitem, len = 0, l;
    TabBuffer* tab;
    struct Buffer* buf;
    Str str;
    char** label;
    char* p;
    static char* comment = " SPC for select / D for delete tab ";

    SelTabV = -1;
    for (i = 0, tab = LastTab; tab != NULL; i++, tab = tab->prevTab) {
        if (tab == CurrentTab)
            SelTabV = i;
    }
    nitem = i;

    label = New_N(char*, nitem + 2);
    for (i = 0, tab = LastTab; i < nitem; i++, tab = tab->prevTab) {
        buf = tab->currentBuffer;
        str = Sprintf("<%s>", buf->buffername);
        if (buf->filename != NULL) {
            switch (buf->currentURL.scheme) {
            case SCM_LOCAL:
                if (strcmp(buf->currentURL.file, "-")) {
                    Strcat_char(str, ' ');
                    Strcat_charp(str,
                        conv_from_system(buf->currentURL.real_file));
                }
                break;
                /* case SCM_UNKNOWN: */
            case SCM_MISSING:
                break;
            default:
                p = url_decode2(parsedURL2Str(&buf->currentURL)->ptr, NULL);
                Strcat_charp(str, p);
                break;
            }
        }
        label[i] = str->ptr;
        if (len < str->length)
            len = str->length;
    }
    l = strlen(comment);
    if (len < l + 4)
        len = l + 4;
    if (len > COLS - 2 * FRAME_WIDTH)
        len = COLS - 2 * FRAME_WIDTH;
    len = (len > 1) ? ((len - l + 1) / 2) : 0;
    str = Strnew();
    for (i = 0; i < len; i++)
        Strcat_char(str, '-');
    Strcat_charp(str, comment);
    for (i = 0; i < len; i++)
        Strcat_char(str, '-');
    label[nitem] = str->ptr;
    label[nitem + 1] = NULL;

    // new_option_menu(&SelTabMenu, label, &SelTabV, smChTab);
    SelTabMenu.initial = SelTabV;
    SelTabMenu.cursorX = Currentbuf->cursorX + Currentbuf->rootX;
    SelTabMenu.cursorY = Currentbuf->cursorY + Currentbuf->rootY;
    SelTabMenu.keymap['D'] = smDelTab;
    SelTabMenu.item[nitem].type = MENU_NOP;
}

static void
smChTab(void)
{
    int i;
    TabBuffer* tab;
    struct Buffer* buf;

    if (SelTabV < 0 || SelTabV >= SelTabMenu.nitem)
        return;
    for (i = 0, tab = LastTab; i < SelTabV && tab != NULL;
        i++, tab = tab->prevTab)
        ;
    CurrentTab = tab;
    for (tab = LastTab; tab != NULL; tab = tab->prevTab) {
        if (tab == CurrentTab)
            continue;
        buf = tab->currentBuffer;
        deleteImage(buf);
        if (clear_buffer)
            tmpClearBuffer(buf);
    }
}

static int
smDelTab(struct CmdArgs *args)
{
    int i, x, y, mselect;
    TabBuffer* tab;

    if (CurrentMenu->select < 0 || CurrentMenu->select >= SelTabMenu.nitem)
        return (MENU_NOTHING);
    for (i = 0, tab = LastTab; i < CurrentMenu->select && tab != NULL;
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

    displayBuffer(args, B_FORCE_REDRAW);
    draw_all_menu(CurrentMenu);
    select_menu(CurrentMenu, CurrentMenu->select);
    return (MENU_NOTHING);
}

/* --- SelectMenu (END) --- */

/* --- OptionMenu --- */

void optionMenu(struct CmdArgs *args, int x, int y, const char** label, int* variable, int initial, const char* cmd)
{
    set_menu_frame();

    Menu menu;
    new_option_menu(&menu, label, variable, cmd);
    menu.cursorX = COLS - 1;
    menu.cursorY = (LINES - 1);
    menu.x = x;
    menu.y = y;
    menu.initial = initial;

    popup_menu(args, NULL, &menu);
}

/* --- OptionMenu (END) --- */

/* --- InitMenu --- */

static void
interpret_menu(FILE* mf)
{
    Str line;
    int in_menu = 0, nmenu = 0, nitem = 0, type;
    MenuItem* item = NULL;
    wc_ces charset = SystemCharset;

    while (!feof(mf)) {
        line = Strfgets(mf);
        Strchop(line);
        Strremovefirstspaces(line);
        if (line->length == 0)
            continue;
        line = Strnew_wc_output(wc_Str_conv(WcOption, line->ptr, line->length, charset, InnerCharset));
        const char* p = line->ptr;
        char* s = getWord(&p);
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
                item = New_Reuse(MenuItem, item, (nitem + 1));
                w3mMenuList[nmenu].item = item;
                item[nitem].type = MENU_END;
            }
        } else if (!strcmp(s, "menu")) {
            s = getQWord(&p);
            if (*s == '\0') /* error */
                continue;
            in_menu = 1;
            if ((nmenu = getMenuN(w3mMenuList, s)) != -1)
                w3mMenuList[nmenu].item = New(MenuItem);
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
        MenuItem* item;
        for (item = MainMenuItem; item->type != MENU_END; item++)
            item->label = Strnew_wc_output(wc_conv(WcOption, item->label, MainMenuCharset, InnerCharset))->ptr;
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

int setMenuItem(MenuItem* item, const char* type, const char* line)
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
        item->cmd = func;
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

int addMenuList(MenuList** mlist, char* id)
{
    int n;
    MenuList* list = *mlist;

    for (n = 0; list->id != NULL; list++, n++)
        ;
    *mlist = New_Reuse(MenuList, *mlist, (n + 2));
    list = *mlist + n;
    list->id = id;
    list->menu = New(Menu);
    list->item = New(MenuItem);
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

/* --- LinkMenu (END) --- */

struct Anchor*
accesskey_menu(struct CmdArgs *args, struct Buffer* buf)
{
    Menu menu;
    struct AnchorList* al = buf->href;
    struct Anchor* a;
    struct Anchor** ap;
    int i, n, nitem = 0, key = -1;
    const char** label;

    if (!al)
        return NULL;
    for (i = 0; i < al->nanchor; i++) {
        a = &al->anchors[i];
        if (!a->slave && a->accesskey && IS_ASCII(a->accesskey))
            nitem++;
    }
    if (!nitem)
        return NULL;

    const char* t;
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
    menu.cursorX = buf->cursorX + buf->rootX;
    menu.cursorY = buf->cursorY + buf->rootY;
    menu.x = menu.cursorX + FRAME_WIDTH + 1;
    menu.y = menu.cursorY + 2;
    for (i = 0; i < 128; i++)
        menu.keyselect[i] = -1;
    for (i = 0; i < nitem; i++) {
        unsigned char c = ap[i]->accesskey;
        menu.keymap[(int)c] = mSelect;
        menu.keyselect[(int)c] = i;
    }
    for (i = 0; i < nitem; i++) {
        unsigned char c = ap[i]->accesskey;
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

    popup_menu(args, NULL, &menu);

    return (key >= 0) ? ap[key] : NULL;
}

static char lmKeys[] = "abcdefgimopqrstuvwxyz";
static char lmKeys2[] = "1234567890ABCDEFGHILMOPQRSTUVWXYZ";
#define nlmKeys (sizeof(lmKeys) - 1)
#define nlmKeys2 (sizeof(lmKeys2) - 1)

static int
lmGoto(struct CmdArgs *args)
{
    if (IS_ASCII(args->ch) && CurrentMenu->keyselect[args->ch] >= 0) {
        goto_menu(CurrentMenu, CurrentMenu->nitem - 1, -1);
        goto_menu(CurrentMenu, CurrentMenu->keyselect[args->ch] * nlmKeys, 1);
    }
    return (MENU_NOTHING);
}

static int
lmSelect(struct CmdArgs *args)
{
    if (IS_ASCII(args->ch))
        return select_menu(CurrentMenu, (CurrentMenu->select / nlmKeys) * nlmKeys + CurrentMenu->keyselect[args->ch]);
    else
        return (MENU_NOTHING);
}

struct Anchor* list_menu(struct CmdArgs *args, struct Buffer* buf)
{
    Menu menu;
    struct AnchorList* al = buf->href;
    struct Anchor* a;
    struct Anchor** ap;
    int i, n, nitem = 0, key = -1, two = false;
    const char** label;
    const char* t;

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
    new_option_menu(&menu, label, &key, NULL);

    menu.initial = 0;
    menu.cursorX = buf->cursorX + buf->rootX;
    menu.cursorY = buf->cursorY + buf->rootY;
    menu.x = menu.cursorX + FRAME_WIDTH + 1;
    menu.y = menu.cursorY + 2;
    for (i = 0; i < 128; i++)
        menu.keyselect[i] = -1;
    if (two) {
        for (i = 0; i < nlmKeys2; i++) {
            int c = lmKeys2[i];
            menu.keymap[c] = lmGoto;
            menu.keyselect[c] = i;
        }
        for (i = 0; i < nlmKeys; i++) {
            int c = lmKeys[i];
            menu.keymap[c] = lmSelect;
            menu.keyselect[c] = i;
        }
    } else {
        for (i = 0; i < nitem; i++) {
            int c = lmKeys[i];
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

    popup_menu(args, NULL, &menu);

    return (key >= 0) ? ap[key] : NULL;
}
