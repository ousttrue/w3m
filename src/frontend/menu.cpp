#include <stdio.h>
#include "rc.h"
#include "indep.h"
#include "regex.h"
#include "public.h"
#include "myctype.h"
#include "regex.h"
#include "command_dispatcher.h"
#include "commands.h"
#include "symbol.h"
#include "file.h"
#include "html/html.h"
#include "charset.h"
#include "frontend/anchor.h"
#include "stream/url.h"
#include "frontend/display.h"
#include "frontend/menu.h"

#include "frontend/mouse.h"
#include "frontend/buffer.h"
#include "frontend/tabbar.h"
#include "frontend/lineinput.h"
#include "frontend/terminal.h"
#include "frontend/screen.h"

static const char **FRAME;
static int FRAME_WIDTH;
static int graph_mode = false;
#define G_start                                    \
    {                                              \
        if (graph_mode)                            \
            Screen::Instance().Enable(S_GRAPHICS); \
    }
#define G_end                                       \
    {                                               \
        if (graph_mode)                             \
            Screen::Instance().Disable(S_GRAPHICS); \
    }

static int mEsc(char c);
static int mEscB(char c);
static int mEscD(char c);
static int mNull(char c);
static int mSelect(char c);
static int mDown(char c);
static int mUp(char c);
static int mLast(char c);
static int mTop(char c);
static int mNext(char c);
static int mPrev(char c);
static int mFore(char c);
static int mBack(char c);
static int mLineU(char c);
static int mLineD(char c);
static int mOk(char c);
static int mCancel(char c);
static int mClose(char c);
static int mSusp(char c);
static int mMouse(char c);
static int mSrchF(char c);
static int mSrchB(char c);
static int mSrchN(char c);
static int mSrchP(char c);
#ifdef __EMX__
static int mPc(char c);
#endif

/* *INDENT-OFF* */
static int (*MenuKeymap[128])(char c) = {
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
    // keep indent
};
static int (*MenuEscKeymap[128])(char c) = {
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,

    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,

    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    /*  O     */
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mEscB,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    /*  [                                     */
    mNull, mNull, mNull, mEscB, mNull, mNull, mNull, mNull,

    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    /*  v             */
    mNull, mNull, mNull, mNull, mNull, mNull, mPrev, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    // keep indent
};
static int (*MenuEscBKeymap[128])(char c) = {
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,

    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    /*  A       B       C       D       E                     */
    mNull, mUp, mDown, mOk, mCancel, mClose, mNull, mNull,
    /*  L       M                     */
    mNull, mNull, mNull, mNull, mClose, mMouse, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,

    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    mNull, mNull, mNull, mNull, mNull, mNull, mNull, mNull,
    // keep indent
};
static int (*MenuEscDKeymap[128])(char c) = {
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
    // keep indent
};

/* *INDENT-ON* */
/* --- SelectMenu --- */

static MenuPtr SelectMenu = std::make_shared<Menu>();
static int SelectV = 0;
static void initSelectMenu(void);
static void smChBuf(w3mApp *w3m, const CommandContext &context);
static int smDelBuf(char c);

/* --- SelectMenu (END) --- */

/* --- SelTabMenu --- */

static MenuPtr SelTabMenu = std::make_shared<Menu>();
static int SelTabV = 0;
static void initSelTabMenu(void);
static void smChTab(void);
static int smDelTab(char c);

/* --- SelTabMenu (END) --- */

/* --- MainMenu --- */

static MenuPtr MainMenu = std::make_shared<Menu>();

/* FIXME: gettextize here */
static CharacterEncodingScheme MainMenuCharset = WC_CES_US_ASCII; /* FIXME: charset of source code */
static int MainMenuEncode = false;

static std::vector<MenuItem> MainMenuItem = {
    /* type        label           variable value func     popup keys data  */
    {MENU_FUNC, " Back         (b) ", NULL, 0, backBf, NULL, "b"},
    {MENU_POPUP, " Select Buffer(s) ", NULL, 0, NULL, SelectMenu, "s"},
    {MENU_POPUP, " Select Tab   (t) ", NULL, 0, NULL, SelTabMenu, "tT"},
    {MENU_FUNC, " View Source  (v) ", NULL, 0, vwSrc, NULL, "vV"},
    {MENU_FUNC, " Edit Source  (e) ", NULL, 0, editBf, NULL, "eE"},
    {MENU_FUNC, " Save Source  (S) ", NULL, 0, svSrc, NULL, "S"},
    {MENU_FUNC, " Reload       (r) ", NULL, 0, reload, NULL, "rR"},
    {MENU_NOP, " ---------------- ", NULL, 0, nulcmd, NULL, ""},
    {MENU_FUNC, " Go Link      (a) ", NULL, 0, followA, NULL, "a"},
    {MENU_FUNC, "   on New Tab (n) ", NULL, 0, tabA, NULL, "nN"},
    {MENU_FUNC, " Save Link    (A) ", NULL, 0, svA, NULL, "A"},
    {MENU_FUNC, " View Image   (i) ", NULL, 0, followI, NULL, "i"},
    {MENU_FUNC, " Save Image   (I) ", NULL, 0, svI, NULL, "I"},
    // {MENU_FUNC, " View Frame   (f) ", NULL, 0, rFrame, NULL, "fF"},
    {MENU_NOP, " ---------------- ", NULL, 0, nulcmd, NULL, ""},
    {MENU_FUNC, " Bookmark     (B) ", NULL, 0, ldBmark, NULL, "B"},
    {MENU_FUNC, " Help         (h) ", NULL, 0, ldhelp, NULL, "hH"},
    {MENU_FUNC, " Option       (o) ", NULL, 0, ldOpt, NULL, "oO"},
    {MENU_NOP, " ---------------- ", NULL, 0, nulcmd, NULL, ""},
    {MENU_FUNC, " Quit         (q) ", NULL, 0, qquitfm, NULL, "qQ"},
    {MENU_END, "", NULL, 0, nulcmd, NULL, ""},
    // keep indent
};

/* --- MainMenu (END) --- */

struct MenuList
{
    const char *id;
    MenuPtr menu;
    std::vector<MenuItem> item;
};
static std::vector<MenuList> w3mMenuList;
int addMenuList(const char *id)
{
    w3mMenuList.push_back({});
    auto list = &w3mMenuList.back();
    list->id = id;
    return w3mMenuList.size() - 1;
}

int getMenuN(std::string_view id)
{
    int n = 0;
    for (auto &list : w3mMenuList)
    {
        if (list.id == id)
            return n;
        ++n;
    }
    return -1;
}

static MenuPtr CurrentMenu = std::make_shared<Menu>();

#define mvaddch(y, x, c) (Screen::Instance().LineCol(y, x), addch(c))
#define mvaddstr(y, x, str) (Screen::Instance().LineCol(y, x), Screen::Instance().Puts(str))
#define mvaddnstr(y, x, str, n) (Screen::Instance().LineCol(y, x), Screen::Instance().PutsColumns(str, n))

static void new_menu(MenuPtr menu, std::vector<MenuItem> &item)
{
    int i, l;
    const char *p;

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

    if (item.empty())
        return;

    for (i = 0; item[i].type != MENU_END; i++)
        ;
    menu->nitem = i;
    menu->height = menu->nitem;
    for (i = 0; i < 128; i++)
        menu->keymap[i] = MenuKeymap[i];
    menu->width = 0;
    for (i = 0; i < menu->nitem; i++)
    {
        if ((p = item[i].keys.c_str()) != NULL)
        {
            while (*p)
            {
                if (IS_ASCII(*p))
                {
                    menu->keymap[(int)*p] = mSelect;
                    menu->keyselect[(int)*p] = i;
                }
                p++;
            }
        }
        l = get_strwidth(item[i].label.c_str());
        if (l > menu->width)
            menu->width = l;
    }
}

void Menu::geom_menu(int x, int y, bool mselect)
{
    int win_x, win_y, win_w, win_h;

    this->select = mselect;

    if (this->width % FRAME_WIDTH)
        this->width = (this->width / FRAME_WIDTH + 1) * FRAME_WIDTH;
    win_x = this->x - FRAME_WIDTH;
    win_w = this->width + 2 * FRAME_WIDTH;
    if (win_x + win_w > Terminal::columns())
        win_x = Terminal::columns() - win_w;
    if (win_x < 0)
    {
        win_x = 0;
        if (win_w > Terminal::columns())
        {
            this->width = Terminal::columns() - 2 * FRAME_WIDTH;
            this->width -= this->width % FRAME_WIDTH;
            win_w = this->width + 2 * FRAME_WIDTH;
        }
    }
    this->x = win_x + FRAME_WIDTH;

    win_y = this->y - mselect - 1;
    win_h = this->height + 2;
    if (win_y + win_h > (Terminal::lines() - 1))
        win_y = (Terminal::lines() - 1) - win_h;
    if (win_y < 0)
    {
        win_y = 0;
        if (win_y + win_h > (Terminal::lines() - 1))
        {
            win_h = (Terminal::lines() - 1) - win_y;
            this->height = win_h - 2;
            if (this->height <= mselect)
                this->offset = mselect - this->height + 1;
        }
    }
    this->y = win_y + 1;
}

void draw_all_menu(MenuPtr menu)
{
    if (menu->parent != NULL)
        draw_all_menu(menu->parent);
    draw_menu(menu);
}

void draw_menu(MenuPtr menu)
{
    int x, y, w;
    int i, j;

    x = menu->x - FRAME_WIDTH;
    w = menu->width + 2 * FRAME_WIDTH;
    y = menu->y - 1;

    if (menu->offset == 0)
    {
        G_start;
        mvaddstr(y, x, FRAME[3]);
        for (i = FRAME_WIDTH; i < w - FRAME_WIDTH; i += FRAME_WIDTH)
            mvaddstr(y, x + i, FRAME[10]);
        mvaddstr(y, x + i, FRAME[6]);
        G_end;
    }
    else
    {
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

    for (j = 0; j < menu->height; j++)
    {
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
    if (menu->offset + menu->height == menu->nitem)
    {
        G_start;
        mvaddstr(y, x, FRAME[9]);
        for (i = FRAME_WIDTH; i < w - FRAME_WIDTH; i += FRAME_WIDTH)
            mvaddstr(y, x + i, FRAME[10]);
        mvaddstr(y, x + i, FRAME[12]);
        G_end;
    }
    else
    {
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

void draw_menu_item(MenuPtr menu, int mselect)
{
    mvaddnstr(menu->y + mselect - menu->offset, menu->x,
              menu->item[mselect].label.c_str(), menu->width);
}

int select_menu(MenuPtr menu, int mselect)
{
    if (mselect < 0 || mselect >= menu->nitem)
        return (MENU_NOTHING);
    if (mselect < menu->offset)
        up_menu(menu, menu->offset - mselect);
    else if (mselect >= menu->offset + menu->height)
        down_menu(menu, mselect - menu->offset - menu->height + 1);

    if (menu->select >= menu->offset &&
        menu->select < menu->offset + menu->height)
        draw_menu_item(menu, menu->select);
    menu->select = mselect;
    Screen::Instance().Enable(S_STANDOUT);
    draw_menu_item(menu, menu->select);
    Screen::Instance().Disable(S_STANDOUT);
    /* 
     * move(menu->cursorY, menu->cursorX); */
    Screen::Instance().LineCol(menu->y + mselect - menu->offset, menu->x);
    Screen::Instance().StandToggle();
    Screen::Instance().Refresh();
    Terminal::flush();

    return (menu->select);
}

void goto_menu(MenuPtr menu, int mselect, int down)
{
    int select_in;
    if (mselect >= menu->nitem)
        mselect = menu->nitem - 1;
    else if (mselect < 0)
        mselect = 0;
    select_in = mselect;
    while (menu->item[mselect].type == MENU_NOP)
    {
        if (down > 0)
        {
            if (++mselect >= menu->nitem)
            {
                down_menu(menu, select_in - menu->select);
                mselect = menu->select;
                break;
            }
        }
        else if (down < 0)
        {
            if (--mselect < 0)
            {
                up_menu(menu, menu->select - select_in);
                mselect = menu->select;
                break;
            }
        }
        else
        {
            return;
        }
    }
    select_menu(menu, mselect);
}

void up_menu(MenuPtr menu, int n)
{
    if (n < 0 || menu->offset == 0)
        return;
    menu->offset -= n;
    if (menu->offset < 0)
        menu->offset = 0;

    draw_menu(menu);
}

void down_menu(MenuPtr menu, int n)
{
    if (n < 0 || menu->offset + menu->height == menu->nitem)
        return;
    menu->offset += n;
    if (menu->offset + menu->height > menu->nitem)
        menu->offset = menu->nitem - menu->height;

    draw_menu(menu);
}

int action_menu(MenuPtr menu)
{
    char c;
    int mselect;
    MenuItem item;

    if (menu->active == 0)
    {
        if (menu->parent != NULL)
            menu->parent->active = 0;
        return (0);
    }
    draw_all_menu(menu);
    select_menu(menu, menu->select);

    while (1)
    {
        Terminal::mouse_on();
        c = Terminal::getch();
        Terminal::mouse_off();

        if (IS_ASCII(c))
        { /* Ascii */
            mselect = (*menu->keymap[(int)c])(c);
            if (mselect != MENU_NOTHING)
                break;
        }
    }
    if (mselect >= 0 && mselect < menu->nitem)
    {
        item = menu->item[mselect];
        if (item.type & MENU_POPUP)
        {
            popup_menu(menu, item.popup);
            return (1);
        }
        if (menu->parent != NULL)
            menu->parent->active = 0;
        if (item.type & MENU_VALUE)
            *item.variable = item.value;
        if (item.type & MENU_FUNC)
        {
            (*item.func)(&w3mApp::Instance(), {
                data : item.data,
            });
        }
    }
    else if (mselect == MENU_CLOSE)
    {
        if (menu->parent != NULL)
            menu->parent->active = 0;
    }
    return (0);
}

void popup_menu(MenuPtr parent, MenuPtr menu)
{
    int active = 1;

    if (menu->item.empty())
        return;
    if (menu->active)
        return;

    menu->parent = parent;
    menu->select = menu->initial;
    menu->offset = 0;
    menu->active = 1;
    if (parent != NULL)
    {
        menu->cursorX = parent->cursorX;
        menu->cursorY = parent->cursorY;
        guess_menu_xy(parent, menu->width, &menu->x, &menu->y);
    }
    menu->geom_menu();

    CurrentMenu = menu;
    while (active)
    {
        active = action_menu(CurrentMenu);
        // displayCurrentbuf(B_FORCE_REDRAW);
    }
    menu->active = 0;
    CurrentMenu = parent;
}

void guess_menu_xy(MenuPtr parent, int width, int *x, int *y)
{
    *x = parent->x + parent->width + FRAME_WIDTH - 1;
    if (*x + width + FRAME_WIDTH > Terminal::columns())
    {
        *x = Terminal::columns() - width - FRAME_WIDTH;
        if ((parent->x + parent->width / 2 > *x) &&
            (parent->x + parent->width / 2 > Terminal::columns() / 2))
            *x = parent->x - width - FRAME_WIDTH + 1;
    }
    *y = parent->y + parent->select - parent->offset;
}

void new_option_menu(MenuPtr menu, tcb::span<std::string> label, int *variable, Command func)
{
    if (label.empty())
        return;

    std::vector<MenuItem> item(label.size() + 1);
    for (int i = 0; i < label.size(); i++)
    {
        auto &p = label[i];
        if (func != NULL)
            item[i].type = MENU_VALUE | MENU_FUNC;
        else
            item[i].type = MENU_VALUE;
        item[i].label = Strnew(p)->ptr;
        item[i].variable = variable;
        item[i].value = i;
        item[i].func = func;
        item[i].popup = NULL;
        item[i].keys = "";
    }
    item[label.size()].type = MENU_END;

    new_menu(menu, item);
}

static void
set_menu_frame(void)
{
    if (Terminal::graph_ok())
    {
        graph_mode = true;
        FRAME_WIDTH = 1;
        FRAME = graph_symbol;
    }
    else
    {
        graph_mode = false;

        FRAME_WIDTH = 0;
        FRAME = get_symbol(w3mApp::Instance().DisplayCharset, &FRAME_WIDTH);
        if (!WcOption.use_wide)
            FRAME_WIDTH = 1;
    }
}

/* --- MenuFunctions --- */

#ifdef __EMX__
static int
mPc(char c)
{
    c = getch();
    return (MenuPcKeymap[(int)c](c));
}
#endif

static int
mEsc(char c)
{
    c = Terminal::getch();
    return (MenuEscKeymap[(int)c](c));
}

static int
mEscB(char c)
{
    c = Terminal::getch();
    if (IS_DIGIT(c))
        return (mEscD(c));
    else
        return (MenuEscBKeymap[(int)c](c));
}

static int
mEscD(char c)
{
    int d;

    d = (int)c - (int)'0';
    c = Terminal::getch();
    if (IS_DIGIT(c))
    {
        d = d * 10 + (int)c - (int)'0';
        c = Terminal::getch();
    }
    if (c == '~')
        return (MenuEscDKeymap[d](c));
    else
        return (MENU_NOTHING);
}

static int
mNull(char c)
{
    return (MENU_NOTHING);
}

static int
mSelect(char c)
{
    if (IS_ASCII(c))
        return (select_menu(CurrentMenu, CurrentMenu->keyselect[(int)c]));
    else
        return (MENU_NOTHING);
}

static int
mDown(char c)
{
    if (CurrentMenu->select >= CurrentMenu->nitem - 1)
        return (MENU_NOTHING);
    goto_menu(CurrentMenu, CurrentMenu->select + 1, 1);
    return (MENU_NOTHING);
}

static int
mUp(char c)
{
    if (CurrentMenu->select <= 0)
        return (MENU_NOTHING);
    goto_menu(CurrentMenu, CurrentMenu->select - 1, -1);
    return (MENU_NOTHING);
}

static int
mLast(char c)
{
    goto_menu(CurrentMenu, CurrentMenu->nitem - 1, -1);
    return (MENU_NOTHING);
}

static int
mTop(char c)
{
    goto_menu(CurrentMenu, 0, 1);
    return (MENU_NOTHING);
}

static int
mNext(char c)
{
    int mselect = CurrentMenu->select + CurrentMenu->height;

    if (mselect >= CurrentMenu->nitem)
        return mLast(c);
    down_menu(CurrentMenu, CurrentMenu->height);
    goto_menu(CurrentMenu, mselect, -1);
    return (MENU_NOTHING);
}

static int
mPrev(char c)
{
    int mselect = CurrentMenu->select - CurrentMenu->height;

    if (mselect < 0)
        return mTop(c);
    up_menu(CurrentMenu, CurrentMenu->height);
    goto_menu(CurrentMenu, mselect, 1);
    return (MENU_NOTHING);
}

static int
mFore(char c)
{
    if (CurrentMenu->select >= CurrentMenu->nitem - 1)
        return (MENU_NOTHING);
    goto_menu(CurrentMenu, (CurrentMenu->select + CurrentMenu->height - 1),
              (CurrentMenu->height + 1));
    return (MENU_NOTHING);
}

static int
mBack(char c)
{
    if (CurrentMenu->select <= 0)
        return (MENU_NOTHING);
    goto_menu(CurrentMenu, (CurrentMenu->select - CurrentMenu->height + 1),
              (-1 - CurrentMenu->height));
    return (MENU_NOTHING);
}

static int
mLineU(char c)
{
    int mselect = CurrentMenu->select;

    if (mselect >= CurrentMenu->nitem)
        return mLast(c);
    if (CurrentMenu->offset + CurrentMenu->height >= CurrentMenu->nitem)
        mselect++;
    else
    {
        down_menu(CurrentMenu, 1);
        if (mselect < CurrentMenu->offset)
            mselect++;
    }
    goto_menu(CurrentMenu, mselect, 1);
    return (MENU_NOTHING);
}

static int
mLineD(char c)
{
    int mselect = CurrentMenu->select;

    if (mselect <= 0)
        return mTop(c);
    if (CurrentMenu->offset <= 0)
        mselect--;
    else
    {
        up_menu(CurrentMenu, 1);
        if (mselect >= CurrentMenu->offset + CurrentMenu->height)
            mselect--;
    }
    goto_menu(CurrentMenu, mselect, -1);
    return (MENU_NOTHING);
}

static int
mOk(char c)
{
    int mselect = CurrentMenu->select;

    if (CurrentMenu->item[mselect].type == MENU_NOP)
        return (MENU_NOTHING);
    return (mselect);
}

static int
mCancel(char c)
{
    return (MENU_CANCEL);
}

static int
mClose(char c)
{
    return (MENU_CLOSE);
}

static int
mSusp(char c)
{
    susp(&w3mApp::Instance(), {});
    draw_all_menu(CurrentMenu);
    select_menu(CurrentMenu, CurrentMenu->select);
    return (MENU_NOTHING);
}

static std::string SearchString;

int (*menuSearchRoutine)(MenuPtr, std::string_view, int);

static int menuForwardSearch(MenuPtr menu, std::string_view str, int from)
{
    int i;
    const char *p;
    if ((p = regexCompile(str.data(), w3mApp::Instance().IgnoreCase)) != NULL)
    {
        message(p);
        return -1;
    }
    if (from < 0)
        from = 0;
    for (i = from; i < menu->nitem; i++)
        if (menu->item[i].type != MENU_NOP &&
            regexMatch(menu->item[i].label.c_str(), -1, 1) == 1)
            return i;
    return -1;
}

static int menu_search_forward(MenuPtr menu, int from)
{
    std::string_view str = inputStrHist("Forward: ", "", w3mApp::Instance().TextHist, 1);
    if (str.empty())
        str = SearchString;
    if (str.empty())
        return -1;
    SearchString = str;
    str = GetCurrentBuffer()->conv_search_string(str, w3mApp::Instance().DisplayCharset);
    menuSearchRoutine = menuForwardSearch;
    auto found = menuForwardSearch(menu, str, from + 1);
    if (w3mApp::Instance().WrapSearch && found == -1)
        found = menuForwardSearch(menu, str, 0);
    if (found >= 0)
        return found;
    disp_message("Not found", true);
    return -1;
}

static int
mSrchF(char c)
{
    int mselect;
    mselect = menu_search_forward(CurrentMenu, CurrentMenu->select);
    if (mselect >= 0)
        goto_menu(CurrentMenu, mselect, 1);
    return (MENU_NOTHING);
}

static int menuBackwardSearch(MenuPtr menu, std::string_view str, int from)
{
    int i;
    const char *p;
    if ((p = regexCompile(str.data(), w3mApp::Instance().IgnoreCase)) != NULL)
    {
        message(p);
        return -1;
    }
    if (from >= menu->nitem)
        from = menu->nitem - 1;
    for (i = from; i >= 0; i--)
        if (menu->item[i].type != MENU_NOP &&
            regexMatch(menu->item[i].label.c_str(), -1, 1) == 1)
            return i;
    return -1;
}

static int menu_search_backward(MenuPtr menu, int from)
{
    std::string_view str = inputStrHist("Backward: ", "", w3mApp::Instance().TextHist, 1);
    if (str.empty())
        str = SearchString;
    if (str.empty())
        return -1;
    SearchString = str;
    str = GetCurrentBuffer()->conv_search_string(str, w3mApp::Instance().DisplayCharset);
    menuSearchRoutine = menuBackwardSearch;
    auto found = menuBackwardSearch(menu, str, from - 1);
    if (w3mApp::Instance().WrapSearch && found == -1)
        found = menuBackwardSearch(menu, str, menu->nitem);
    if (found >= 0)
        return found;
    disp_message("Not found", true);
    return -1;
}

static int mSrchB(char c)
{
    auto mselect = menu_search_backward(CurrentMenu, CurrentMenu->select);
    if (mselect >= 0)
        goto_menu(CurrentMenu, mselect, -1);
    return (MENU_NOTHING);
}

static int menu_search_next_previous(MenuPtr menu, int from, int reverse)
{
    static int (*routine[2])(MenuPtr, std::string_view, int) = {
        menuForwardSearch, menuBackwardSearch};

    if (menuSearchRoutine == NULL)
    {
        disp_message("No previous regular expression", true);
        return -1;
    }
    std::string_view str = GetCurrentBuffer()->conv_search_string(SearchString, w3mApp::Instance().DisplayCharset);
    if (reverse != 0)
        reverse = 1;
    if (menuSearchRoutine == menuBackwardSearch)
        reverse ^= 1;
    from += reverse ? -1 : 1;
    auto found = (*routine[reverse])(menu, str, from);
    if (w3mApp::Instance().WrapSearch && found == -1)
        found = (*routine[reverse])(menu, str, reverse * menu->nitem);
    if (found >= 0)
        return found;
    disp_message("Not found", true);
    return -1;
}

static int
mSrchN(char c)
{
    int mselect;
    mselect = menu_search_next_previous(CurrentMenu, CurrentMenu->select, 0);
    if (mselect >= 0)
        goto_menu(CurrentMenu, mselect, 1);
    return (MENU_NOTHING);
}

static int
mSrchP(char c)
{
    int mselect;
    mselect = menu_search_next_previous(CurrentMenu, CurrentMenu->select, 1);
    if (mselect >= 0)
        goto_menu(CurrentMenu, mselect, -1);
    return (MENU_NOTHING);
}

static int
mMouse_scroll_line(void)
{
    int i = 0;
    if (w3mApp::Instance().relative_wheel_scroll)
        i = (w3mApp::Instance().relative_wheel_scroll_ratio * CurrentMenu->height + 99) / 100;
    else
        i = w3mApp::Instance().fixed_wheel_scroll_count;
    return i ? i : 1;
}

static int
process_mMouse(int btn, int x, int y)
{
    MenuPtr menu;
    int mselect, i;
    static int press_btn = MOUSE_BTN_RESET, press_x, press_y;
    char c = ' ';

    menu = CurrentMenu;

    if (x < 0 || x >= Terminal::columns() || y < 0 || y > (Terminal::lines() - 1))
        return (MENU_NOTHING);

    if (btn == MOUSE_BTN_UP)
    {
        switch (press_btn)
        {
        case MOUSE_BTN1_DOWN:
        case MOUSE_BTN3_DOWN:
            if (x < menu->x - FRAME_WIDTH ||
                x >= menu->x + menu->width + FRAME_WIDTH ||
                y < menu->y - 1 || y >= menu->y + menu->height + 1)
            {
                return (MENU_CANCEL);
            }
            else if ((x >= menu->x - FRAME_WIDTH &&
                      x < menu->x) ||
                     (x >= menu->x + menu->width &&
                      x < menu->x + menu->width + FRAME_WIDTH))
            {
                return (MENU_NOTHING);
            }
            else if (press_y > y)
            {
                for (i = 0; i < press_y - y; i++)
                    mLineU(c);
                return (MENU_NOTHING);
            }
            else if (press_y < y)
            {
                for (i = 0; i < y - press_y; i++)
                    mLineD(c);
                return (MENU_NOTHING);
            }
            else if (y == menu->y - 1)
            {
                mPrev(c);
                return (MENU_NOTHING);
            }
            else if (y == menu->y + menu->height)
            {
                mNext(c);
                return (MENU_NOTHING);
            }
            else
            {
                mselect = y - menu->y + menu->offset;
                if (menu->item[mselect].type == MENU_NOP)
                    return (MENU_NOTHING);
                return (select_menu(menu, mselect));
            }
            break;
        case MOUSE_BTN4_DOWN_RXVT:
            for (i = 0; i < mMouse_scroll_line(); i++)
                mLineD(c);
            break;
        case MOUSE_BTN5_DOWN_RXVT:
            for (i = 0; i < mMouse_scroll_line(); i++)
                mLineU(c);
            break;
        }
    }
    else if (btn == MOUSE_BTN4_DOWN_XTERM)
    {
        for (i = 0; i < mMouse_scroll_line(); i++)
            mLineD(c);
    }
    else if (btn == MOUSE_BTN5_DOWN_XTERM)
    {
        for (i = 0; i < mMouse_scroll_line(); i++)
            mLineU(c);
    }

    if (btn != MOUSE_BTN4_DOWN_RXVT || press_btn == MOUSE_BTN_RESET)
    {
        press_btn = btn;
        press_x = x;
        press_y = y;
    }
    else
    {
        press_btn = MOUSE_BTN_RESET;
    }
    return (MENU_NOTHING);
}

static int
mMouse(char c)
{
    int btn, x, y;

    btn = (unsigned char)Terminal::getch() - 32;
    x = (unsigned char)Terminal::getch() - 33;
    if (x < 0)
        x += 0x100;
    y = (unsigned char)Terminal::getch() - 33;
    if (y < 0)
        y += 0x100;

    /* 
     * if (x < 0 || x >= Terminal::columns() || y < 0 || y > (Terminal::lines()-1)) return; */
    return process_mMouse(btn, x, y);
}

/* --- MainMenu --- */

void popupMenu(int x, int y, MenuPtr menu)
{
    set_menu_frame();

    initSelectMenu();
    initSelTabMenu();

    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    auto [_x, _y] = buf->GlobalXY();
    menu->cursorX = _x;
    menu->cursorY = _y;
    menu->x = x + FRAME_WIDTH + 1;
    menu->y = y + 2;

    popup_menu(NULL, menu);
}

void mainMenu(int x, int y)
{
    popupMenu(x, y, MainMenu);
}

/* --- MainMenu (END) --- */

/* --- SelectMenu --- */

static void
initSelectMenu(void)
{
    int i, nitem, len = 0, l;
    BufferPtr buf;
    Str str;
    char **label;
    char *p;
    static const char *comment = " SPC for select / D for delete buffer ";

    assert(false);

    // SelectV = GetCurrentTab()->GetCurrentBufferIndex();
    // nitem = i;

    // label = New_N(char *, nitem + 2);
    // auto tab = GetCurrentTab();
    // for (i = 0; i < nitem; i++)
    // {
    //     auto buf = tab->GetBuffer(i);

    //     str = Sprintf("<%s>", buf->buffername);
    //     if (buf->filename.size())
    //     {
    //         switch (buf->currentURL.scheme)
    //         {
    //         case SCM_LOCAL:
    //             if (!buf->currentURL.StdIn())
    //             {
    //                 str->Push(' ');
    //                 str->Push(conv_from_system(buf->currentURL.real_file));
    //             }
    //             break;
    //             /* case SCM_UNKNOWN: */
    //         case SCM_MISSING:
    //             break;
    //         default:
    //             str->Push(' ');
    //             p = buf->currentURL.ToStr()->ptr;
    //             if (DecodeURL)
    //                 p = url_unquote_conv(p, WC_CES_NONE);
    //             str->Push(p);
    //             break;
    //         }
    //     }
    //     label[i] = str->ptr;
    //     if (len < str->Size())
    //         len = str->Size();
    // }
    // l = get_strwidth(comment);
    // if (len < l + 4)
    //     len = l + 4;
    // if (len > Terminal::columns() - 2 * FRAME_WIDTH)
    //     len = Terminal::columns() - 2 * FRAME_WIDTH;
    // len = (len > 1) ? ((len - l + 1) / 2) : 0;
    // str = Strnew();
    // for (i = 0; i < len; i++)
    //     str->Push('-');
    // str->Push(comment);
    // for (i = 0; i < len; i++)
    //     str->Push('-');
    // label[nitem] = str->ptr;
    // label[nitem + 1] = NULL;

    // new_option_menu(&SelectMenu, label, &SelectV, smChBuf);
    // SelectMenu.initial = SelectV;
    // auto [_x, _y] = GetCurrentBuffer()->GlobalXY();
    // SelectMenu.cursorX = _x;
    // SelectMenu.cursorY = _y;
    // SelectMenu.keymap['D'] = smDelBuf;
    // SelectMenu.item[nitem].type = MENU_NOP;
}

static void
smChBuf(w3mApp *w3m, const CommandContext &context)
{
    if (SelectV < 0 || SelectV >= SelectMenu->nitem)
        return;

    auto tab = GetCurrentTab();
    // auto buf = tab->GetBuffer(SelectV);
    tab->SetCurrent(SelectV);
}

static int
smDelBuf(char c)
{
    int i, x, y, mselect;

    if (CurrentMenu->select < 0 || CurrentMenu->select >= SelectMenu->nitem)
        return (MENU_NOTHING);

    auto tab = GetCurrentTab();
    assert(false);
    // auto buf = tab->GetBuffer(CurrentMenu->select);
    // GetCurrentTab()->DeleteBuffer(buf);

    x = CurrentMenu->x;
    y = CurrentMenu->y;
    mselect = CurrentMenu->select;

    initSelectMenu();

    CurrentMenu->x = x;
    CurrentMenu->y = y;

    CurrentMenu->geom_menu(x, y);

    CurrentMenu->select = (mselect <= CurrentMenu->nitem - 2) ? mselect
                                                              : (CurrentMenu->nitem - 2);

    // displayCurrentbuf(B_FORCE_REDRAW);
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
    TabPtr tab;
    BufferPtr buf;
    Str str;
    char **label;
    char *p;
    static const char *comment = " SPC for select / D for delete tab ";

    // TODO

    // SelTabV = -1;
    // for (i = 0, tab = GetLastTab(); tab != NULL; i++, tab = tab->prevTab)
    // {
    //     if (tab == GetCurrentTab())
    //         SelTabV = i;
    // }
    // nitem = i;

    // label = New_N(char *, nitem + 2);
    // for (i = 0, tab = GetLastTab(); i < nitem; i++, tab = tab->prevTab)
    // {
    //     buf = tab->currentBuffer;
    //     str = Sprintf("<%s>", buf->buffername);
    //     if (buf->filename != NULL)
    //     {
    //         switch (buf->currentURL.scheme)
    //         {
    //         case SCM_LOCAL:
    //             if (strcmp(buf->currentURL.file, "-"))
    //             {
    //                 str->Push( ' ');
    //                 str->Push(
    //                              conv_from_system(buf->currentURL.real_file));
    //             }
    //             break;
    //             /* case SCM_UNKNOWN: */
    //         case SCM_MISSING:
    //             break;
    //         default:
    //             p = buf->currentURL.ToStr()->ptr;
    //             if (DecodeURL)
    //                 p = url_unquote_conv(p, 0);
    //             str->Push( p);
    //             break;
    //         }
    //     }
    //     label[i] = str->ptr;
    //     if (len < str->length)
    //         len = str->length;
    // }
    // l = strlen(comment);
    // if (len < l + 4)
    //     len = l + 4;
    // if (len > Terminal::columns() - 2 * FRAME_WIDTH)
    //     len = Terminal::columns() - 2 * FRAME_WIDTH;
    // len = (len > 1) ? ((len - l + 1) / 2) : 0;
    // str = Strnew();
    // for (i = 0; i < len; i++)
    //     str->Push( '-');
    // str->Push( comment);
    // for (i = 0; i < len; i++)
    //     str->Push( '-');
    // label[nitem] = str->ptr;
    // label[nitem + 1] = NULL;

    // new_option_menu(&SelTabMenu, label, &SelTabV, smChTab);
    // SelTabMenu.initial = SelTabV;
    // SelTabMenu.cursorX = GetCurrentBuffer()->cursorX + GetCurrentBuffer()->rootX;
    // SelTabMenu.cursorY = GetCurrentBuffer()->cursorY + GetCurrentBuffer()->rootY;
    // SelTabMenu.keymap['D'] = smDelTab;
    // SelTabMenu.item[nitem].type = MENU_NOP;
}

static void
smChTab(void)
{
    // TODO
    //     int i;
    //     TabBuffer *tab;
    //     BufferPtr buf;

    //     if (SelTabV < 0 || SelTabV >= SelTabMenu.nitem)
    //         return;
    //     for (i = 0, tab = GetLastTab(); i < SelTabV && tab != NULL;
    //          i++, tab = tab->prevTab)
    //         ;
    //     SetCurrentTab(tab);
    //     for (tab = GetLastTab(); tab != NULL; tab = tab->prevTab)
    //     {
    //         if (tab == GetCurrentTab())
    //             continue;
    //         buf = tab->currentBuffer;
    // #ifdef USE_IMAGE
    //         deleteImage(buf);
    // #endif
    //         if (clear_buffer)
    //             tmpClearBuffer(buf);
    //     }
}

static int
smDelTab(char c)
{
    int i, x, y, mselect;
    TabPtr tab;

    // TODO

    // if (CurrentMenu->select < 0 || CurrentMenu->select >= SelTabMenu.nitem)
    //     return (MENU_NOTHING);
    // for (i = 0, tab = GetLastTab(); i < CurrentMenu->select && tab != NULL;
    //      i++, tab = tab->prevTab)
    //     ;
    // deleteTab(tab);

    // x = CurrentMenu->x;
    // y = CurrentMenu->y;
    // mselect = CurrentMenu->select;

    // initSelTabMenu();

    // CurrentMenu->x = x;
    // CurrentMenu->y = y;

    // geom_menu(CurrentMenu, x, y, 0);

    // CurrentMenu->select = (mselect <= CurrentMenu->nitem - 2) ? mselect
    //                                                           : (CurrentMenu->nitem - 2);

    // displayCurrentbuf(B_FORCE_REDRAW);
    // draw_all_menu(CurrentMenu);
    // select_menu(CurrentMenu, CurrentMenu->select);
    return (MENU_NOTHING);
}

/* --- SelectMenu (END) --- */

/* --- OptionMenu --- */

void optionMenu(int x, int y, tcb::span<std::string> label, int *variable, int initial,
                Command func)
{
    auto menu = std::make_shared<Menu>();

    set_menu_frame();

    new_option_menu(menu, label, variable, func);
    menu->cursorX = Terminal::columns() - 1;
    menu->cursorY = (Terminal::lines() - 1);
    menu->x = x;
    menu->y = y;
    menu->initial = initial;

    popup_menu(NULL, menu);
}

/* --- OptionMenu (END) --- */

/* --- InitMenu --- */

static void
interpret_menu(FILE *mf)
{
    Str line;
    int in_menu = 0, nmenu = 0, nitem = 0;
    std::vector<MenuItem> *item = nullptr;

    CharacterEncodingScheme charset = w3mApp::Instance().SystemCharset;

    while (!feof(mf))
    {
        line = Strfgets(mf);
        Strip(line);
        if (line->Size() == 0)
            continue;

        line = wc_Str_conv(line, charset, w3mApp::Instance().InnerCharset);

        std::string_view p = line->ptr;
        std::string s;
        std::tie(p, s) = getWord(p);
        if (s.size() && s[0] == '#') /* comment */
            continue;
        if (in_menu)
        {
            auto type = (*item)[nitem].setMenuItem(s, p);
            if (type == -1)
                continue; /* error */
            if (type == MENU_END)
                in_menu = 0;
            else
            {
                nitem++;
                item->resize(nitem + 1);
                // w3mMenuList[nmenu].item = item;
                (*item)[nitem].type = MENU_END;
            }
        }
        else if (s == "menu")
        {
            std::tie(p, s) = getQWord(p);
            if (s.size()) /* error */
                continue;
            in_menu = 1;
            if ((nmenu = getMenuN(s.c_str())) != -1)
                w3mMenuList[nmenu].item.resize(1);
            else
                nmenu = addMenuList(s.c_str());
            item = &w3mMenuList[nmenu].item;
            nitem = 0;
            (*item)[nitem].type = MENU_END;
        }
        else if (s == "charset" || s == "encoding")
        {
            std::tie(p, s) = getQWord(p);
            if (s.size()) /* error */
                continue;
            charset = wc_guess_charset(s.c_str(), charset);
        }
    }
}

void initMenu(void)
{
    FILE *mf;
    MenuList *list;

    w3mMenuList.resize(3);
    w3mMenuList[0].id = "Main";
    w3mMenuList[0].menu = MainMenu;
    w3mMenuList[0].item = MainMenuItem;
    w3mMenuList[1].id = "Select";
    w3mMenuList[1].menu = SelectMenu;
    // w3mMenuList[1].item = NULL;
    w3mMenuList[2].id = "SelectTab";
    w3mMenuList[2].menu = SelTabMenu;
    // w3mMenuList[2].item = NULL;
    w3mMenuList[3].id = NULL;

    if (!MainMenuEncode)
    {
        MenuItem *item;
#ifdef ENABLE_NLS
        /* FIXME: charset that gettext(3) returns */
        MainMenuCharset = SystemCharset;
#endif
        for (item = MainMenuItem.data(); item->type != MENU_END; item++)
            item->label =
                wc_conv(item->label.c_str(), MainMenuCharset, w3mApp::Instance().InnerCharset)->ptr;
        MainMenuEncode = true;
    }

    if ((mf = fopen(confFile(MENU_FILE), "rt")) != NULL)
    {
        interpret_menu(mf);
        fclose(mf);
    }
    if ((mf = fopen(rcFile(MENU_FILE), "rt")) != NULL)
    {
        interpret_menu(mf);
        fclose(mf);
    }

    for (auto &list : w3mMenuList)
    {
        if (list.item.empty())
            continue;
        new_menu(list.menu, list.item);
    }
}

MenuTypes MenuItem::setMenuItem(std::string_view type, std::string_view line)
{
    if (type.empty()) /* error */
        return MENU_ERROR;

    if (type == "end")
    {
        this->type = MENU_END;
        return MENU_END;
    }

    if (type == "nop")
    {
        this->type = MENU_NOP;
        std::tie(line, this->label) = getQWord(line);
        return MENU_NOP;
    }

    if (type == "func")
    {
        std::string label;
        std::tie(line, label) = getQWord(line);
        std::string func;
        std::tie(line, func) = getWord(line);
        std::string keys;
        std::tie(line, keys) = getQWord(line);
        std::string data;
        std::tie(line, data) = getQWord(line);
        if (func.empty()) /* error */
            return MENU_ERROR;
        this->type = MENU_FUNC;
        this->label = label;
        Command f = getFuncList(func);
        this->func = f;
        this->keys = keys;
        this->data = data;
        return MENU_FUNC;
    }

    if (type == "popup")
    {
        std::string label;
        std::tie(line, label) = getQWord(line);
        std::string popup;
        std::tie(line, popup) = getQWord(line);
        std::string keys;
        std::tie(line, keys) = getQWord(line);
        if (popup.empty()) /* error */
            return MENU_ERROR;
        this->type = MENU_POPUP;
        this->label = label;
        int n;
        if ((n = getMenuN(popup.data())) == -1)
            n = addMenuList(popup.data());
        this->popup = w3mMenuList[n].menu;
        this->keys = keys;
        return MENU_POPUP;
    }

    return MENU_ERROR;
}

///
/// Bufferのリンクリストをポップアップし、選択されたリンクを返す
///
Link *link_menu(const BufferPtr &buf)
{
    if (buf->m_document->linklist.empty())
        return NULL;

    // int i, nitem, len = 0, ;
    // Str str;

    std::vector<std::string> labelBuffer;
    std::vector<std::string> labels;
    auto maxLen = 0;
    for (auto &l : buf->m_document->linklist)
    {
        auto str = Strnew_m_charp(l.title(), l.type());
        std::string_view p;
        if (l.url().empty())
            p = "";
        else if (w3mApp::Instance().DecodeURL)
            p = url_unquote_conv(l.url(), buf->m_document->document_charset);
        else
            p = l.url();
        str->Push(p);

        labelBuffer.push_back(str->ptr);
        labels.push_back(labelBuffer.back().c_str());
        if (str->Size() > maxLen)
            maxLen = str->Size();
    }

    set_menu_frame();
    MenuPtr menu = std::make_shared<Menu>();
    int linkV = -1;
    new_option_menu(menu, labels, &linkV, NULL);
    menu->initial = 0;
    auto [_x, _y] = buf->GlobalXY();
    menu->cursorX = _x;
    menu->cursorY = _y;
    menu->x = menu->cursorX + FRAME_WIDTH + 1;
    menu->y = menu->cursorY + 2;
    popup_menu(NULL, menu);

    if (linkV < 0 || linkV >= buf->m_document->linklist.size())
        return NULL;
    return &buf->m_document->linklist[linkV];
}

/* --- LinkMenu (END) --- */

AnchorPtr accesskey_menu(const BufferPtr &buf)
{
    AnchorList &al = buf->m_document->href;
    if (!al)
        return NULL;

    int nitem = 0;
    for (int i = 0; i < al.size(); i++)
    {
        auto a = al.anchors[i];
        if (!a->slave && a->accesskey && IS_ASCII(a->accesskey))
            nitem++;
    }
    if (!nitem)
        return NULL;

    auto menu = std::make_shared<Menu>();
    int i, key = -1;
    char *t;
    unsigned char c;

    std::vector<std::string> label;
    std::vector<AnchorPtr> ap(nitem);
    int n;
    for (i = 0, n = 0; i < al.size(); i++)
    {
        auto a = al.anchors[i];
        if (!a->slave && a->accesskey && IS_ASCII(a->accesskey))
        {
            t = getAnchorText(buf, al, a);
            label.push_back(Sprintf("%c: %s", a->accesskey, t ? t : "")->ptr);
            ap[n] = a;
            n++;
        }
    }

    new_option_menu(menu, label, &key, NULL);

    menu->initial = 0;
    auto [_x, _y] = buf->GlobalXY();
    menu->cursorX = _x;
    menu->cursorY = _y;
    menu->x = menu->cursorX + FRAME_WIDTH + 1;
    menu->y = menu->cursorY + 2;
    for (i = 0; i < 128; i++)
        menu->keyselect[i] = -1;
    for (i = 0; i < nitem; i++)
    {
        c = ap[i]->accesskey;
        menu->keymap[(int)c] = mSelect;
        menu->keyselect[(int)c] = i;
    }
    for (i = 0; i < nitem; i++)
    {
        c = ap[i]->accesskey;
        if (!IS_ALPHA(c) || menu->keyselect[n] >= 0)
            continue;
        c = TOLOWER(c);
        menu->keymap[(int)c] = mSelect;
        menu->keyselect[(int)c] = i;
        c = TOUPPER(c);
        menu->keymap[(int)c] = mSelect;
        menu->keyselect[(int)c] = i;
    }

    auto a = buf->m_document->href.RetrieveAnchor(buf->CurrentPoint());
    if (a && a->accesskey && IS_ASCII(a->accesskey))
    {
        for (i = 0; i < nitem; i++)
        {
            if (a->hseq == ap[i]->hseq)
            {
                menu->initial = i;
                break;
            }
        }
    }

    popup_menu(NULL, menu);

    return (key >= 0) ? ap[key] : NULL;
}

static char lmKeys[] = "abcdefgimopqrstuvwxyz";
static char lmKeys2[] = "1234567890ABCDEFGHILMOPQRSTUVWXYZ";
#define nlmKeys (sizeof(lmKeys) - 1)
#define nlmKeys2 (sizeof(lmKeys2) - 1)

static int
lmGoto(char c)
{
    if (IS_ASCII(c) && CurrentMenu->keyselect[(int)c] >= 0)
    {
        goto_menu(CurrentMenu, CurrentMenu->nitem - 1, -1);
        goto_menu(CurrentMenu, CurrentMenu->keyselect[(int)c] * nlmKeys, 1);
    }
    return (MENU_NOTHING);
}

static int
lmSelect(char c)
{
    if (IS_ASCII(c))
        return select_menu(CurrentMenu, (CurrentMenu->select / nlmKeys) *
                                                nlmKeys +
                                            CurrentMenu->keyselect[(int)c]);
    else
        return (MENU_NOTHING);
}

AnchorPtr list_menu(const BufferPtr &buf)
{
    auto menu = std::make_shared<Menu>();
    AnchorList &al = buf->m_document->href;
    int i, n, nitem = 0, key = -1, two = false;
    const char *t;
    unsigned char c;

    if (!al)
        return NULL;
    for (i = 0; i < al.size(); i++)
    {
        auto a = al.anchors[i];
        if (!a->slave)
            nitem++;
    }
    if (!nitem)
        return NULL;

    if (nitem >= nlmKeys)
        two = true;

    std::vector<std::string> label;
    std::vector<AnchorPtr> ap(nitem);
    for (i = 0, n = 0; i < al.size(); i++)
    {
        auto a = al.anchors[i];
        if (!a->slave)
        {
            t = getAnchorText(buf, al, a);
            if (!t)
                t = "";
            if (two && n >= nlmKeys2 * nlmKeys)
                label.push_back(Sprintf("  : %s", t)->ptr);
            else if (two)
                label.push_back(Sprintf("%c%c: %s", lmKeys2[n / nlmKeys],
                                        lmKeys[n % nlmKeys], t)
                                    ->ptr);
            else
                label.push_back(Sprintf("%c: %s", lmKeys[n], t)->ptr);
            ap[n] = a;
            n++;
        }
    }

    set_menu_frame();
    set_menu_frame();
    new_option_menu(menu, label, &key, NULL);

    menu->initial = 0;
    auto [_x, _y] = buf->GlobalXY();
    menu->cursorX = _x;
    menu->cursorY = _y;
    menu->x = menu->cursorX + FRAME_WIDTH + 1;
    menu->y = menu->cursorY + 2;
    for (i = 0; i < 128; i++)
        menu->keyselect[i] = -1;
    if (two)
    {
        for (i = 0; i < nlmKeys2; i++)
        {
            c = lmKeys2[i];
            menu->keymap[(int)c] = lmGoto;
            menu->keyselect[(int)c] = i;
        }
        for (i = 0; i < nlmKeys; i++)
        {
            c = lmKeys[i];
            menu->keymap[(int)c] = lmSelect;
            menu->keyselect[(int)c] = i;
        }
    }
    else
    {
        for (i = 0; i < nitem; i++)
        {
            c = lmKeys[i];
            menu->keymap[(int)c] = mSelect;
            menu->keyselect[(int)c] = i;
        }
    }

    auto a = buf->m_document->href.RetrieveAnchor(buf->CurrentPoint());
    if (a)
    {
        for (i = 0; i < nitem; i++)
        {
            if (a->hseq == ap[i]->hseq)
            {
                menu->initial = i;
                break;
            }
        }
    }

    popup_menu(NULL, menu);

    return (key >= 0) ? ap[key] : NULL;
}

void PopupMenu(std::string_view data)
{
    MenuPtr menu = MainMenu;

    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    auto [x, y] = buf->GlobalXY();

    if (data.size())
    {
        auto n = getMenuN(data);
        if (n < 0)
            return;
        menu = w3mMenuList[n].menu;
    }
    TryGetMouseActionPosition(&x, &y);
    popupMenu(x, y, menu);
}

void PopupBufferMenu()
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    auto [x, y] = buf->GlobalXY();

    TryGetMouseActionPosition(&x, &y);
    popupMenu(x, y, SelectMenu);
}

void PopupTabMenu()
{
    auto tab = GetCurrentTab();
    auto buf = GetCurrentBuffer();
    auto [x, y] = buf->GlobalXY();

    TryGetMouseActionPosition(&x, &y);
    popupMenu(x, y, SelTabMenu);
}
