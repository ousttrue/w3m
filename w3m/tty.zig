const std = @import("std");
const c = @cImport({
    @cInclude("sys/ioctl.h");
    @cInclude("unistd.h");
});
const g = @import("global.zig");
const runtime = @import("runtime.zig");
const Tty = @import("TtyLinux.zig");

var tty: ?*Tty = null;

pub fn init() void {
    tty = .create(
        runtime.allocator,
        runtime.io,
        std.Io.File.stdin(),
        std.Io.File.stdout(),
    );
}

// int set_tty(void)
// {
//     char* ttyn;
//
//     if (isatty(0)) /* stdin */
//         ttyn = ttyname(0);
//     else
//         ttyn = DEV_TTY_PATH;
//     tty = open(ttyn, O_RDWR);
//     if (tty < 0) {
//         /* use stderr instead of stdin... is it OK???? */
//         tty = 2;
//     }
//     ttyf = fdopen(tty, "w");
//     tcgetattr(tty, &d_ioval);
//     if (displayTitleTerm != NULL) {
//         struct w3m_term_info* p;
//         for (p = w3m_term_info_list; p->term != NULL; p++) {
//             if (!strncmp(displayTitleTerm, p->term, strlen(p->term))) {
//                 title_str = p->title_str;
//                 break;
//             }
//         }
//     }
//     {
//         char* term = getenv("TERM");
//         if (term != NULL) {
//             struct w3m_term_info* p;
//             for (p = w3m_term_info_list; p->term != NULL; p++) {
//                 if (!strncmp(term, p->term, strlen(p->term))) {
//                     is_xterm = p->mouse_flag;
//                     break;
//                 }
//             }
//         }
//     }
//
//     return 0;
// }

export fn close_tty() void {
    // if (tty > 2)
    //     close(tty);
}

export fn ttyname_tty() [*c]const u8 {
    return c.ttyname(tty.?.stdin.handle);
}

pub fn deinit() void {
    if (tty) |p| {
        p.destroy();
    }
}

export fn setlinescols() void {
    // char* p;
    // int i;

    var wins: c.winsize = undefined;
    const i = c.ioctl(tty.?.stdin.handle, c.TIOCGWINSZ, &wins);
    if (i >= 0 and wins.ws_row != 0 and wins.ws_col != 0) {
        g.LINES = wins.ws_row;
        g.COLS = wins.ws_col;
    }

    // if (LINES <= 0 && (p = getenv("LINES")) != NULL && (i = atoi(p)) >= 0)
    //     LINES = i;
    // if (COLS <= 0 && (p = getenv("COLUMNS")) != NULL && (i = atoi(p)) >= 0)
    //     COLS = i;
    // if (LINES <= 0)
    //     LINES = tgetnum("li"); /* number of line */
    // if (COLS <= 0)
    //     COLS = tgetnum("co"); /* number of column */
    // if (COLS > MAX_COLUMN)
    //     COLS = MAX_COLUMN;
    // if (LINES > MAX_LINE)
    //     LINES = MAX_LINE;
}

export fn crmode() void {
    tty.?.crmode(true) catch {};
}

export fn term_echo() void {
    tty.?.echo(true) catch {};
}

export fn term_noecho() void {
    tty.?.echo(false) catch {};
}

export fn term_raw() void {
    tty.?.raw() catch {};
}

export fn term_cbreak() void {
    tty.?.cooked(.{ .echo = false }) catch {};
}

export fn term_title(s: [*c]const u8) void {
    _ = s;
    //     if (!fmInitialized)
    //         return;
    //     if (title_str != NULL) {
    //         fprintf(ttyf, title_str, s);
    //     }
}

export fn get_pixel_per_cell(ppc: *c_int, ppl: *c_int) c_int {
    _ = ppc;
    _ = ppl;
    // fd_set rfd;
    // struct timeval tval;
    // char buf[100];
    // char* p;
    // ssize_t len;
    // ssize_t left;
    // int wp, hp, wc, hc;
    // int i;
    //
    // struct winsize ws;
    // if (ioctl(tty, TIOCGWINSZ, &ws) == 0 && ws.ws_ypixel > 0 && ws.ws_row > 0 && ws.ws_xpixel > 0 && ws.ws_col > 0) {
    //     *ppc = ws.ws_xpixel / ws.ws_col;
    //     *ppl = ws.ws_ypixel / ws.ws_row;
    //     return 1;
    // }
    //
    // fputs("\x1b[14t\x1b[18t", ttyf);
    // flush_tty();
    //
    // p = buf;
    // left = sizeof(buf) - 1;
    // for (i = 0; i < 10; i++) {
    //     tval.tv_usec = 200000; /* 0.2 sec * 10 */
    //     tval.tv_sec = 0;
    //     FD_ZERO(&rfd);
    //     FD_SET(tty, &rfd);
    //     if (select(tty + 1, &rfd, NULL, NULL, &tval) <= 0 || !FD_ISSET(tty, &rfd))
    //         continue;
    //
    //     if ((len = read(tty, p, left)) <= 0)
    //         continue;
    //     p[len] = '\0';
    //
    //     if (sscanf(buf, "\x1b[4;%d;%dt\x1b[8;%d;%dt", &hp, &wp, &hc, &wc) == 4) {
    //         if (wp > 0 && wc > 0 && hp > 0 && hc > 0) {
    //             *ppc = wp / wc;
    //             *ppl = hp / hc;
    //             return 1;
    //         } else {
    //             return 0;
    //         }
    //     }
    //     p += len;
    //     left -= len;
    // }

    return 0;
}

export fn flush_tty() void {
    tty.?.stdout_writer.flush() catch {};
}

export fn writer(str: [*]const u8, len: usize) void {
    (tty.?.puts(str[0..len]) catch {});
    tty.?.stdout_writer.flush() catch {};
}

export fn write1(ch: u8) c_int {
    _ = (tty.?.putc(ch) catch {});
    tty.?.stdout_writer.flush() catch {};
    return 0;
}

export fn put_image_sixel(
    url: [*c]const u8,
    x: c_int,
    y: c_int,
    w: c_int,
    h: c_int,
    sx: c_int,
    sy: c_int,
    sw: c_int,
    sh: c_int,
    n_terminal_image: c_int,
) void {
    _ = url;
    _ = x;
    _ = y;
    _ = w;
    _ = h;
    _ = sx;
    _ = sy;
    _ = sw;
    _ = sh;
    _ = n_terminal_image;
    //     pid_t pid;
    //     int do_anim;
    //     MySignalHandler (*volatile previntr)(SIGNAL_ARG);
    //     MySignalHandler (*volatile prevquit)(SIGNAL_ARG);
    //     MySignalHandler (*volatile prevstop)(SIGNAL_ARG);
    //
    //     MOVE(y, x);
    //     flush_tty();
    //
    //     do_anim = (n_terminal_image == 1 && x == 0 && y == 0 && sx == 0 && sy == 0);
    //
    //     previntr = mySignal(SIGINT, SIG_IGN);
    //     prevquit = mySignal(SIGQUIT, SIG_IGN);
    //     prevstop = mySignal(SIGTSTP, SIG_IGN);
    //
    //     if ((pid = fork()) == 0) {
    //         char* env;
    //         int n = 0;
    //         char* argv[20];
    //         char digit[2][11 + 1];
    //         char clip[44 + 3 + 1];
    //         Str str_url;
    //
    //         close(STDERR_FILENO); /* Don't output error message. */
    //         if (do_anim) {
    //             writestr("\x1b[?80h");
    //         } else if (!strstr(url, "://") && strcmp(url + strlen(url) - 4, ".gif") == 0 && (str_url = save_first_animation_frame(url))) {
    //             url = str_url->ptr;
    //         }
    //         ttymode_add_local_input(ISIG, 0);
    //
    //         if ((env = getenv("W3M_IMG2SIXEL"))) {
    //             char* p;
    //             env = Strnew_charp(env)->ptr;
    //             while (n < 8 && (p = strchr(env, ' '))) {
    //                 *p = '\0';
    //                 if (*env != '\0') {
    //                     argv[n++] = env;
    //                 }
    //                 env = p + 1;
    //             }
    //             if (*env != '\0') {
    //                 argv[n++] = env;
    //             }
    //         } else {
    //             argv[n++] = "img2sixel";
    //         }
    //         argv[n++] = "-l";
    //         argv[n++] = do_anim ? "auto" : "disable";
    //         argv[n++] = "-w";
    //         sprintf(digit[0], "%d", w);
    //         argv[n++] = digit[0];
    //         argv[n++] = "-h";
    //         sprintf(digit[1], "%d", h);
    //         argv[n++] = digit[1];
    //         argv[n++] = "-c";
    //         sprintf(clip, "%dx%d+%d+%d", sw, sh, sx, sy);
    //         argv[n++] = clip;
    //         argv[n++] = url;
    //         if (getenv("TERM") && strcmp(getenv("TERM"), "screen") == 0 && (!getenv("SCREEN_VARIANT") || strcmp(getenv("SCREEN_VARIANT"), "sixel") != 0)) {
    //             argv[n++] = "-P";
    //         }
    //         argv[n++] = NULL;
    //         execvp(argv[0], argv);
    //         exit(0);
    //     } else if (pid > 0) {
    //         int status;
    //         waitpid(pid, &status, 0);
    //         ttymode_remove_local_input(ISIG, 0);
    //         mySignal(SIGINT, previntr);
    //         mySignal(SIGQUIT, prevquit);
    //         mySignal(SIGTSTP, prevstop);
    //         if (do_anim) {
    //             writestr("\x1b[?80l");
    //         }
    //     }
    //
    //     MOVE(Currentbuf->cursorY, Currentbuf->cursorX);
}

export fn put_image_osc5379(
    url: [*c]const u8,
    x: c_int,
    y: c_int,
    w: c_int,
    h: c_int,
    sx: c_int,
    sy: c_int,
    sw: c_int,
    sh: c_int,
) void {
    _ = url;
    _ = x;
    _ = y;
    _ = w;
    _ = h;
    _ = sx;
    _ = sy;
    _ = sw;
    _ = sh;
    //     Str buf;
    //     char* size;
    //
    //     if (w > 0 && h > 0)
    //         size = Sprintf("%dx%d", w, h)->ptr;
    //     else
    //         size = "";
    //
    //     MOVE(y, x);
    //     buf = Sprintf("\x1b]5379;show_picture %s %s %dx%d+%d+%d\x07", url, size, sw, sh, sx, sy);
    //     writestr(buf->ptr);
    //     MOVE(Currentbuf->cursorY, Currentbuf->cursorX);
}

export fn put_image_iterm2(url: [*c]const u8, x: c_int, y: c_int, w: c_int, h: c_int) void {
    _ = url;
    _ = x;
    _ = y;
    _ = w;
    _ = h;
    //     Str buf;
    //     char* cbuf;
    //     FILE* fp;
    //     int c, i;
    //     struct stat st;
    //
    //     if (stat(url, &st))
    //         return;
    //
    //     fp = fopen(url, "r");
    //     if (!fp)
    //         return;
    //
    //     buf = Sprintf("\x1b]1337;"
    //                   "File="
    //                   "name=%s;"
    //                   "size=%d;"
    //                   "width=%d;"
    //                   "height=%d;"
    //                   "preserveAspectRatio=0;"
    //                   "inline=1"
    //                   ":",
    //         url, st.st_size, w, h);
    //
    //     MOVE(y, x);
    //
    //     writestr(buf->ptr);
    //
    //     cbuf = GC_MALLOC_ATOMIC(3072);
    //     if (!cbuf)
    //         goto cleanup;
    //     i = 0;
    //     while ((c = fgetc(fp)) != EOF) {
    //         cbuf[i++] = c;
    //         if (i == 3072) {
    //             buf = base64_encode(cbuf, i);
    //             writestr(buf->ptr);
    //             i = 0;
    //         }
    //     }
    //
    //     if (i) {
    //         buf = base64_encode(cbuf, i);
    //         writestr(buf->ptr);
    //     }
    //
    // cleanup:
    //     fclose(fp);
    //     writestr("\a");
    //     MOVE(Currentbuf->cursorY, Currentbuf->cursorX);
}

export fn put_image_kitty(
    url: [*c]const u8,
    x: c_int,
    y: c_int,
    w: c_int,
    h: c_int,
    sx: c_int,
    sy: c_int,
    sw: c_int,
    sh: c_int,
    cols: c_int,
    rows: c_int,
) void {
    _ = url;
    _ = x;
    _ = y;
    _ = w;
    _ = h;
    _ = sx;
    _ = sy;
    _ = sw;
    _ = sh;
    _ = cols;
    _ = rows;
    //     Str buf, base64;
    //     char *cbuf, *tmpf;
    //     char* argv[4];
    //     FILE* fp;
    //     int c, i, j, m, t, is_anim;
    //     struct stat st;
    //     pid_t pid;
    //     MySignalHandler (*volatile previntr)(SIGNAL_ARG);
    //     MySignalHandler (*volatile prevquit)(SIGNAL_ARG);
    //     MySignalHandler (*volatile prevstop)(SIGNAL_ARG);
    //
    //     if (!url)
    //         return;
    //
    //     const char* type = guessContentType(url);
    //     t = 100; /* always convert to png for now. */
    //
    //     if (!(type && !strcasecmp(type, "image/png"))) {
    //         tmpf = Sprintf("%s/%s.png", tmp_dir, mybasename(url))->ptr;
    //
    //         if (type && !strcasecmp(type, "image/gif")) {
    //             is_anim = 1;
    //         } else {
    //             is_anim = 0;
    //         }
    //
    //         /* convert only if png doesn't exist yet. */
    //
    //         if (stat(tmpf, &st)) {
    //             if (stat(url, &st))
    //                 return;
    //
    //             flush_tty();
    //
    //             previntr = mySignal(SIGINT, SIG_IGN);
    //             prevquit = mySignal(SIGQUIT, SIG_IGN);
    //             prevstop = mySignal(SIGTSTP, SIG_IGN);
    //
    //             if ((pid = fork()) == 0) {
    //                 i = 0;
    //
    //                 close(STDERR_FILENO); /* Don't output error message. */
    //                 ttymode_add_local_input(ISIG, 0);
    //
    //                 if ((cbuf = getenv("W3M_KITTY_TO_PNG")))
    //                     argv[i++] = cbuf;
    //                 else
    //                     argv[i++] = "convert";
    //
    //                 if (is_anim) {
    //                     buf = Strnew_charp(url);
    //                     Strcat_charp(buf, "[0]");
    //                     argv[i++] = buf->ptr;
    //                 } else {
    //                     argv[i++] = url;
    //                 }
    //                 argv[i++] = tmpf;
    //                 argv[i++] = NULL;
    //                 execvp(argv[0], argv);
    //                 exit(0);
    //             } else if (pid > 0) {
    //                 waitpid(pid, &i, 0);
    //                 ttymode_remove_local_input(ISIG, 0);
    //                 mySignal(SIGINT, previntr);
    //                 mySignal(SIGQUIT, prevquit);
    //                 mySignal(SIGTSTP, prevstop);
    //             }
    //
    //             pushText(fileToDelete, tmpf);
    //         }
    //         url = tmpf;
    //     }
    //
    //     if (stat(url, &st))
    //         return;
    //
    //     fp = fopen(url, "r");
    //     if (!fp)
    //         return;
    //
    //     MOVE(y, x);
    //
    //     cbuf = GC_MALLOC_ATOMIC(3072); /* base64-encoded chunks of 4096 bytes */
    //     if (!cbuf)
    //         goto cleanup;
    //     i = 0;
    //
    //     while (i < 3072 && (c = fgetc(fp)) != EOF)
    //         cbuf[i++] = c;
    //
    //     base64 = base64_encode(cbuf, i);
    //
    //     if (c == EOF)
    //         m = 0;
    //     else
    //         m = 1;
    //     buf = Sprintf("\x1b_Gf=%d,s=%d,v=%d,a=T,m=%d,x=%d,y=%d,w=%d,h=%d,c=%d,r=%d;"
    //                   "%s\x1b\\",
    //         t, w, h, m, sx, sy, sw, sh, cols, rows, base64->ptr);
    //     writestr(buf->ptr);
    //
    //     if (m) {
    //         i = 0;
    //         j = 0;
    //         while ((c = fgetc(fp)) != EOF) {
    //             if (j) {
    //                 base64 = base64_encode(cbuf, i);
    //                 buf = Sprintf("\x1b_Gm=1;%s\x1b\\", base64->ptr);
    //                 writestr(buf->ptr);
    //                 i = 0;
    //                 j = 0;
    //             }
    //             cbuf[i++] = c;
    //             if (i == 3072)
    //                 j = 1;
    //         }
    //
    //         if (i) {
    //             base64 = base64_encode(cbuf, i);
    //             buf = Sprintf("\x1b_Gm=0;%s\x1b\\", base64->ptr);
    //             writestr(buf->ptr);
    //         }
    //     }
    // cleanup:
    //     fclose(fp);
    //     MOVE(Currentbuf->cursorY, Currentbuf->cursorX);
}

pub export fn getch() u8 {
    return tty.?.getch();
}

export fn sleep_till_anykey(sec: c_int, purge: c_int) c_int {
    _ = sec;
    _ = purge;
    // fd_set rfd;
    // struct timeval tim;
    // int er, c, ret;
    // struct termios ioval;
    //
    // tcgetattr(tty, &ioval);
    // term_raw();
    //
    // tim.tv_sec = sec;
    // tim.tv_usec = 0;
    //
    // FD_ZERO(&rfd);
    // FD_SET(tty, &rfd);
    //
    // ret = select(tty + 1, &rfd, 0, 0, &tim);
    // if (ret > 0 && purge) {
    //     c = getch();
    //     if (c == ESC_CODE)
    //         skip_escseq();
    // }
    // er = tcsetattr(tty, TCSANOW, &ioval);
    // if (er == -1) {
    //     printf("Error occurred: errno=%d\n", errno);
    //     reset_error_exit(SIGNAL_ARGLIST);
    // }
    // return ret;
    return 1;
}

export fn clear_tty() void {
    flush_tty();

    if (tty) |t| {
        t.restore();
        if (t.stdin.handle != 2) {
            t.destroy();
            tty = null;
        }
    }
}

export fn bell() void {
    if (tty) |t| {
        t.putc(7) catch @panic("putc");
    }
}
