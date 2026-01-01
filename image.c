#include "image.h"
#include "input_stream.h"
#include "hash.h"
#include "terms.h"
#include "file.h"
#include "indep.h"
#include "local_cgi.h"
#include "message.h"
#include "buffer.h"
#include "anchor.h"
#include "display.h"
#include "tab.h"
#include "etc.h"
#include "w3m_rc.h"
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>

static int image_index = 0;

// display image

struct TerminalImage {
    struct ImageCache* cache;
    short x;
    short y;
    short sx;
    short sy;
    short width;
    short height;
};

static struct TerminalImage* terminal_image = NULL;
static int n_terminal_image = 0;
static int max_terminal_image = 0;
static FILE *Imgdisplay_rf = NULL, *Imgdisplay_wf = NULL;
static pid_t Imgdisplay_pid = 0;
static int openImgdisplay(void);
static void closeImgdisplay(void);
static int getCharSize(void);

void initImage()
{
    if (getRuntime()->displayImage) {
        if (getRuntime()->activeImage)
            return;
        if (getCharSize())
            getRuntime()->activeImage = TRUE;
    }
}

static int
getCharSize(void)
{
    FILE* f;
    Str tmp;
    int w = 0, h = 0;

    set_environ("W3M_TTY", ttyname_tty());

    if (getRuntime()->enable_inline_image) {
        int ppc, ppl;

        if (get_pixel_per_cell(&ppc, &ppl)) {
            getRuntime()->pixel_per_char_i = ppc;
            getRuntime()->pixel_per_line_i = ppl;
            getRuntime()->pixel_per_char = (double)ppc;
            getRuntime()->pixel_per_line = (double)ppl;
        } else {
            getRuntime()->pixel_per_char_i = (int)getRuntime()->pixel_per_char;
            getRuntime()->pixel_per_line_i = (int)getRuntime()->pixel_per_line;
        }

        return TRUE;
    }

    tmp = Strnew();
    if (!strchr(getRuntime()->Imgdisplay, '/'))
        Strcat_m_charp(tmp, w3m_auxbin_dir(), "/", NULL);
    Strcat_m_charp(tmp, getRuntime()->Imgdisplay, " -test 2>/dev/null", NULL);
    f = popen(tmp->ptr, "r");
    if (!f)
        return FALSE;
    while (fscanf(f, "%d %d", &w, &h) < 0) {
        if (feof(f))
            break;
    }
    pclose(f);

    if (!(w > 0 && h > 0))
        return FALSE;
    if (!getRuntime()->set_pixel_per_char)
        getRuntime()->pixel_per_char = (int)(1.0 * w / TTY_COLS() + 0.5);
    if (!getRuntime()->set_pixel_per_line)
        getRuntime()->pixel_per_line = (int)(1.0 * h / TTY_LINES() + 0.5);
    return TRUE;
}

void termImage()
{
    if (!getRuntime()->activeImage)
        return;
    clearImage();
    if (Imgdisplay_wf) {
        fputs("2;\n", Imgdisplay_wf); /* ClearImage() */
        fflush(Imgdisplay_wf);
    }
    closeImgdisplay();
}

static int
openImgdisplay()
{
    char* cmd;

    if (!strchr(getRuntime()->Imgdisplay, '/'))
        cmd = Strnew_m_charp(w3m_auxbin_dir(), "/", getRuntime()->Imgdisplay, NULL)->ptr;
    else
        cmd = getRuntime()->Imgdisplay;
    Imgdisplay_pid = open_pipe_rw(&Imgdisplay_rf, &Imgdisplay_wf);
    if (Imgdisplay_pid < 0)
        goto err0;
    if (Imgdisplay_pid == 0) {
        /* child */
        setup_child(FALSE, 2, -1);
        myExec(cmd);
        /* XXX: ifdef __EMX__, use start /f ? */
    }
    getRuntime()->activeImage = TRUE;
    return TRUE;
err0:
    Imgdisplay_pid = 0;
    getRuntime()->activeImage = FALSE;
    return FALSE;
}

static void
closeImgdisplay(void)
{
    if (Imgdisplay_wf)
        fclose(Imgdisplay_wf);
    if (Imgdisplay_rf) {
        /* sync with the child */
        getc(Imgdisplay_rf); /* EOF expected */
        fclose(Imgdisplay_rf);
    }
    if (Imgdisplay_pid)
        kill(Imgdisplay_pid, SIGKILL);
    Imgdisplay_rf = NULL;
    Imgdisplay_wf = NULL;
    Imgdisplay_pid = 0;
}

void addImage(struct ImageCache* cache, int x, int y, int sx, int sy, int w, int h)
{
    struct TerminalImage* i;

    if (!getRuntime()->activeImage)
        return;
    if (n_terminal_image >= max_terminal_image) {
        max_terminal_image = max_terminal_image ? (2 * max_terminal_image) : 8;
        terminal_image = New_Reuse(struct TerminalImage, terminal_image,
            max_terminal_image);
    }
    i = &terminal_image[n_terminal_image];
    i->cache = cache;
    i->x = x;
    i->y = y;
    i->sx = sx;
    i->sy = sy;
    i->width = w;
    i->height = h;
    n_terminal_image++;
}

static void
syncImage(void)
{
    if (getRuntime()->enable_inline_image) {
        return;
    }

    fputs("3;\n", Imgdisplay_wf); /* XSync() */
    fputs("4;\n", Imgdisplay_wf); /* put '\n' */
    while (fflush(Imgdisplay_wf) != 0) {
        if (ferror(Imgdisplay_wf))
            goto err;
    }
    if (!fgetc(Imgdisplay_rf))
        goto err;
    return;
err:
    closeImgdisplay();
    image_index += MAX_IMAGE;
    n_terminal_image = 0;
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

static void put_image_sixel(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int n_terminal_image)
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
        tty_add_ISIG();

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
        argv[n++] = (char*)url;
        if (getenv("TERM") && strcmp(getenv("TERM"), "screen") == 0 && (!getenv("SCREEN_VARIANT") || strcmp(getenv("SCREEN_VARIANT"), "sixel") != 0)) {
            argv[n++] = "-P";
        }
        argv[n++] = NULL;
        execvp(argv[0], argv);
        exit(0);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        tty_remove_ISIG();
        mySignal(SIGINT, previntr);
        mySignal(SIGQUIT, prevquit);
        mySignal(SIGTSTP, prevstop);
        if (do_anim) {
            writestr("\x1b[?80l");
        }
    }
}

static Str get_image_osc5379(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh)
{
    const char* size;
    if (w > 0 && h > 0)
        size = Sprintf("%dx%d", w, h)->ptr;
    else
        size = "";

    return Sprintf("\x1b]5379;show_picture %s %s %dx%d+%d+%d\x07", url, size, sw, sh, sx, sy);
}

static void put_image_iterm2(const char* url, int x, int y, int w, int h)
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
    tty_MOVE(Currentbuf->doc.cursorY, Currentbuf->doc.cursorX);
}

static void put_image_kitty(const char* url, int x, int y, int w, int h, int sx, int sy, int sw,
    int sh, int cols, int rows)
{
    Str buf, base64;
    char *cbuf, *tmpf;
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

    const char* type = guessContentType(url);
    t = 100; /* always convert to png for now. */

    if (!(type && !strcasecmp(type, "image/png"))) {
        tmpf = Sprintf("%s/%s.png", getRuntime()->tmp_dir, mybasename(url))->ptr;

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
                tty_add_ISIG();

                if ((cbuf = getenv("W3M_KITTY_TO_PNG")))
                    argv[i++] = cbuf;
                else
                    argv[i++] = "convert";

                if (is_anim) {
                    buf = Strnew_charp(url);
                    Strcat_charp(buf, "[0]");
                    argv[i++] = buf->ptr;
                } else {
                    argv[i++] = (char*)url;
                }
                argv[i++] = tmpf;
                argv[i++] = NULL;
                execvp(argv[0], argv);
                exit(0);
            } else if (pid > 0) {
                waitpid(pid, &i, 0);
                tty_remove_ISIG();
                mySignal(SIGINT, previntr);
                mySignal(SIGQUIT, prevquit);
                mySignal(SIGTSTP, prevstop);
            }

            pushText(getRuntime()->fileToDelete, tmpf);
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
    tty_MOVE(Currentbuf->doc.cursorY, Currentbuf->doc.cursorX);
}

void drawImage(struct Buffer* currentbuf)
{
    struct Runtime* rt = getRuntime();
    if (!rt->activeImage)
        return;
    if (!n_terminal_image)
        return;

    bool draw = false;
    for (int j = 0; j < n_terminal_image; j++) {
        struct TerminalImage* i = &terminal_image[j];

        if (rt->enable_inline_image) {
            /*
             * So this shouldn't ever happen, but if it does then at least let's
             * not have external programs fetch images from the Internet...
             */
            if (!i->cache->touch)
                return;
            struct stat st;
            if (stat(i->cache->file, &st) != 0)
                return;

            const char* url = i->cache->file;
            int x = i->x / rt->pixel_per_char_i;
            int y = i->y / rt->pixel_per_line_i;
            int w = (i->cache->a_width > 0) //
                ? ((i->cache->width + i->x % rt->pixel_per_char_i + rt->pixel_per_char_i - 1) / rt->pixel_per_char_i)
                : 0;
            int h = (i->cache->a_height > 0) //
                ? ((i->cache->height + i->y % rt->pixel_per_line_i + rt->pixel_per_line_i - 1) / rt->pixel_per_line_i)
                : 0;
            int sx = i->sx / rt->pixel_per_char_i;
            int sy = i->sy / rt->pixel_per_line_i;
            int sw = (i->width + i->sx % rt->pixel_per_char_i + rt->pixel_per_char_i - 1) / rt->pixel_per_char_i;
            int sh = (i->height + i->sy % rt->pixel_per_line_i + rt->pixel_per_line_i - 1) / rt->pixel_per_line_i;

            if (rt->enable_inline_image == INLINE_IMG_SIXEL) {
                w = i->cache->a_width > 0 ? i->width : 0;
                h = i->cache->a_height > 0 ? i->height : 0;
                put_image_sixel(url, x, y, w, h, i->sx, i->sy, sw * rt->pixel_per_char, sh * rt->pixel_per_line_i, n_terminal_image);
                tty_MOVE(Currentbuf->doc.cursorY, Currentbuf->doc.cursorX);
            } else if (rt->enable_inline_image == INLINE_IMG_OSC5379) {
                Str buf = get_image_osc5379(url, x, y, w, h, sx, sy, sw, sh);
                tty_MOVE(y, x);
                writestr(buf->ptr);
                tty_MOVE(Currentbuf->doc.cursorY, Currentbuf->doc.cursorX);
            } else if (rt->enable_inline_image == INLINE_IMG_ITERM2) {
                put_image_iterm2(url, x, y, sw, sh);
            } else if (rt->enable_inline_image == INLINE_IMG_KITTY) {
                put_image_kitty(url, x, y, i->width, i->height, i->sx, i->sy, sw * rt->pixel_per_char, sh * rt->pixel_per_line_i, sw, sh);
            }

            continue;
        }

        if (!(i->cache->loaded & IMG_FLAG_LOADED && i->width > 0 && i->height > 0))
            continue;
        if (!(Imgdisplay_rf && Imgdisplay_wf)) {
            if (!openImgdisplay())
                return;
        }
        if (i->cache->index > 0) {
            i->cache->index *= -1;
            fputs("0;", Imgdisplay_wf); /* DrawImage() */
        } else
            fputs("1;", Imgdisplay_wf); /* DrawImage(redraw) */

        char buf[64];
        sprintf(buf, "%d;%d;%d;%d;%d;%d;%d;%d;%d;",
            (-i->cache->index - 1) % MAX_IMAGE + 1, i->x, i->y,
            (i->cache->width > 0) ? i->cache->width : 0,
            (i->cache->height > 0) ? i->cache->height : 0,
            i->sx, i->sy, i->width, i->height);
        fputs(buf, Imgdisplay_wf);
        fputs(i->cache->file, Imgdisplay_wf);
        fputs("\n", Imgdisplay_wf);
        draw = TRUE;
    }

    if (!rt->enable_inline_image) {
        if (!draw)
            return;
        syncImage();
    } else
        n_terminal_image = 0;

    screen_touch_cursor();
}

void clearImage()
{
    static char buf[64];
    int j;
    struct TerminalImage* i;

    if (!getRuntime()->activeImage)
        return;
    if (!n_terminal_image)
        return;
    if (!Imgdisplay_wf) {
        n_terminal_image = 0;
        return;
    }
    for (j = 0; j < n_terminal_image; j++) {
        i = &terminal_image[j];
        if (!(i->cache->loaded & IMG_FLAG_LOADED && i->width > 0 && i->height > 0))
            continue;
        sprintf(buf, "6;%d;%d;%d;%d\n", i->x, i->y, i->width, i->height);
        fputs(buf, Imgdisplay_wf);
    }
    syncImage();
    n_terminal_image = 0;
}

/* load image */

#ifndef MAX_LOAD_IMAGE
#define MAX_LOAD_IMAGE 8
#endif
static int n_load_image = 0;
static Hash_sv* image_hash = NULL;
static Hash_sv* image_file = NULL;
static GeneralList* image_list = NULL;
static struct ImageCache** image_cache = NULL;
// static struct Buffer* image_buffer = NULL;

void deleteImage(struct Buffer* buf)
{
    struct AnchorList* al;
    struct Anchor* a;
    int i;

    if (!buf)
        return;
    al = buf->doc.img;
    if (!al)
        return;
    for (i = 0, a = al->anchors; i < al->nanchor; i++, a++) {
        if (a->image && a->image->cache && a->image->cache->loaded != IMG_FLAG_UNLOADED && !(a->image->cache->loaded & IMG_FLAG_DONT_REMOVE) && a->image->cache->index < 0)
            unlink(a->image->cache->file);
    }
    loadImage(IMG_FLAG_STOP);
}

// void getAllImage(struct Buffer* buf)
// {
//     struct AnchorList* al;
//     struct Anchor* a;
//     struct Url* current;
//     int i;
//
//     image_buffer = buf;
//     if (!buf)
//         return;
//     buf->image_loaded = TRUE;
//     al = buf->doc.img;
//     if (!al)
//         return;
//     current = baseURL(buf);
//     for (i = 0, a = al->anchors; i < al->nanchor; i++, a++) {
//         if (a->image) {
//             a->image->cache = getImage(a->image, current, buf->image_flag);
//             if (a->image->cache && a->image->cache->loaded == IMG_FLAG_UNLOADED)
//                 buf->image_loaded = FALSE;
//         }
//     }
// }

static void
showImageProgress(struct Buffer* buf)
{
    struct AnchorList* al;
    struct Anchor* a;
    int i, l, n;

    if (!buf)
        return;
    al = buf->doc.img;
    if (!al)
        return;
    for (i = 0, l = 0, n = 0, a = al->anchors; i < al->nanchor; i++, a++) {
        if (a->image && a->hseq >= 0) {
            n++;
            if (a->image->cache && a->image->cache->loaded & IMG_FLAG_LOADED)
                l++;
        }
    }
    if (n) {
        if (getRuntime()->enable_inline_image && n == l)
            drawImage(buf);
        message(Sprintf("%d/%d images loaded", l, n)->ptr,
            buf->doc.cursorX + buf->doc.rootX, buf->doc.cursorY + buf->doc.rootY);
    }
}

void loadImage(enum ImageLoadFlags flag)
{
    if (!getRuntime()->activeImage) {
        return;
    }
    // if (!buf) {
    //     return;
    // }
    // if (!buf->doc.img) {
    //     return;
    // }
    // if (buf->image_loaded) {
    //     return;
    // }

    if (getRuntime()->maxLoadImage > MAX_LOAD_IMAGE)
        getRuntime()->maxLoadImage = MAX_LOAD_IMAGE;
    else if (getRuntime()->maxLoadImage < 1)
        getRuntime()->maxLoadImage = 1;
    if (n_load_image == 0)
        n_load_image = getRuntime()->maxLoadImage;
    if (!image_cache) {
        image_cache = New_N(struct ImageCache*, MAX_LOAD_IMAGE);
        bzero(image_cache, sizeof(struct ImageCache*) * MAX_LOAD_IMAGE);
    }

    // bool draw = false;
    // for (int i = 0; i < n_load_image; i++) {
    //     struct ImageCache* cache = image_cache[i];
    //     if (!cache || !cache->touch)
    //         continue;
    //     struct stat st;
    //     if (lstat(cache->touch, &st) != 0)
    //         continue;
    //     if (cache->pid) {
    //         continue;
    //         kill(cache->pid, SIGKILL);
    //         /*
    //          * #ifdef HAVE_WAITPID
    //          * waitpid(cache->pid, &wait_st, 0);
    //          * #else
    //          * wait(&wait_st);
    //          * #endif
    //          */
    //         cache->pid = 0;
    //     }
    //     if (stat(cache->file, &st) == 0) {
    //         cache->loaded = IMG_FLAG_LOADED;
    //         getImageSize(cache);
    //         draw = true;
    //     } else {
    //         cache->loaded = IMG_FLAG_ERROR;
    //     }
    //     unlink(cache->touch);
    //     image_cache[i] = NULL;
    // }
    //
    // for (int i = 0; i < n_load_image; i++) {
    //     struct ImageCache* cache = image_cache[i];
    //     if (!cache || !cache->touch)
    //         continue;
    //     if (cache->pid) {
    //         kill(cache->pid, SIGKILL);
    //         /*
    //          * #ifdef HAVE_WAITPID
    //          * waitpid(cache->pid, &wait_st, 0);
    //          * #else
    //          * wait(&wait_st);
    //          * #endif
    //          */
    //         cache->pid = 0;
    //     }
    //     /*TODO make sure removing this didn't break anything
    //     unlink(cache->touch);
    //     */
    //     image_cache[i] = NULL;
    // }

    if (flag == IMG_FLAG_STOP) {
        image_list = NULL;
        image_file = NULL;
        n_load_image = getRuntime()->maxLoadImage;
        // image_buffer = NULL;
        return;
    }

    // if (draw) {
    //     if (!getRuntime()->enable_inline_image)
    //         drawImage(buf);
    //     showImageProgress(buf);
    // }

    // image_buffer = buf;

    if (!image_list)
        return;
    for (int i = 0; i < n_load_image; i++) {
        if (image_cache[i])
            continue;

        struct ImageCache* cache = 0;
        while (1) {
            cache = (struct ImageCache*)popValue(image_list);
            if (!cache) {
                for (i = 0; i < n_load_image; i++) {
                    if (image_cache[i])
                        return;
                }
                image_list = NULL;
                image_file = NULL;
                return;
            }
            if (cache->loaded == IMG_FLAG_UNLOADED)
                break;
        }
        image_cache[i] = cache;
        if (!cache->touch) {
            continue;
        }

        // loadGeneralFile(cache->url, cache->current, NULL, 0, NULL, false, cache->file);
        checkRedirection(NULL);
        struct ContentData data = get_content(cache->url, cache->current, NULL,
            (struct LoadOption) {
                .flag = 0,
                .referer = NULL,
                .extra_header = NULL,
            },
            (struct AuthInfo) {
                .realm = NULL,
                .uname = NULL,
                .pwd = NULL,
            },
            NULL);
        struct Buffer* b = NULL;
        if (is_save2tmp(data.stream, cache->file)) {
            b = newBuffer(INIT_BUFFER_WIDTH);
            b->content.sourcefile = cache->file;
        }
        is_close(data.stream);
        // TRAP_OFF;
        // return b;

        symlink(cache->file, cache->touch);

        // flush_tty();
        // if ((cache->pid = fork()) == 0) {
        //     /*
        //      * setup_child(TRUE, 0, -1);
        //      */
        //     setup_child(FALSE, 0, -1);
        //     getRuntime()->image_source = cache->file;
        //     loadGeneralFile(cache->url, cache->current, NULL, 0, NULL, false);
        //     /* TODO make sure removing this didn't break anything
        //     if (!b || !b->real_type || strncasecmp(b->real_type, "image/", 6))
        //         unlink(cache->file);
        //     */
        //     symlink(cache->file, cache->touch);
        //     exit(0);
        // } else if (cache->pid < 0) {
        //     cache->pid = 0;
        //     return;
        // }
    }
}

struct ImageCache*
getImage(struct Image* image, struct Url* current, enum ImageGetFlags flag)
{
    Str key = NULL;
    struct ImageCache* cache;

    if (!getRuntime()->activeImage)
        return NULL;
    if (!image_hash)
        image_hash = newHash_sv(100);
    if (image->cache)
        cache = image->cache;
    else {
        key = Sprintf("%d;%d;%s", image->width, image->height, image->url);
        cache = (struct ImageCache*)getHash_sv(image_hash, key->ptr, NULL);
    }
    if (cache && cache->index && abs(cache->index) <= image_index - MAX_IMAGE) {
        struct stat st;
        if (stat(cache->file, &st))
            cache->loaded = IMG_FLAG_UNLOADED;
        cache->index = 0;
    }

    if (!cache) {
        if (flag == IMG_FLAG_SKIP)
            return NULL;

        cache = New(struct ImageCache);
        cache->url = image->url;
        cache->current = current;
        cache->file = tmpfname(TMPF_DFL, image->ext)->ptr;
        cache->pid = 0;
        cache->index = 0;
        cache->loaded = IMG_FLAG_UNLOADED;
        if (getRuntime()->enable_inline_image == INLINE_IMG_OSC5379) {
            if (image->width > 0 && image->width % getRuntime()->pixel_per_char_i > 0)
                image->width += (getRuntime()->pixel_per_char_i - image->width % getRuntime()->pixel_per_char_i);

            if (image->height > 0 && image->height % getRuntime()->pixel_per_line_i > 0)
                image->height += (getRuntime()->pixel_per_line_i - image->height % getRuntime()->pixel_per_line_i);
        }
        cache->touch = tmpfname(TMPF_DFL, NULL)->ptr;

        cache->width = image->width;
        cache->height = image->height;
        cache->a_width = image->width;
        cache->a_height = image->height;
        putHash_sv(image_hash, key->ptr, (void*)cache);
    }
    if (flag != IMG_FLAG_SKIP) {
        if (cache->loaded == IMG_FLAG_UNLOADED) {
            if (!image_file)
                image_file = newHash_sv(100);
            if (!getHash_sv(image_file, cache->file, NULL)) {
                putHash_sv(image_file, cache->file, (void*)cache);
                if (!image_list)
                    image_list = newGeneralList();
                pushValue(image_list, (void*)cache);
            }
        }
        if (!cache->index)
            cache->index = ++image_index;
    }
    if (cache->loaded & IMG_FLAG_LOADED)
        getImageSize(cache);
    return cache;
}

static int
parseImageHeader(char* path, u_int* width, u_int* height)
{
    FILE* fp;
    u_char buf[8];

    if (!(fp = fopen(path, "r")))
        return FALSE;

    if (fread(buf, 1, 2, fp) != 2)
        goto error;

    if (memcmp(buf, "\xff\xd8", 2) == 0) {
        /* JPEG */
        if (fseek(fp, 2, SEEK_CUR) < 0)
            goto error; /* 0xffe0 */
        while (fread(buf, 1, 2, fp) == 2) {
            size_t len = ((buf[0] << 8) | buf[1]) - 2;
            if (fseek(fp, len, SEEK_CUR) < 0)
                goto error;
            if (fread(buf, 1, 2, fp) == 2 &&
                /* SOF0 or SOF2 */
                (memcmp(buf, "\xff\xc0", 2) == 0 || memcmp(buf, "\xff\xc2", 2) == 0)) {
                fseek(fp, 3, SEEK_CUR);
                if (fread(buf, 1, 2, fp) == 2) {
                    *height = (buf[0] << 8) | buf[1];
                    if (fread(buf, 1, 2, fp) == 2) {
                        *width = (buf[0] << 8) | buf[1];
                        goto success;
                    }
                }
                break;
            }
        }
        goto error;
    }

    if (fread(buf + 2, 1, 1, fp) != 1)
        goto error;

    if (memcmp(buf, "GIF", 3) == 0) {
        /* GIF */
        if (fseek(fp, 3, SEEK_CUR) < 0)
            goto error;
        if (fread(buf, 1, 2, fp) == 2) {
            *width = (buf[1] << 8) | buf[0];
            if (fread(buf, 1, 2, fp) == 2) {
                *height = (buf[1] << 8) | buf[0];
                goto success;
            }
        }
        goto error;
    }

    if (fread(buf + 3, 1, 5, fp) != 5)
        goto error;

    if (memcmp(buf, "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a", 8) == 0) {
        /* PNG */
        if (fseek(fp, 8, SEEK_CUR) < 0)
            goto error;
        if (fread(buf, 1, 4, fp) == 4) {
            *width = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
            if (fread(buf, 1, 4, fp) == 4) {
                *height = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
                goto success;
            }
        }
        goto error;
    }

error:
    fclose(fp);
    return FALSE;

success:
    fclose(fp);
    return TRUE;
}

int getImageSize(struct ImageCache* cache)
{
    Str tmp;
    FILE* f;
    unsigned int w = 0, h = 0;

    if (!getRuntime()->activeImage)
        return FALSE;
    if (!cache || !(cache->loaded & IMG_FLAG_LOADED) || (cache->width > 0 && cache->height > 0))
        return FALSE;

    if (parseImageHeader(cache->file, &w, &h))
        goto got_image_size;

    tmp = Strnew();
    if (!strchr(getRuntime()->Imgdisplay, '/'))
        Strcat_m_charp(tmp, w3m_auxbin_dir(), "/", NULL);
    Strcat_m_charp(tmp, getRuntime()->Imgdisplay, " -size ", shell_quote(cache->file), NULL);
    f = popen(tmp->ptr, "r");
    if (!f)
        return FALSE;
    while (fscanf(f, "%u %u", &w, &h) < 0) {
        if (feof(f))
            break;
    }
    pclose(f);

    if (!(w > 0 && h > 0))
        return FALSE;

got_image_size:
    w = (int)(w * getRuntime()->image_scale / 100 + 0.5);
    if (w == 0)
        w = 1;
    h = (int)(h * getRuntime()->image_scale / 100 + 0.5);
    if (h == 0)
        h = 1;
    if (cache->width < 0 && cache->height < 0) {
        cache->width = (w > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : w;
        cache->height = (h > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : h;
    } else if (cache->width < 0) {
        int tmp = (int)((double)cache->height * w / h + 0.5);
        cache->a_width = cache->width = (tmp > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : tmp;
    } else if (cache->height < 0) {
        int tmp = (int)((double)cache->width * h / w + 0.5);
        cache->a_height = cache->height = (tmp > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : tmp;
    }
    if (cache->width == 0)
        cache->width = 1;
    if (cache->height == 0)
        cache->height = 1;
    tmp = Sprintf("%d;%d;%s", cache->width, cache->height, cache->url);
    putHash_sv(image_hash, tmp->ptr, (void*)cache);
    return TRUE;
}
