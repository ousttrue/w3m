/* $Id: terms.c,v 1.63 2010/08/20 09:34:47 htrb Exp $ */
/*
 * An original curses library for EUC-kanji by Akinori ITO,     December 1989
 * revised by Akinori ITO, January 1995
 */
#include "tty.h"
#include "termcap_entry.h"
#include "config.h"
#include <stdexcept>
#include "fm.h"
#include "term_screen.h"

#define MAX_LINE 200
#define MAX_COLUMN 400

static std::string get_term()
{
    if (auto ent = getenv("TERM"))
    {
        return ent;
    }
    return {};
}

TermcapEntry g_entry;

TermScreen g_screen;

void MOVE(int line, int column)
{
    tty::writestr(g_entry.move(line, column));
}

static char *title_str = NULL;

#define W3M_TERM_INFO(name, title, mouse) name, title

auto XTERM_TITLE = "\033]0;w3m: %s\007";
auto SCREEN_TITLE = "\033k%s\033\134";

/* *INDENT-OFF* */
static struct w3m_term_info
{
    const char *term;
    const char *title_str;
} w3m_term_info_list[] = {
    {W3M_TERM_INFO("xterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF))},
    {W3M_TERM_INFO("kterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF))},
    {W3M_TERM_INFO("rxvt", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF))},
    {W3M_TERM_INFO("Eterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF))},
    {W3M_TERM_INFO("mlterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF))},
    {W3M_TERM_INFO("screen", SCREEN_TITLE, 0)},
    {W3M_TERM_INFO(NULL, NULL, 0)}};
#undef W3M_TERM_INFO
/* *INDENT-ON * */

void reset_tty(void)
{
    tty::writestr(g_entry.T_op); /* turn off */
    tty::writestr(g_entry.T_me);
    tty::writestr(g_entry.T_se); /* reset terminal */
    tty::flush();

    tty::reset();
}

extern "C"
{
    extern int tgetnum(const char *);
}

void setlinescols(void)
{
    if (auto size = tty::get_lines_cols())
    {
        LINES = std::get<0>(*size);
        COLS = std::get<1>(*size);
    }

    char *p;
    int i;
    if (LINES <= 0 && (p = getenv("LINES")) != NULL && (i = atoi(p)) >= 0)
        LINES = i;
    if (COLS <= 0 && (p = getenv("COLUMNS")) != NULL && (i = atoi(p)) >= 0)
        COLS = i;
    if (LINES <= 0)
        LINES = tgetnum("li"); /* number of line */
    if (COLS <= 0)
        COLS = tgetnum("co"); /* number of column */
    if (COLS > MAX_COLUMN)
        COLS = MAX_COLUMN;
    if (LINES > MAX_LINE)
        LINES = MAX_LINE;
#if defined(__CYGWIN__)
    LASTLINE = LINES - (isWinConsole == TERM_CYGWIN_RESERVE_IME ? 2 : 1);
#endif /* defined(__CYGWIN__) */
}

/*
 * Screen initialize
 */
int initscr(void)
{
    tty::set();

    auto ent = get_term();
    g_entry.load(ent.c_str());

    LINES = COLS = 0;
    setlinescols();

    tty::writestr(g_entry.T_cl);

    setupscreen();
    return 0;
}

int graph_ok(void)
{
    if (UseGraphicChar != GRAPHIC_CHAR_DEC)
        return 0;
    return g_entry.T_as[0] != 0 && g_entry.T_ae[0] != 0 && g_entry.T_ac[0] != 0;
}

void refresh(void)
{
    wc_putc_init(InnerCharset, DisplayCharset);
    auto buffer = g_screen.render(g_entry);
    fwrite(buffer.data(), 1, buffer.size(), tty::get_file());

    auto pos = g_screen.pos();
    MOVE(pos.row, pos.col);
    tty::flush();
}

static void scroll_raw(void)
{
    MOVE(LINES - 1, 0);
    tty::write1('\n');
}

void term_title(char *s)
{
    if (!fmInitialized)
        return;
    if (title_str != NULL)
    {
        fprintf(tty::get_file(), title_str, s);
    }
}

void bell(void)
{
    tty::write1(7);
}
