#include "w3m_tty.h"
#include "w3m.h"
#include "str_gc.h"
#include "str_const.h"
#include "ctrlcode.h"
#include "rc.h"
#include "proto.h"
#include "tab.h"
#include <errno.h>
#include <fcntl.h>
#include <gc/gc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

#define DEV_TTY_PATH "/dev/tty"

SigActionFunc prevtrap = NULL;

void TrapOn(SigActionFunc keyAbort, volatile SigActionFunc *prevtrap)
{
    if (TrapSignal) {
        *prevtrap = mySignal(SIGINT, keyAbort);
        if (fmInitialized)
            tty_cbreak();
    }
}

void TrapOff()
{
    if (TrapSignal) {
        if (fmInitialized)
            tty_raw();
        if (prevtrap)
            mySignal(SIGINT, prevtrap);
    }
}

const char* displayTitleTerm = NULL;
int opt_cols = 0;
int use_ti_te = true;

// typedef struct termios TerminalMode;
// #define TerminalSet(fd, x) tcsetattr(fd, TCSANOW, x)
// #define TerminalGet(fd, x) tcgetattr(fd, x)
// #define MODEFLAG(d) ((d).c_lflag)
// #define IMODEFLAG(d) ((d).c_iflag)

static struct termios d_ioval;
static int tty = -1;
FILE* ttyf = NULL;
FILE* tty_file(void)
{
    return ttyf;
}

static const char *title_str_common = "\033]0;w3m: %s\007",
                  *title_str_screen = "\033k%s\033\134";
static const char* title_str;

static struct TermSize TERM_SIZE = { 0, 0 };
struct TermSize termSize()
{
    return TERM_SIZE;
}

extern int tgetent(char*, char*);
extern int tgetnum(char*);
extern int tgetflag(char*);
extern char* tgetstr(char*, char**);
extern char* tgoto(char*, int, int);
extern int tputs(const char*, int, int (*)(char));

void tty_update_size(void)
{
    char* p;
    int i;
    struct winsize wins;

    i = ioctl(tty, TIOCGWINSZ, &wins);
    if (i >= 0 && wins.ws_row != 0 && wins.ws_col != 0) {
        TERM_SIZE.lines = wins.ws_row;
        TERM_SIZE.cols = wins.ws_col;
    }

    if (LINES <= 0 && (p = getenv("LINES")) != NULL && (i = atoi(p)) >= 0)
        TERM_SIZE.lines = i;
    if (COLS <= 0 && (p = getenv("COLUMNS")) != NULL && (i = atoi(p)) >= 0)
        TERM_SIZE.cols = i;
    if (LINES <= 0)
        TERM_SIZE.lines = tgetnum("li"); /* number of line */
    if (COLS <= 0)
        TERM_SIZE.cols = tgetnum("co"); /* number of column */
    if (MaxCols && COLS > MaxCols)
        TERM_SIZE.cols = MaxCols;
    if (opt_cols && COLS > opt_cols)
        TERM_SIZE.cols = opt_cols;
}
int set_tty(void)
{
    char* ttyn;

    if (isatty(0)) /* stdin */
        ttyn = ttyname(0);
    else
        ttyn = DEV_TTY_PATH;
    tty = open(ttyn, O_RDWR);
    if (tty < 0) {
        /* use stderr instead of stdin... is it OK???? */
        tty = 2;
    }
    ttyf = fdopen(tty, "w");
    tcgetattr(tty, &d_ioval);
    if (displayTitleTerm) {
        if (!strncmp(displayTitleTerm, "screen", 6)) {
            title_str = title_str_screen;
        } else {
            title_str = title_str_common;
        }
    }
    return 0;
}

static void
reset_exit_with_value(int, int rval)
{
    tty_reset();
    w3m_exit(rval);
}

void reset_error_exit(int)
{
    reset_exit_with_value(0, 1);
}

void reset_exit(int)
{
    reset_exit_with_value(0, 0);
}

void error_dump(int)
{
    mySignal(SIGIOT, SIG_DFL);
    tty_reset();
    abort();
}

static void set_int(void)
{
    mySignal(SIGHUP, reset_exit);
    mySignal(SIGINT, reset_exit);
    mySignal(SIGQUIT, reset_exit);
    mySignal(SIGTERM, reset_exit);
    mySignal(SIGILL, error_dump);
    mySignal(SIGIOT, error_dump);
    mySignal(SIGFPE, error_dump);
#ifdef SIGBUS
    mySignal(SIGBUS, error_dump);
#endif /* SIGBUS */
    /* mySignal(SIGSEGV, error_dump); */
}

static char bp[1024], funcstr[256];
char gcmap[96];

char *T_cd, *T_ce, *T_kr, *T_kl, *T_cr, *T_bt, *T_ta, *T_sc, *T_rc,
    *T_so, *T_se, *T_us, *T_ue, *T_cl, *T_cm, *T_al, *T_sr, *T_md, *T_me,
    *T_ti, *T_te, *T_nd, *T_as, *T_ae, *T_eA, *T_ac, *T_op;

static void
setgraphchar(void)
{
    int c, i, n;

    for (c = 0; c < 96; c++)
        gcmap[c] = (char)(c + ' ');

    if (!T_ac)
        return;

    n = strlen(T_ac);
    for (i = 0; i < n - 1; i += 2) {
        c = (unsigned)T_ac[i] - ' ';
        if (c >= 0 && c < 96)
            gcmap[c] = T_ac[i + 1];
    }
}

#define GETSTR(v, s)               \
    {                              \
        v = pt;                    \
        suc = tgetstr(s, &pt);     \
        if (!suc)                  \
            v = "";                \
        else                       \
            v = allocStr(suc).ptr; \
    }

void getTCstr(void)
{
    char* ent;
    char* suc;
    char* pt = funcstr;

    if (!(ent = getenv("TERM"))) {
        fprintf(stderr, "TERM is not set\n");
        reset_error_exit(0);
    }

    int r = tgetent(bp, ent);
    if (r != 1) {
        /* Can't find termcap entry */
        fprintf(stderr, "Can't find termcap entry %s\n", ent);
        reset_error_exit(0);
    }

    GETSTR(T_ce, "ce"); /* clear to the end of line */
    GETSTR(T_cd, "cd"); /* clear to the end of display */
    GETSTR(T_kr, "nd"); /* cursor right */
    if (suc == NULL)
        GETSTR(T_kr, "kr");
    if (tgetflag("bs"))
        T_kl = "\b"; /* cursor left */
    else {
        GETSTR(T_kl, "le");
        if (suc == NULL)
            GETSTR(T_kl, "kb");
        if (suc == NULL)
            GETSTR(T_kl, "kl");
    }
    GETSTR(T_cr, "cr"); /* carriage return */
    GETSTR(T_ta, "ta"); /* tab */
    GETSTR(T_sc, "sc"); /* save cursor */
    GETSTR(T_rc, "rc"); /* restore cursor */
    GETSTR(T_so, "so"); /* standout mode */
    GETSTR(T_se, "se"); /* standout mode end */
    GETSTR(T_us, "us"); /* underline mode */
    GETSTR(T_ue, "ue"); /* underline mode end */
    GETSTR(T_md, "md"); /* bold mode */
    GETSTR(T_me, "me"); /* bold mode end */
    GETSTR(T_cl, "cl"); /* clear screen */
    GETSTR(T_cm, "cm"); /* cursor move */
    GETSTR(T_al, "al"); /* append line */
    GETSTR(T_sr, "sr"); /* scroll reverse */
    GETSTR(T_ti, "ti"); /* terminal init */
    GETSTR(T_te, "te"); /* terminal end */
    GETSTR(T_nd, "nd"); /* move right one space */
    GETSTR(T_eA, "eA"); /* enable alternative charset */
    GETSTR(T_as, "as"); /* alternative (graphic) charset start */
    GETSTR(T_ae, "ae"); /* alternative (graphic) charset end */
    GETSTR(T_ac, "ac"); /* graphics charset pairs */
    GETSTR(T_op, "op"); /* set default color pair to its original value */

    tty_update_size();
    setgraphchar();
}

static int write1(const char c)
{
    putc(c, ttyf);
#ifdef SCREEN_DEBUG
    flush_tty();
#endif /* SCREEN_DEBUG */
    return 0;
}

static void
writestr(const char* s)
{
    tputs(s, 1, write1);
}

int tty_init(void)
{
    if (set_tty() < 0)
        return -1;
    set_int();
    getTCstr();
    if (T_ti && use_ti_te)
        writestr(T_ti);
    // setupscreen();
    return 0;
}

void tty_flush(void)
{
    if (ttyf)
        fflush(ttyf);
}

void tty_bell(void)
{
    write1(7);
}

static void
set_cc(int spec, int val)
{
    struct termios ioval;

    tcgetattr(tty, &ioval);
    ioval.c_cc[spec] = val;
    while (tcsetattr(tty, TCSANOW, &ioval) == -1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred: errno=%d\n", errno);
        reset_error_exit(SIGNAL_ARGLIST);
    }
}

void tty_crmode(void)
{
    ttymode_reset(ICANON, IXON);
    ttymode_set(ISIG, 0);
    set_cc(VMIN, 1);
}

void tty_noecho(void)
{
    ttymode_reset(ECHO, 0);
}

#define TTY_MODE ISIG | ICANON | ECHO | IEXTEN
void tty_raw(void)
{
    ttymode_reset(TTY_MODE, IXON | IXOFF);
    set_cc(VMIN, 1);
}

void tty_cooked(void)
{
    ttymode_set(TTY_MODE, 0);
    set_cc(VMIN, 4);
}

void tty_cbreak(void)
{
    tty_cooked();
    tty_noecho();
}

void tty_title(const char* s)
{
    if (!fmInitialized)
        return;
    if (title_str != NULL) {
        /*
         * TODO(chimera lover):
         * broken, should rm once SUPPORT_WIN9X_CONSOLE_MBCS and
         * TERM=cygwin special handle announced deprecation and
         * no one complains
         */
        fprintf(ttyf, title_str, s);
    }
}

char tty_getch(void)
{
    char c;

    while (
        read(tty, &c, 1)
        < (int)1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        /* error happend on read(2) */
        quitfm();
        break; /* unreachable */
    }
    return c;
}

static void
skip_escseq(void)
{
    int c = tty_getch();
    if (c == '[' || c == 'O') {
        c = tty_getch();
        while (IS_DIGIT(c))
            c = tty_getch();
    }
}

int tty_sleep_till_anykey(int sec, int purge)
{
    fd_set rfd;
    struct timeval tim;
    int er, c, ret;
    struct termios ioval;

    tcgetattr(tty, &ioval);
    tty_raw();

    tim.tv_sec = sec;
    tim.tv_usec = 0;

    FD_ZERO(&rfd);
    FD_SET(tty, &rfd);

    ret = select(tty + 1, &rfd, 0, 0, &tim);
    if (ret > 0 && purge) {
        c = tty_getch();
        if (c == ESC_CODE)
            skip_escseq();
    }
    er = tcsetattr(tty, TCSANOW, &ioval);
    if (er == -1) {
        printf("Error occurred: errno=%d\n", errno);
        reset_error_exit(SIGNAL_ARGLIST);
    }
    return ret;
}

void ttymode_set(int mode, int imode)
{
    struct termios ioval;

    tcgetattr(tty, &ioval);
    ioval.c_lflag |= mode;
    ioval.c_iflag |= imode;

    while (tcsetattr(tty, TCSANOW, &ioval) == -1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred while set %x: errno=%d\n", mode, errno);
        reset_error_exit(SIGNAL_ARGLIST);
    }
}

void ttymode_reset(int mode, int imode)
{
    struct termios ioval;

    tcgetattr(tty, &ioval);
    ioval.c_lflag &= ~mode;
    ioval.c_iflag &= ~imode;

    while (tcsetattr(tty, TCSANOW, &ioval) == -1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred while reset %x: errno=%d\n", mode, errno);
        reset_error_exit(SIGNAL_ARGLIST);
    }
}

void tty_close(void)
{
    if (tty > 2)
        close(tty);
}

const char* tty_name(void)
{
    return ttyname(tty);
}

void tty_reset(void)
{
    writestr(T_op); /* turn off */
    writestr(T_me);
    if (use_ti_te) {
        if (T_te && *T_te)
            writestr(T_te);
        else
            writestr(T_cl);
    }
    writestr(T_se); /* reset terminal */
    tty_flush();
    tcsetattr(tty, TCSANOW, &d_ioval);
    if (tty != 2)
        tty_close();
}

int get_pixel_per_cell(int* ppc, int* ppl)
{
    fd_set rfd;
    struct timeval tval;
    char buf[100];
    char* p;
    ssize_t len;
    ssize_t left;
    int wp, hp, wc, hc;
    int i;

    fputs("\x1b[14t\x1b[18t", ttyf);
    tty_flush();

    p = buf;
    left = sizeof(buf) - 1;
    for (i = 0; i < 10; i++) {
        tval.tv_usec = 200000; /* 0.2 sec * 10 */
        tval.tv_sec = 0;
        FD_ZERO(&rfd);
        FD_SET(tty, &rfd);
        if (select(tty + 1, &rfd, NULL, NULL, &tval) <= 0 || !FD_ISSET(tty, &rfd))
            continue;

        if ((len = read(tty, p, left)) <= 0)
            continue;
        p[len] = '\0';

        if (sscanf(buf, "\x1b[4;%d;%dt\x1b[8;%d;%dt", &hp, &wp, &hc, &wc) == 4) {
            if (wp > 0 && wc > 0 && hp > 0 && hc > 0) {
                *ppc = wp / wc;
                *ppl = hp / hc;
                return 1;
            } else {
                return 0;
            }
        }
        p += len;
        left -= len;
    }

    return 0;
}

/*
 * From the Kitty spec,
 * <https://sw.kovidgoyal.net/kitty/graphics-protocol/#remote-client>:
 *
 * > The pixel data must first be base64 encoded then chunked up into
 * > chunks no larger than 4096 bytes.
 *
 * base64 inflates the size by 4/3, hence we read 3072.
 *
 * For simplicity we use the same size everywhere.
 */
#define IMG_BUF_SZ 3072
#define MOVE(line, column) writestr(tgoto(T_cm, column, line));

void put_image_osc5379(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh)
{
    pStr buf;
    char* size;

    if (w > 0 && h > 0)
        size = Sprintf("%dx%d", w, h)->ptr;
    else
        size = "";

    MOVE(y, x);
    buf = Sprintf("\x1b]5379;show_picture %s %s %dx%d+%d+%d\x07", url, size, sw, sh, sx, sy);
    writestr(buf->ptr);
    MOVE(Currentbuf->cursorY, Currentbuf->cursorX);
}

void put_image_iterm2(const char* url, int x, int y, int w, int h)
{
    pStr buf;
    char cbuf[IMG_BUF_SZ];
    FILE* fp;
    int n;
    struct stat st;

    if (stat(url, &st))
        return;

    if (!(fp = fopen(url, "r")))
        return;

    buf = Sprintf("\x1b]1337;"
                  "File="
                  "name=%s;"
                  "size=%ld;"
                  "width=%d;"
                  "height=%d;"
                  "preserveAspectRatio=0;"
                  "inline=1"
                  ":",
        url, st.st_size, w, h);

    MOVE(y, x);

    writestr(buf->ptr);

    while ((n = fread(cbuf, 1, IMG_BUF_SZ, fp)))
        writestr(base64_encode(cbuf, n)->ptr);

    fclose(fp);
    writestr("\a");
    MOVE(Currentbuf->cursorY, Currentbuf->cursorX);
}

void put_image_kitty(const char* url, int x, int y, int w, int h, int sx, int sy, int sw,
    int sh, int cols, int rows)
{
    pStr buf, base64;
    char* tmpf;
    char cbuf[IMG_BUF_SZ];
    char* argv[4];
    const char* type;
    FILE* fp;
    int n, m, is_anim;
    struct stat st;
    pid_t pid;
    void (*volatile previntr)(SIGNAL_ARG);
    void (*volatile prevquit)(SIGNAL_ARG);
    void (*volatile prevstop)(SIGNAL_ARG);

    type = guessContentType(url);

    /* convert to PNG, so that we transfer as little data as possible. */
    if (!type || strcasecmp(type, "image/png")) {
        tmpf = Sprintf("%s/%s.png", tmp_dir, mybasename(url))->ptr;
        is_anim = type && !strcasecmp(type, "image/gif");

        /* convert only if png doesn't exist yet. */
        if (stat(tmpf, &st)) {
            if (stat(url, &st))
                return;

            tty_flush();

            previntr = mySignal(SIGINT, SIG_IGN);
            prevquit = mySignal(SIGQUIT, SIG_IGN);
            prevstop = mySignal(SIGTSTP, SIG_IGN);

            if ((pid = fork()) == 0) {
                close(STDERR_FILENO); /* Don't output error message. */
                ttymode_set(ISIG, 0);

                if (!(argv[0] = getenv("W3M_KITTY_TO_PNG")))
                    argv[0] = "convert";

                if (is_anim) {
                    buf = Strnew_charp(url);
                    Strcat_charp(buf, "[0]");
                    argv[1] = buf->ptr;
                } else {
                    argv[1] = allocStr(url).ptr;
                }
                argv[2] = tmpf;
                argv[3] = NULL;
                execvp(argv[0], argv);
                exit(0);
            } else if (pid > 0) {
                waitpid(pid, &n, 0);
                ttymode_reset(ISIG, 0);
                mySignal(SIGINT, previntr);
                mySignal(SIGQUIT, prevquit);
                mySignal(SIGTSTP, prevstop);
            }

            pushTmpFile(tmpf);
        }
        url = tmpf;
    }

    if (!(fp = fopen(url, "r")))
        return;

    MOVE(y, x);

    n = fread(cbuf, 1, IMG_BUF_SZ, fp);
    base64 = base64_encode(cbuf, n);

    m = n == IMG_BUF_SZ; /* m=1 -> has more, m=0 -> finished */
    /* 100 is format = PNG in Kitty */
    buf = Sprintf("\x1b_Gf=100,a=T,s=%d,v=%d,m=%d,x=%d,y=%d,w=%d,h=%d,c=%d,r=%d"
                  ";%s\x1b\\",
        w, h, m, sx, sy, sw, sh, cols, rows, base64->ptr);
    writestr(buf->ptr);

    while ((n = fread(cbuf, 1, sizeof cbuf, fp))) {
        m = n == IMG_BUF_SZ;
        base64 = base64_encode(cbuf, n);
        buf = Sprintf("\x1b_Gm=%d;%s\x1b\\", m, base64->ptr);
        writestr(buf->ptr);
    }

    fclose(fp);
    MOVE(Currentbuf->cursorY, Currentbuf->cursorX);
}

static void
save_gif(const char* path, unsigned char* header, size_t header_size, unsigned char* body, size_t body_size)
{
    int fd;

    if ((fd = open(path, O_WRONLY | O_CREAT, 0600)) >= 0) {
        write(fd, header, header_size);
        write(fd, body, body_size);
        write(fd, "\x3b", 1);
        close(fd);
    }
}

static unsigned char*
skip_gif_header(unsigned char* p)
{
    /* Header */
    p += 10;

    if (*(p) & 0x80) {
        p += (3 * (2 << ((*p) & 0x7)));
    }
    p += 3;

    return p;
}

static pStr
save_first_animation_frame(const char* path)
{
    int fd;
    struct stat st;
    unsigned char* header;
    size_t header_size;
    unsigned char* body;
    unsigned char* p;
    ssize_t len;
    pStr new_path;

    new_path = Strnew_charp(path);
    Strcat_charp(new_path, "-1");
    if (stat(new_path->ptr, &st) == 0) {
        return new_path;
    }

    if ((fd = open(path, O_RDONLY)) < 0) {
        return NULL;
    }

    if (fstat(fd, &st) != 0 || !(header = GC_malloc(st.st_size))) {
        close(fd);
        return NULL;
    }

    len = read(fd, header, st.st_size);
    close(fd);

    /* Header */

    if (len != st.st_size || strncmp((char*)header, "GIF89a", 6) != 0) {
        return NULL;
    }

    p = skip_gif_header(header);
    header_size = p - header;

    /* Application Extension */
    if (p[0] == 0x21 && p[1] == 0xff) {
        p += 19;
    }

    /* Other blocks */
    body = NULL;
    while (p + 2 < header + st.st_size) {
        if (*(p++) == 0x21 && *(p++) == 0xf9 && *(p++) == 0x04) {
            if (body) {
                /* Graphic Control Extension */
                save_gif(new_path->ptr, header, header_size, body, p - 3 - body);
                return new_path;
            } else {
                /* skip the first frame. */
            }
            body = p - 3;
        }
    }

    return NULL;
}

void put_image_sixel(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int n_terminal_image)
{
    pid_t pid;
    int do_anim;
    void (*volatile previntr)(SIGNAL_ARG);
    void (*volatile prevquit)(SIGNAL_ARG);
    void (*volatile prevstop)(SIGNAL_ARG);

    MOVE(y, x);
    tty_flush();

    do_anim = (n_terminal_image == 1 && x == 0 && y == 0 && sx == 0 && sy == 0);

    previntr = mySignal(SIGINT, SIG_IGN);
    prevquit = mySignal(SIGQUIT, SIG_IGN);
    prevstop = mySignal(SIGTSTP, SIG_IGN);

    if ((pid = fork()) == 0) {
        char* env;
        int n = 0;
        char* argv[20];
        char digit[2][11 + 1];
        char clip[44 + 3 + 1];
        pStr str_url;

        close(STDERR_FILENO); /* Don't output error message. */
        if (do_anim) {
            writestr("\x1b[?80h");
        } else if (!strstr(url, "://") && strcmp(url + strlen(url) - 4, ".gif") == 0 && (str_url = save_first_animation_frame(url))) {
            url = str_url->ptr;
        }
        ttymode_set(ISIG, 0);

        if ((env = getenv("W3M_IMG2SIXEL"))) {
            char* p;
            env = Strnew_charp(env)->ptr;
            while (n < 8 && (p = strchr(env, ' '))) {
                *p = '\0';
                if (*env != '\0') {
                    argv[n++] = env;
                }
                env = p + 1;
            }
            if (*env != '\0') {
                argv[n++] = env;
            }
        } else {
            argv[n++] = "img2sixel";
        }
        argv[n++] = "-l";
        argv[n++] = do_anim ? "auto" : "disable";
        argv[n++] = "-w";
        sprintf(digit[0], "%d", w);
        argv[n++] = digit[0];
        argv[n++] = "-h";
        sprintf(digit[1], "%d", h);
        argv[n++] = digit[1];
        argv[n++] = "-c";
        sprintf(clip, "%dx%d+%d+%d", sw, sh, sx, sy);
        argv[n++] = clip;
        argv[n++] = allocStr(url).ptr;
        if (getenv("TERM") && strcmp(getenv("TERM"), "screen") == 0 && (!getenv("SCREEN_VARIANT") || strcmp(getenv("SCREEN_VARIANT"), "sixel") != 0)) {
            argv[n++] = "-P";
        }
        argv[n++] = NULL;
        execvp(argv[0], argv);
        exit(0);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        ttymode_reset(ISIG, 0);
        mySignal(SIGINT, previntr);
        mySignal(SIGQUIT, prevquit);
        mySignal(SIGTSTP, prevstop);
        if (do_anim) {
            writestr("\x1b[?80l");
        }
    }

    MOVE(Currentbuf->cursorY, Currentbuf->cursorX);
}
