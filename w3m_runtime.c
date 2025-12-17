#include "w3m_runtime.h"
#include "image.h"
#include "terms.h"
#include "config.h"
#include "Str.h"
#include "indep.h"
#include "etc.h"
#include "fm.h"
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <errno.h>
#include <termcap.h>
#include <unistd.h>

static struct termios d_ioval;

struct Runtime g_runtime = {
    .lines = 0,
    .cols = 0,
    .fmInitialized = false,
    .Do_not_use_ti_te = false,
};
struct Runtime* getRuntime()
{
    return &g_runtime;
}
bool fmInitialized() { return g_runtime.fmInitialized; }
#define MAXIMUM_COLS 1024
void tty_set_cols(int cols)
{
    g_runtime.cols = cols;
    if (g_runtime.cols > MAXIMUM_COLS) {
        g_runtime.cols = MAXIMUM_COLS;
    }
}

void enterRawMode(void)
{
    if (!g_runtime.fmInitialized) {
        initscr();
        term_raw();
        term_noecho();
        initImage();
    }
    g_runtime.fmInitialized = TRUE;
}

void exitRawMode(void)
{
    if (g_runtime.fmInitialized) {
        move(LASTLINE(), 0);
        clrtoeolx();
        refresh();
        loadImage(NULL, IMG_FLAG_STOP);
        reset_tty();
        g_runtime.fmInitialized = FALSE;
    }
}

static MySignalHandler reset_exit_with_value(SIGNAL_ARG, int rval)
{
    reset_tty();
    w3m_exit(rval);
    SIGNAL_RETURN;
}

MySignalHandler reset_error_exit(SIGNAL_ARG)
{
    reset_exit_with_value(SIGNAL_ARGLIST, 1);
}

MySignalHandler
reset_exit(SIGNAL_ARG)
{
    reset_exit_with_value(SIGNAL_ARGLIST, 0);
}

MySignalHandler
error_dump(SIGNAL_ARG)
{
    mySignal(SIGIOT, SIG_DFL);
    reset_tty();
    abort();
    SIGNAL_RETURN;
}

void set_int(void)
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

static void
setgraphchar(void)
{
    for (int c = 0; c < 96; c++)
        g_runtime.gcmap[c] = (char)(c + ' ');

    if (!getRuntime()->T_ac)
        return;

    int n = strlen(getRuntime()->T_ac);
    for (int i = 0; i < n - 1; i += 2) {
        int c = (unsigned)getRuntime()->T_ac[i] - ' ';
        if (c >= 0 && c < 96)
            g_runtime.gcmap[c] = getRuntime()->T_ac[i + 1];
    }
}

char graphchar(char c)
{
    return (((unsigned)(c) >= ' ' && (unsigned)(c) < 128) ? g_runtime.gcmap[(c) - ' '] : (c));
}

#define GETSTR(v, s)               \
    {                              \
        v = pt;                    \
        suc = tgetstr(s, &pt);     \
        if (!suc)                  \
            v = "";                \
        else                       \
            v = allocStr(suc, -1); \
    }

void setlinescols(void)
{
    struct winsize wins;
    int i = ioctl(g_runtime.tty_input, TIOCGWINSZ, &wins);
    if (i >= 0 && wins.ws_row != 0 && wins.ws_col != 0) {
        g_runtime.lines = wins.ws_row;
        g_runtime.cols = wins.ws_col;
    }
}

static char bp[1024], funcstr[256];

static void getTCstr(void)
{
    char* ent;
    char* suc;
    char* pt = funcstr;
    int r;

    ent = getenv("TERM") ? getenv("TERM") : DEFAULT_TERM;
    if (ent == NULL) {
        fprintf(stderr, "TERM is not set\n");
        reset_error_exit(SIGNAL_ARGLIST);
    }

    r = tgetent(bp, ent);
    if (r != 1) {
        /* Can't find termcap entry */
        fprintf(stderr, "Can't find termcap entry %s\n", ent);
        reset_error_exit(SIGNAL_ARGLIST);
    }

    GETSTR(g_runtime.T_ce, "ce"); /* clear to the end of line */
    GETSTR(g_runtime.T_cd, "cd"); /* clear to the end of display */
    GETSTR(g_runtime.T_kr, "nd"); /* cursor right */
    if (suc == NULL)
        GETSTR(g_runtime.T_kr, "kr");
    if (tgetflag("bs"))
        g_runtime.T_kl = "\b"; /* cursor left */
    else {
        GETSTR(g_runtime.T_kl, "le");
        if (suc == NULL)
            GETSTR(g_runtime.T_kl, "kb");
        if (suc == NULL)
            GETSTR(g_runtime.T_kl, "kl");
    }
    GETSTR(g_runtime.T_cr, "cr"); /* carriage return */
    GETSTR(g_runtime.T_ta, "ta"); /* tab */
    GETSTR(g_runtime.T_sc, "sc"); /* save cursor */
    GETSTR(g_runtime.T_rc, "rc"); /* restore cursor */
    GETSTR(g_runtime.T_so, "so"); /* standout mode */
    GETSTR(g_runtime.T_se, "se"); /* standout mode end */
    GETSTR(g_runtime.T_us, "us"); /* underline mode */
    GETSTR(g_runtime.T_ue, "ue"); /* underline mode end */
    GETSTR(g_runtime.T_md, "md"); /* bold mode */
    GETSTR(g_runtime.T_me, "me"); /* bold mode end */
    GETSTR(g_runtime.T_cl, "cl"); /* clear screen */
    GETSTR(g_runtime.T_cm, "cm"); /* cursor move */
    GETSTR(g_runtime.T_al, "al"); /* append line */
    GETSTR(g_runtime.T_sr, "sr"); /* scroll reverse */
    GETSTR(g_runtime.T_ti, "ti"); /* terminal init */
    GETSTR(g_runtime.T_te, "te"); /* terminal end */
    GETSTR(g_runtime.T_nd, "nd"); /* move right one space */
    GETSTR(g_runtime.T_eA, "eA"); /* enable alternative charset */
    GETSTR(g_runtime.T_as, "as"); /* alternative (graphic) charset start */
    GETSTR(g_runtime.T_ae, "ae"); /* alternative (graphic) charset end */
    GETSTR(g_runtime.T_ac, "ac"); /* graphics charset pairs */
    GETSTR(g_runtime.T_op, "op"); /* set default color pair to its original value */
#if defined(CYGWIN) && CYGWIN < 1
    /* for TERM=pcansi on MS-DOS prompt. */
#if 0
    T_eA = "";
    T_as = "\033[12m";
    T_ae = "\033[10m";
    T_ac = "l\001k\002m\003j\004x\005q\006n\020a\024v\025w\026u\027t\031";
#endif
    T_eA = "";
    T_as = "";
    T_ae = "";
    T_ac = "";
#endif /* CYGWIN */

    setlinescols();
    setgraphchar();
}

void init_tty()
{
    // stdin
    g_runtime.tty_input = 0;

    // stdout
    g_runtime.tty_output_f = stdout;

    tcgetattr(g_runtime.tty_input, &d_ioval);

    getTCstr();
}

int write1(int c)
{
    putc(c, g_runtime.tty_output_f);
    return 0;
}

void writestr(const char* s)
{
    tputs(s, 1, write1);
}

char* ttyname_tty(void)
{
    return ttyname(g_runtime.tty_input);
}

void reset_tty(void)
{
    writestr(g_runtime.T_op); /* turn off */
    writestr(g_runtime.T_me);
    if (!g_runtime.Do_not_use_ti_te) {
        if (g_runtime.T_te && *g_runtime.T_te)
            writestr(g_runtime.T_te);
        else
            writestr(g_runtime.T_cl);
    }
    writestr(g_runtime.T_se); /* reset terminal */
    flush_tty();
    tcsetattr(g_runtime.tty_input, TCSANOW, &d_ioval);
}

void ttymode_set(int mode, int imode)
{
    struct termios ioval;
    tcgetattr(g_runtime.tty_input, &ioval);
    ioval.c_lflag |= mode;
    ioval.c_iflag |= imode;
    while (tcsetattr(g_runtime.tty_input, TCSANOW, &ioval) == -1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred while set %x: errno=%d\n", mode, errno);
        reset_error_exit(SIGNAL_ARGLIST);
    }
}

void ttymode_reset(int mode, int imode)
{
    struct termios ioval;
    tcgetattr(g_runtime.tty_input, &ioval);
    ioval.c_lflag &= ~mode;
    ioval.c_iflag &= ~imode;
    while (tcsetattr(g_runtime.tty_input, TCSANOW, &ioval) == -1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred while reset %x: errno=%d\n", mode, errno);
        reset_error_exit(SIGNAL_ARGLIST);
    }
}

void set_cc(int spec, int val)
{
    struct termios ioval;
    tcgetattr(g_runtime.tty_input, &ioval);
    ioval.c_cc[spec] = val;
    while (tcsetattr(g_runtime.tty_input, TCSANOW, &ioval) == -1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred: errno=%d\n", errno);
        reset_error_exit(SIGNAL_ARGLIST);
    }
}

void quitfm(void);

char getch(void)
{
    char c;

    while (
        read(getRuntime()->tty_input, &c, 1)
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
    int c = getch();
    if (c == '[' || c == 'O') {
        c = getch();
        while (IS_DIGIT(c))
            c = getch();
    }
}

// int sleep_till_anykey(int sec, bool purge)
// {
//     struct termios ioval;
//     tcgetattr(getRuntime()->tty_input, &ioval);
//     term_raw();
//
//     struct timeval tim;
//     tim.tv_sec = sec;
//     tim.tv_usec = 0;
//
//     fd_set rfd;
//     FD_ZERO(&rfd);
//     FD_SET(getRuntime()->tty_input, &rfd);
//
//     int ret = select(getRuntime()->tty_input + 1, &rfd, 0, 0, &tim);
//     if (ret > 0 && purge) {
//         int c = getch();
//         if (c == ESC_CODE)
//             skip_escseq();
//     }
//     int er = tcsetattr(getRuntime()->tty_input, TCSANOW, &ioval);
//     if (er == -1) {
//         printf("Error occurred: errno=%d\n", errno);
//         reset_error_exit(SIGNAL_ARGLIST);
//     }
//     return ret;
// }

void flush_tty(void)
{
    if (g_runtime.tty_output_f)
        fflush(g_runtime.tty_output_f);
}

void tty_MOVE(int line, int column)
{
    writestr(tgoto(g_runtime.T_cm, column, line));
}

void put_image_osc5379(char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh)
{
    Str buf;
    char* size;

    if (w > 0 && h > 0)
        size = Sprintf("%dx%d", w, h)->ptr;
    else
        size = "";

    tty_MOVE(y, x);
    buf = Sprintf("\x1b]5379;show_picture %s %s %dx%d+%d+%d\x07", url, size, sw, sh, sx, sy);
    writestr(buf->ptr);
    tty_MOVE(Currentbuf->cursorY, Currentbuf->cursorX);
}

void put_image_iterm2(char* url, int x, int y, int w, int h)
{
    Str buf;
    char* cbuf;
    FILE* fp;
    int c, i;
    struct stat st;

    if (stat(url, &st))
        return;

    fp = fopen(url, "r");
    if (!fp)
        return;

    buf = Sprintf("\x1b]1337;"
                  "File="
                  "name=%s;"
                  "size=%d;"
                  "width=%d;"
                  "height=%d;"
                  "preserveAspectRatio=0;"
                  "inline=1"
                  ":",
        url, st.st_size, w, h);

    tty_MOVE(y, x);

    writestr(buf->ptr);

    cbuf = GC_MALLOC_ATOMIC(3072);
    if (!cbuf)
        goto cleanup;
    i = 0;
    while ((c = fgetc(fp)) != EOF) {
        cbuf[i++] = c;
        if (i == 3072) {
            buf = base64_encode(cbuf, i);
            writestr(buf->ptr);
            i = 0;
        }
    }

    if (i) {
        buf = base64_encode(cbuf, i);
        writestr(buf->ptr);
    }

cleanup:
    fclose(fp);
    writestr("\a");
    tty_MOVE(Currentbuf->cursorY, Currentbuf->cursorX);
}

void put_image_kitty(char* url, int x, int y, int w, int h, int sx, int sy, int sw,
    int sh, int cols, int rows)
{
    Str buf, base64;
    char *cbuf, *type, *tmpf;
    char* argv[4];
    FILE* fp;
    int c, i, j, m, t, is_anim;
    struct stat st;
    pid_t pid;
    MySignalHandler (*volatile previntr)(SIGNAL_ARG);
    MySignalHandler (*volatile prevquit)(SIGNAL_ARG);
    MySignalHandler (*volatile prevstop)(SIGNAL_ARG);

    if (!url)
        return;

    type = guessContentType(url);
    t = 100; /* always convert to png for now. */

    if (!(type && !strcasecmp(type, "image/png"))) {
        tmpf = Sprintf("%s/%s.png", tmp_dir, mybasename(url))->ptr;

        if (type && !strcasecmp(type, "image/gif")) {
            is_anim = 1;
        } else {
            is_anim = 0;
        }

        /* convert only if png doesn't exist yet. */

        if (stat(tmpf, &st)) {
            if (stat(url, &st))
                return;

            flush_tty();

            previntr = mySignal(SIGINT, SIG_IGN);
            prevquit = mySignal(SIGQUIT, SIG_IGN);
            prevstop = mySignal(SIGTSTP, SIG_IGN);

            if ((pid = fork()) == 0) {
                i = 0;

                close(STDERR_FILENO); /* Don't output error message. */
                ttymode_set(ISIG, 0);

                if ((cbuf = getenv("W3M_KITTY_TO_PNG")))
                    argv[i++] = cbuf;
                else
                    argv[i++] = "convert";

                if (is_anim) {
                    buf = Strnew_charp(url);
                    Strcat_charp(buf, "[0]");
                    argv[i++] = buf->ptr;
                } else {
                    argv[i++] = url;
                }
                argv[i++] = tmpf;
                argv[i++] = NULL;
                execvp(argv[0], argv);
                exit(0);
            } else if (pid > 0) {
                waitpid(pid, &i, 0);
                ttymode_reset(ISIG, 0);
                mySignal(SIGINT, previntr);
                mySignal(SIGQUIT, prevquit);
                mySignal(SIGTSTP, prevstop);
            }

            pushText(fileToDelete, tmpf);
        }
        url = tmpf;
    }

    if (stat(url, &st))
        return;

    fp = fopen(url, "r");
    if (!fp)
        return;

    tty_MOVE(y, x);

    cbuf = GC_MALLOC_ATOMIC(3072); /* base64-encoded chunks of 4096 bytes */
    if (!cbuf)
        goto cleanup;
    i = 0;

    while (i < 3072 && (c = fgetc(fp)) != EOF)
        cbuf[i++] = c;

    base64 = base64_encode(cbuf, i);

    if (c == EOF)
        m = 0;
    else
        m = 1;
    buf = Sprintf("\x1b_Gf=%d,s=%d,v=%d,a=T,m=%d,x=%d,y=%d,w=%d,h=%d,c=%d,r=%d;"
                  "%s\x1b\\",
        t, w, h, m, sx, sy, sw, sh, cols, rows, base64->ptr);
    writestr(buf->ptr);

    if (m) {
        i = 0;
        j = 0;
        while ((c = fgetc(fp)) != EOF) {
            if (j) {
                base64 = base64_encode(cbuf, i);
                buf = Sprintf("\x1b_Gm=1;%s\x1b\\", base64->ptr);
                writestr(buf->ptr);
                i = 0;
                j = 0;
            }
            cbuf[i++] = c;
            if (i == 3072)
                j = 1;
        }

        if (i) {
            base64 = base64_encode(cbuf, i);
            buf = Sprintf("\x1b_Gm=0;%s\x1b\\", base64->ptr);
            writestr(buf->ptr);
        }
    }
cleanup:
    fclose(fp);
    tty_MOVE(Currentbuf->cursorY, Currentbuf->cursorX);
}

static void
save_gif(const char* path, u_char* header, size_t header_size, u_char* body, size_t body_size)
{
    int fd;

    if ((fd = open(path, O_WRONLY | O_CREAT, 0600)) >= 0) {
        write(fd, header, header_size);
        write(fd, body, body_size);
        write(fd, "\x3b", 1);
        close(fd);
    }
}

static u_char*
skip_gif_header(u_char* p)
{
    /* Header */
    p += 10;

    if (*(p) & 0x80) {
        p += (3 * (2 << ((*p) & 0x7)));
    }
    p += 3;

    return p;
}

static Str
save_first_animation_frame(const char* path)
{
    int fd;
    struct stat st;
    u_char* header;
    size_t header_size;
    u_char* body;
    u_char* p;
    ssize_t len;
    Str new_path;

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

void put_image_sixel(char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int n_terminal_image)
{
    pid_t pid;
    int do_anim;
    MySignalHandler (*volatile previntr)(SIGNAL_ARG);
    MySignalHandler (*volatile prevquit)(SIGNAL_ARG);
    MySignalHandler (*volatile prevstop)(SIGNAL_ARG);

    tty_MOVE(y, x);
    flush_tty();

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
        Str str_url;

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
        argv[n++] = url;
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

    tty_MOVE(Currentbuf->cursorY, Currentbuf->cursorX);
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

    struct winsize ws;
    if (ioctl(g_runtime.tty_input, TIOCGWINSZ, &ws) == 0 && ws.ws_ypixel > 0 && ws.ws_row > 0 && ws.ws_xpixel > 0 && ws.ws_col > 0) {
        *ppc = ws.ws_xpixel / ws.ws_col;
        *ppl = ws.ws_ypixel / ws.ws_row;
        return 1;
    }

    fputs("\x1b[14t\x1b[18t", g_runtime.tty_output_f);
    flush_tty();

    p = buf;
    left = sizeof(buf) - 1;
    for (i = 0; i < 10; i++) {
        tval.tv_usec = 200000; /* 0.2 sec * 10 */
        tval.tv_sec = 0;
        FD_ZERO(&rfd);
        FD_SET(g_runtime.tty_input, &rfd);
        if (select(g_runtime.tty_input + 1, &rfd, NULL, NULL, &tval) <= 0 || !FD_ISSET(g_runtime.tty_input, &rfd))
            continue;

        if ((len = read(g_runtime.tty_input, p, left)) <= 0)
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

int initscr(void)
{
    set_int();
    if (g_runtime.T_ti && !g_runtime.Do_not_use_ti_te)
        writestr(g_runtime.T_ti);
    setupscreen();
    return 0;
}

int graph_ok(void)
{
    if (UseGraphicChar != GRAPHIC_CHAR_DEC)
        return 0;
    return g_runtime.T_as[0] != 0 && g_runtime.T_ae[0] != 0 && g_runtime.T_ac[0] != 0;
}

void crmode(void)
{
    ttymode_reset(ICANON, IXON);
    ttymode_set(ISIG, 0);
    set_cc(VMIN, 1);
}

void nocrmode(void)
{
    ttymode_set(ICANON, 0);
    set_cc(VMIN, 4);
}

void term_echo(void)
{
    ttymode_set(ECHO, 0);
}

void term_noecho(void)
{
    ttymode_reset(ECHO, 0);
}

#define TTY_MODE ISIG | ICANON | ECHO | IEXTEN
void term_raw(void)
{
    ttymode_reset(TTY_MODE, IXON | IXOFF | INLCR | IGNCR | ICRNL);
    set_cc(VMIN, 1);
}

void term_cooked(void)
{
    ttymode_set(TTY_MODE, 0);
    set_cc(VMIN, 4);
}

void term_cbreak(void)
{
    term_cooked();
    term_noecho();
}

static const char* title_str = NULL;

void term_title(const char* s)
{
    if (!fmInitialized())
        return;
    if (title_str != NULL) {
        fprintf(getRuntime()->tty_output_f, title_str, s);
    }
}

void bell(void)
{
    write1(7);
}
