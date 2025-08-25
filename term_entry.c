#include "term_entry.h"
#include "Str.h"
#include "indep.h"
#include "tty.h"
#include "config.h"
#include "textlist.h"
#include "fm.h"

// base64_encode
#include "etc.h"

// guessContentType
#include "url.h"

// tmp_dir
#include "rc.h"

#include <stdlib.h>

// tput
#include <sys/wait.h>
#include <term.h>

#include <sys/stat.h>

// fork
#include <unistd.h>

static char bp[1024], funcstr[256];

enum GrahicCharType UseGraphicChar = GRAPHIC_CHAR_CHARSET;
int Do_not_use_ti_te = FALSE;

char *T_cd, *T_ce, *T_kr, *T_kl, *T_cr, *T_bt, *T_ta, *T_sc, *T_rc,
    *T_so, *T_se, *T_us, *T_ue, *T_cl, *T_cm, *T_al, *T_sr, *T_md, *T_me,
    *T_ti, *T_te, *T_nd, *T_as, *T_ae, *T_eA, *T_ac, *T_op;

#define GETSTR(v, s)               \
    {                              \
        v = pt;                    \
        suc = tgetstr(s, &pt);     \
        if (!suc)                  \
            v = "";                \
        else                       \
            v = allocStr(suc, -1); \
    }

static char gcmap[96];

char graphchar(char c)
{
    return (((unsigned)(c) >= ' ' && (unsigned)(c) < 128) ? gcmap[(c) - ' '] : (c));
}

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

void getTCstr(void)
{
    char* ent;
    char* suc;
    char* pt = funcstr;
    int r;

    ent = getenv("TERM") ? getenv("TERM") : DEFAULT_TERM;
    if (ent == NULL) {
        fprintf(stderr, "TERM is not set\n");
        // reset_error_exit(SIGNAL_ARGLIST);
        abort();
    }

    r = tgetent(bp, ent);
    if (r != 1) {
        /* Can't find termcap entry */
        fprintf(stderr, "Can't find termcap entry %s\n", ent);
        // reset_error_exit(SIGNAL_ARGLIST);
        abort();
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
#if defined(CYGWIN) && CYGWIN < 1
    /* for TERM=pcansi on MS-DOS prompt. */
    T_eA = "";
    T_as = "";
    T_ae = "";
    T_ac = "";
#endif /* CYGWIN */

    setgraphchar();
}

bool graph_ok()
{
    if (UseGraphicChar != GRAPHIC_CHAR_DEC)
        return 0;
    return T_as[0] != 0 && T_ae[0] != 0 && T_ac[0] != 0;
}

void initTerm()
{
    getTCstr();
    if (T_ti && !Do_not_use_ti_te)
        writestr(T_ti);
}

void resetTerm(void)
{
    writestr(T_op); /* turn off */
    writestr(T_me);
    if (!Do_not_use_ti_te) {
        if (T_te && *T_te)
            writestr(T_te);
        else
            writestr(T_cl);
    }
    writestr(T_se); /* reset terminal */
}

void write_T_op() { writestr(T_op); }
void write_T_ce() { writestr(T_ce); }
void write_T_ae() { writestr(T_ae); }
void write_T_me() { writestr(T_me); }
void write_T_nd() { writestr(T_nd); }
void write_T_so() { writestr(T_so); }
void write_T_us() { writestr(T_us); }
void write_T_md() { writestr(T_md); }
void write_T_eA() { writestr(T_eA); }
void write_T_as() { writestr(T_as); }
void write_T_cl() { writestr(T_cl); }

void MOVE(int line, int column)
{
    writestr(tgoto(T_cm, column, line));
}

void put_image_osc5379(int cursorX, int cursorY,
    char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh)
{
    Str buf;
    char* size;

    if (w > 0 && h > 0)
        size = Sprintf("%dx%d", w, h)->ptr;
    else
        size = "";

    MOVE(y, x);
    buf = Sprintf("\x1b]5379;show_picture %s %s %dx%d+%d+%d\x07", url, size, sw, sh, sx, sy);
    writestr(buf->ptr);
    MOVE(cursorY, cursorX);
}

void put_image_iterm2(int cursorX, int cursorY,
    char* url, int x, int y, int w, int h)
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

    MOVE(y, x);

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
    MOVE(cursorY, cursorX);
}

void put_image_kitty(int cursorX, int cursorY,
    char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int cols, int rows)
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

            // previntr = mySignal(SIGINT, SIG_IGN);
            // prevquit = mySignal(SIGQUIT, SIG_IGN);
            // prevstop = mySignal(SIGTSTP, SIG_IGN);

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
                // mySignal(SIGINT, previntr);
                // mySignal(SIGQUIT, prevquit);
                // mySignal(SIGTSTP, prevstop);
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

    MOVE(y, x);

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
    MOVE(cursorY, cursorX);
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

void put_image_sixel(int cursorX, int cursorY,
    char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int n_terminal_image)
{
    pid_t pid;
    int do_anim;
    MySignalHandler (*volatile previntr)(SIGNAL_ARG);
    MySignalHandler (*volatile prevquit)(SIGNAL_ARG);
    MySignalHandler (*volatile prevstop)(SIGNAL_ARG);

    MOVE(y, x);
    flush_tty();

    do_anim = (n_terminal_image == 1 && x == 0 && y == 0 && sx == 0 && sy == 0);

    // previntr = mySignal(SIGINT, SIG_IGN);
    // prevquit = mySignal(SIGQUIT, SIG_IGN);
    // prevstop = mySignal(SIGTSTP, SIG_IGN);

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

    MOVE(cursorY, cursorX);
}
