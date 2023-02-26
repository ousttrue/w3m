#include "tty.h"
#include "ctrlcode.h"
#include <stdlib.h>
#include <unistd.h> // read
#include <fcntl.h>  // O_RDWR
#include <errno.h>
#include <signal.h>
#include <stdexcept>
#include <sys/ioctl.h>

#include "config.h"

// #define DEV_TTY_PATH	"con"
#define DEV_TTY_PATH "/dev/tty"

namespace tty
{
static int tty = -1;
static FILE *ttyf = NULL;

#ifdef HAVE_TERMIO_H
#include <termio.h>
typedef struct termio TerminalMode;
#define TerminalSet(fd, x) ioctl(fd, TCSETA, x)
#define TerminalGet(fd, x) ioctl(fd, TCGETA, x)
#define MODEFLAG(d) ((d).c_lflag)
#define IMODEFLAG(d) ((d).c_iflag)
#endif /* HAVE_TERMIO_H */

// #ifdef HAVE_TERMIOS_H
#include <termios.h>
#include <unistd.h>
typedef struct termios TerminalMode;
#define TerminalSet(fd, x) tcsetattr(fd, TCSANOW, x)
#define TerminalGet(fd, x) tcgetattr(fd, x)
#define MODEFLAG(d) ((d).c_lflag)
#define IMODEFLAG(d) ((d).c_iflag)
// #endif /* HAVE_TERMIOS_H */

#ifdef HAVE_SGTTY_H
#include <sgtty.h>
typedef struct sgttyb TerminalMode;
#define TerminalSet(fd, x) ioctl(fd, TIOCSETP, x)
#define TerminalGet(fd, x) ioctl(fd, TIOCGETP, x)
#define MODEFLAG(d) ((d).sg_flags)
#endif /* HAVE_SGTTY_H */

static TerminalMode d_ioval;

void set()
{
    const char *ttyn;

    if (isatty(0)) /* stdin */
        ttyn = ttyname(0);
    else
        ttyn = DEV_TTY_PATH;
    tty = open(ttyn, O_RDWR);
    if (tty < 0)
    {
        /* use stderr instead of stdin... is it OK???? */
        tty = 2;
    }
    ttyf = fdopen(tty, "w");
#ifdef __CYGWIN__
    check_cygwin_console();
#endif
    TerminalGet(tty, &d_ioval);

    // if (displayTitleTerm != NULL)
    // {
    //     struct w3m_term_info *p;
    //     for (p = w3m_term_info_list; p->term != NULL; p++)
    //     {
    //         if (!strncmp(displayTitleTerm, p->term, strlen(p->term)))
    //         {
    //             title_str = p->title_str;
    //             break;
    //         }
    //     }
    // }
}

int get()
{
    return tty;
}

void close()
{
    if (tty > 2)
        ::close(tty);
}

char *get_name()
{
    return ttyname(tty);
}

char getch()
{
    char c;

    while (
#ifdef SUPPORT_WIN9X_CONSOLE_MBCS
        read_win32_console(&c, 1)
#else
        read(tty, &c, 1)
#endif
        < (int)1)
    {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        /* error happend on read(2) */
        // quitfm();
        throw std::runtime_error("fatal");
        break; /* unreachable */
    }
    return c;
}

FILE *get_file()
{
    return ttyf;
}

int write1(char c)
{
    putc(c, ttyf);
#ifdef SCREEN_DEBUG
    tty::flush();
#endif /* SCREEN_DEBUG */
    return 0;
}

void writestr(const char *s)
{
    for (auto p = s; *p; ++p)
    {
        write1(*p);
    }
}

#ifndef HAVE_SGTTY_H
void set_cc(int spec, int val)
{
    TerminalMode ioval;

    TerminalGet(tty, &ioval);
    ioval.c_cc[spec] = val;
    while (TerminalSet(tty, &ioval) == -1)
    {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred: errno=%d\n", errno);
        // reset_error_exit(SIGNAL_ARGLIST);
        throw std::runtime_error("fatal");
    }
}
#endif /* not HAVE_SGTTY_H */

void set_mode(int mode, int imode)
{
    TerminalMode ioval;

    TerminalGet(tty, &ioval);
    MODEFLAG(ioval) |= mode;
#ifndef HAVE_SGTTY_H
    IMODEFLAG(ioval) |= imode;
#endif /* not HAVE_SGTTY_H */

    while (TerminalSet(tty, &ioval) == -1)
    {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred while set %x: errno=%d\n", mode, errno);
        // reset_error_exit(SIGNAL_ARGLIST);
        throw std::runtime_error("fatal");
    }
}

void reset_mode(int mode, int imode)
{
    TerminalMode ioval;

    TerminalGet(tty, &ioval);
    MODEFLAG(ioval) &= ~mode;
#ifndef HAVE_SGTTY_H
    IMODEFLAG(ioval) &= ~imode;
#endif /* not HAVE_SGTTY_H */

    while (TerminalSet(tty, &ioval) == -1)
    {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred while reset %x: errno=%d\n", mode, errno);
        // reset_error_exit(SIGNAL_ARGLIST);
        throw std::runtime_error("fatal");
    }
}

void reset()
{
    TerminalSet(tty, &d_ioval);
    if (tty != 2)
        close();
}

void flush()
{
    if (ttyf)
        fflush(ttyf);
}

void term_raw()
#ifndef HAVE_SGTTY_H
#ifdef IEXTEN
#define TTY_MODE ISIG | ICANON | ECHO | IEXTEN
#else /* not IEXTEN */
#define TTY_MODE ISIG | ICANON | ECHO
#endif /* not IEXTEN */
{
    tty::reset_mode(TTY_MODE, IXON | IXOFF);
#ifdef HAVE_TERMIOS_H
    set_cc(VMIN, 1);
#else  /* not HAVE_TERMIOS_H */
    set_cc(VEOF, 1);
#endif /* not HAVE_TERMIOS_H */
}
#else  /* HAVE_SGTTY_H */
{
    tty::set_mode(RAW, 0);
}
#endif /* HAVE_SGTTY_H */

void crmode(void)
#ifndef HAVE_SGTTY_H
{
    tty::reset_mode(ICANON, IXON);
    tty::set_mode(ISIG, 0);
#ifdef HAVE_TERMIOS_H
    set_cc(VMIN, 1);
#else  /* not HAVE_TERMIOS_H */
    set_cc(VEOF, 1);
#endif /* not HAVE_TERMIOS_H */
}
#else  /* HAVE_SGTTY_H */
{
    tty::set_mode(CBREAK, 0);
}
#endif /* HAVE_SGTTY_H */

void nocrmode(void)
#ifndef HAVE_SGTTY_H
{
    tty::set_mode(ICANON, 0);
#ifdef HAVE_TERMIOS_H
    set_cc(VMIN, 4);
#else  /* not HAVE_TERMIOS_H */
    set_cc(VEOF, 4);
#endif /* not HAVE_TERMIOS_H */
}
#else  /* HAVE_SGTTY_H */
{
    tty::reset_mode(CBREAK, 0);
}
#endif /* HAVE_SGTTY_H */

void term_echo(void)
{
    tty::set_mode(ECHO, 0);
}

void term_noecho(void)
{
    tty::reset_mode(ECHO, 0);
}

void term_cooked(void)
#ifndef HAVE_SGTTY_H
{
    tty::set_mode(TTY_MODE, 0);
#ifdef HAVE_TERMIOS_H
    set_cc(VMIN, 4);
#else  /* not HAVE_TERMIOS_H */
    set_cc(VEOF, 4);
#endif /* not HAVE_TERMIOS_H */
}
#else  /* HAVE_SGTTY_H */
{
    tty::reset_mode(RAW, 0);
}
#endif /* HAVE_SGTTY_H */

void term_cbreak(void)
{
    term_cooked();
    term_noecho();
}

std::optional<std::tuple<unsigned short, unsigned short>> get_lines_cols()
{
    // #if defined(HAVE_TERMIOS_H) && defined(TIOCGWINSZ)
    struct winsize wins;

    auto i = ioctl(tty, TIOCGWINSZ, &wins);
    if (i >= 0 && wins.ws_row != 0 && wins.ws_col != 0)
    {
        return std::make_tuple(wins.ws_row, wins.ws_col);
    }
    else
    {
        return {};
    }
    // #endif /* defined(HAVE-TERMIOS_H) && defined(TIOCGWINSZ) */
}

static void skip_escseq(void)
{
    int c;

    c = getch();
    if (c == '[' || c == 'O')
    {
        c = getch();
        while (IS_DIGIT(c))
            c = getch();
    }
}

int sleep_till_anykey(int sec, int purge)
{
    fd_set rfd;
    struct timeval tim;
    int er, c, ret;
    TerminalMode ioval;

    TerminalGet(tty, &ioval);
    term_raw();

    tim.tv_sec = sec;
    tim.tv_usec = 0;

    FD_ZERO(&rfd);
    FD_SET(tty, &rfd);

    ret = select(tty + 1, &rfd, 0, 0, &tim);
    if (ret > 0 && purge)
    {
        c = getch();
        if (c == ESC_CODE)
            skip_escseq();
    }
    er = TerminalSet(tty, &ioval);
    if (er == -1)
    {
        printf("Error occurred: errno=%d\n", errno);
        // reset_error_exit(SIGNAL_ARGLIST);
        throw std::runtime_error("fatal");
    }
    return ret;
}

int get_pixel_per_cell(int *ppc, int *ppl)
{
    fd_set rfd;
    struct timeval tval;
    char buf[100];
    char *p;
    ssize_t len;
    ssize_t left;
    int wp, hp, wc, hc;
    int i;

#ifdef TIOCGWINSZ
    struct winsize ws;
    if (ioctl(tty, TIOCGWINSZ, &ws) == 0 && ws.ws_ypixel > 0 && ws.ws_row > 0 &&
        ws.ws_xpixel > 0 && ws.ws_col > 0)
    {
        *ppc = ws.ws_xpixel / ws.ws_col;
        *ppl = ws.ws_ypixel / ws.ws_row;
        return 1;
    }
#endif

    fputs("\x1b[14t\x1b[18t", ttyf);
    flush();

    p = buf;
    left = sizeof(buf) - 1;
    for (i = 0; i < 10; i++)
    {
        tval.tv_usec = 200000; /* 0.2 sec * 10 */
        tval.tv_sec = 0;
        FD_ZERO(&rfd);
        FD_SET(tty, &rfd);
        if (select(tty + 1, &rfd, NULL, NULL, &tval) <= 0 || !FD_ISSET(tty, &rfd))
            continue;

        if ((len = read(tty, p, left)) <= 0)
            continue;
        p[len] = '\0';

        if (sscanf(buf, "\x1b[4;%d;%dt\x1b[8;%d;%dt", &hp, &wp, &hc, &wc) == 4)
        {
            if (wp > 0 && wc > 0 && hp > 0 && hc > 0)
            {
                *ppc = wp / wc;
                *ppl = hp / hc;
                return 1;
            }
            else
            {
                return 0;
            }
        }
        p += len;
        left -= len;
    }

    return 0;
}
} // namespace tty
