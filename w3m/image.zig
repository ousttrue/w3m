const std = @import("std");
const c = @import("c.zig").c;
const g = @import("global.zig");
const tty = @import("tty.zig");

export fn initImage() void {
    if (0 == g.activeImage) {
        if (getCharSize()) {
            g.activeImage = 1;
        }
    }
}

export fn tty_pixel_per_cell(ppc: *c_int, ppl: *c_int) bool {
    if (tty.getTermSize()) |ws| {
        if (ws.ws_ypixel > 0 and ws.ws_row > 0 and ws.ws_xpixel > 0 and ws.ws_col > 0) {
            ppc.* = ws.ws_xpixel / ws.ws_col;
            ppl.* = ws.ws_ypixel / ws.ws_row;
            return true;
        }
    } else |_| {
        @panic("getTermSize");
    }

    // XTWINOPS
    //
    // fd_set rfd;
    // struct timeval tval;
    // char buf[100];
    // char* p;
    // ssize_t len;
    // ssize_t left;
    // int wp, hp, wc, hc;
    // int i;
    //
    // fputs("\x1b[14t\x1b[18t", ttyf);
    // tty_flush();
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

    return false;
}

fn getCharSize() bool {
    c.set_environ("W3M_TTY", tty.ttyname_tty());

    if (g.enable_inline_image != 0) {
        var ppc: c_int = undefined;
        var ppl: c_int = undefined;
        if (tty_pixel_per_cell(&ppc, &ppl)) {
            g.pixel_per_char_i = ppc;
            g.pixel_per_line_i = ppl;
            g.pixel_per_char = @floatFromInt(ppc);
            g.pixel_per_line = @floatFromInt(ppl);
        } else {
            g.pixel_per_char_i = @intFromFloat(g.pixel_per_char);
            g.pixel_per_line_i = @intFromFloat(g.pixel_per_line);
        }
        return true;
    }

    // Str tmp = Strnew();
    // if (!strchr(Imgdisplay, '/'))
    //     Strcat_m_charp(tmp, w3m_auxbin_dir(), "/", NULL);
    // Strcat_m_charp(tmp, Imgdisplay, " -test 2>/dev/null", NULL);
    // FILE* f = popen(tmp->ptr, "r");
    // if (!f)
    //     return false;
    //
    // int w = 0, h = 0;
    // while (fscanf(f, "%d %d", &w, &h) < 0) {
    //     if (feof(f))
    //         break;
    // }
    // pclose(f);
    //
    // if (!(w > 0 && h > 0))
    //     return false;
    // if (!set_pixel_per_char)
    //     pixel_per_char = (int)(1.0 * w / COLS + 0.5);
    // if (!set_pixel_per_line)
    //     pixel_per_line = (int)(1.0 * h / LINES + 0.5);
    return false;
}

const TerminalImage = struct {
    //     struct ImageCache* cache;
    //     short x;
    //     short y;
    //     short sx;
    //     short sy;
    //     short width;
    //     short height;
};

// static struct TerminalImage* terminal_image = NULL;
var n_terminal_image: usize = 0;
// static int max_terminal_image = 0;

export fn deinitImage() void {
    clearImage();
}

export fn clearImage() void {
    n_terminal_image = 0;
}

export fn addImage(cache: ?*c.ImageCache, x: c_int, y: c_int, sx: c_int, sy: c_int, w: c_int, h: c_int) void {
    _ = cache;
    _ = x;
    _ = y;
    _ = sx;
    _ = sy;
    _ = w;
    _ = h;

    // if (!activeImage)
    //     return;
    //
    // if (n_terminal_image >= max_terminal_image) {
    //     max_terminal_image = max_terminal_image ? (2 * max_terminal_image) : 8;
    //     terminal_image = New_Reuse(struct TerminalImage, terminal_image,
    //         max_terminal_image);
    // }
    //
    // struct TerminalImage* i = &terminal_image[n_terminal_image];
    // *i = (struct TerminalImage) {
    //     .cache = cache,
    //     .x = x,
    //     .y = y,
    //     .sx = sx,
    //     .sy = sy,
    //     .width = w,
    //     .height = h,
    // };
    // n_terminal_image++;
}

export fn drawImage() void {
    // if (!activeImage)
    //     return;
    // if (!n_terminal_image)
    //     return;
    //
    // for (int j = 0; j < n_terminal_image; j++) {
    //     struct TerminalImage* i = &terminal_image[j];
    //     if (enable_inline_image) {
    //         /*
    //          * So this shouldn't ever happen, but if it does then at least let's
    //          * not have external programs fetch images from the Internet...
    //          */
    //         struct stat st;
    //         if (!i->cache->touch || stat(i->cache->file, &st))
    //             return;
    //
    //         const char* url = i->cache->file;
    //
    //         int x = i->x / pixel_per_char_i;
    //         int y = i->y / pixel_per_line_i;
    //
    //         int w = i->cache->a_width > 0 ? (
    //                                             (i->cache->width + i->x % pixel_per_char_i + pixel_per_char_i - 1) / pixel_per_char_i)
    //                                       : 0;
    //         int h = i->cache->a_height > 0 ? (
    //                                              (i->cache->height + i->y % pixel_per_line_i + pixel_per_line_i - 1) / pixel_per_line_i)
    //                                        : 0;
    //
    //         int sx = i->sx / pixel_per_char_i;
    //         int sy = i->sy / pixel_per_line_i;
    //
    //         int sw = (i->width + i->sx % pixel_per_char_i + pixel_per_char_i - 1) / pixel_per_char_i;
    //         int sh = (i->height + i->sy % pixel_per_line_i + pixel_per_line_i - 1) / pixel_per_line_i;
    //
    //         if (enable_inline_image == INLINE_IMG_SIXEL) {
    //             w = i->cache->a_width > 0 ? i->width : 0;
    //             h = i->cache->a_height > 0 ? i->height : 0;
    //             put_image_sixel(url, x, y, w, h, i->sx, i->sy, sw * pixel_per_char, sh * pixel_per_line_i, n_terminal_image);
    //         } else if (enable_inline_image == INLINE_IMG_OSC5379) {
    //             put_image_osc5379(url, x, y, w, h, sx, sy, sw, sh);
    //         } else if (enable_inline_image == INLINE_IMG_ITERM2) {
    //             put_image_iterm2(url, x, y, sw, sh);
    //         } else if (enable_inline_image == INLINE_IMG_KITTY) {
    //             put_image_kitty(url, x, y, i->width, i->height, i->sx, i->sy, sw * pixel_per_char, sh * pixel_per_line_i, sw, sh);
    //         }
    //
    //         continue;
    //     }
    // }
    //
    // n_terminal_image = 0;
    //
    // touch_cursor();
    // tty_write_sc();
}

// void put_image_kitty(const char* url, int x, int y, int w, int h, int sx, int sy, int sw,
//     int sh, int cols, int rows)
// {
//     if (!url)
//         return;
//
//     const char* type = guessContentType(url);
//     // always convert to png for now.
//     int t = 100;
//
//     // Str buf, base64;
//     // FILE* fp;
//     // int c, i, j, m, t;
//
//     if (!(type && !strcasecmp(type, "image/png"))) {
//         char* tmpf = Sprintf("%s/%s.png", tmp_dir, fpath_basename(url))->ptr;
//
//         bool is_anim = type && !strcasecmp(type, "image/gif");
//
//         // convert only if png doesn't exist yet.
//
//         struct stat st;
//         if (stat(tmpf, &st)) {
//             if (stat(url, &st))
//                 return;
//
//             tty_flush();
//
//             SignalFunc previntr = signal(SIGINT, SIG_IGN);
//             SignalFunc prevquit = signal(SIGQUIT, SIG_IGN);
//             SignalFunc prevstop = signal(SIGTSTP, SIG_IGN);
//
//             pid_t pid = fork();
//             if (pid == 0) {
//                 int i = 0;
//
//                 close(STDERR_FILENO); /* Don't output error message. */
//                 ttymode_add(ISIG, 0);
//
//                 char* argv[4];
//                 char* cbuf;
//                 if ((cbuf = getenv("W3M_KITTY_TO_PNG")))
//                     argv[i++] = cbuf;
//                 else
//                     argv[i++] = "convert";
//
//                 if (is_anim) {
//                     Str buf = Strnew_charp(url);
//                     Strcat_charp(buf, "[0]");
//                     argv[i++] = buf->ptr;
//                 } else {
//                     argv[i++] = (char*)url;
//                 }
//                 argv[i++] = tmpf;
//                 argv[i++] = NULL;
//                 execvp(argv[0], argv);
//                 exit(0);
//             } else if (pid > 0) {
//                 int i;
//                 waitpid(pid, &i, 0);
//                 ttymode_remove(ISIG, 0);
//                 signal(SIGINT, previntr);
//                 signal(SIGQUIT, prevquit);
//                 signal(SIGTSTP, prevstop);
//             }
//
//             addDeleteFile(tmpf);
//         }
//         url = tmpf;
//     }
//
//     struct stat st;
//     if (stat(url, &st))
//         return;
//
//     FILE* fp = fopen(url, "r");
//     if (!fp)
//         return;
//
//     MOVE(&tty_write1, &terminfo, y, x);
//
//     char* cbuf = malloc(3072); /* base64-encoded chunks of 4096 bytes */
//     if (!cbuf)
//         goto cleanup;
//     int i = 0;
//
//     int c;
//     while (i < 3072 && (c = fgetc(fp)) != EOF)
//         cbuf[i++] = c;
//
//     Str base64 = base64_encode(cbuf, i);
//     int m;
//     if (c == EOF)
//         m = 0;
//     else
//         m = 1;
//     Str buf = Sprintf("\x1b_Gf=%d,s=%d,v=%d,a=T,m=%d,x=%d,y=%d,w=%d,h=%d,c=%d,r=%d;"
//                       "%s\x1b\\",
//         t, w, h, m, sx, sy, sw, sh, cols, rows, base64->ptr);
//     writestr(&tty_write1, buf->ptr);
//
//     if (m) {
//         i = 0;
//         int j = 0;
//         while ((c = fgetc(fp)) != EOF) {
//             if (j) {
//                 base64 = base64_encode(cbuf, i);
//                 buf = Sprintf("\x1b_Gm=1;%s\x1b\\", base64->ptr);
//                 writestr(&tty_write1, buf->ptr);
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
//             writestr(&tty_write1, buf->ptr);
//         }
//     }
// cleanup:
//     fclose(fp);
//     MOVE(&tty_write1, &terminfo, Currentbuf->cursorY, Currentbuf->cursorX);
// }
// export fn put_image_kitty(
//     url: [*c]const u8,
//     x: c_int,
//     y: c_int,
//     w: c_int,
//     h: c_int,
//     sx: c_int,
//     sy: c_int,
//     sw: c_int,
//     sh: c_int,
//     cols: c_int,
//     rows: c_int,
// ) void {
//     _ = x;
//     _ = y;
//     _ = w;
//     _ = h;
//     _ = sx;
//     _ = sy;
//     _ = sw;
//     _ = sh;
//     _ = cols;
//     _ = rows;
//     //     Str buf, base64;
//     //     char *cbuf, *tmpf;
//     //     char* argv[4];
//     //     FILE* fp;
//     //     int c, i, j, m, t, is_anim;
//     //     struct stat st;
//     //     pid_t pid;
//     //     MySignalHandler (*volatile previntr)(SIGNAL_ARG);
//     //     MySignalHandler (*volatile prevquit)(SIGNAL_ARG);
//     //     MySignalHandler (*volatile prevstop)(SIGNAL_ARG);
//
//     const content_type = std.mem.span(guessContentType(url));
//     // const t = 100; // always convert to png for now.
//     const path = std.mem.span(url);
//
//     if (!std.ascii.eqlIgnoreCase(content_type, "image/png")) {
//         // conv to png
//         //         tmpf = Sprintf("%s/%s.png", tmp_dir, mybasename(path))->ptr;
//         //
//         //         if (type && !strcasecmp(type, "image/gif")) {
//         //             is_anim = 1;
//         //         } else {
//         //             is_anim = 0;
//         //         }
//         //
//         //         /* convert only if png doesn't exist yet. */
//         //
//         //         if (stat(tmpf, &st)) {
//         //             if (stat(path, &st))
//         //                 return;
//         //
//         //             tty_flush();
//         //
//         //             previntr = signal(SIGINT, SIG_IGN);
//         //             prevquit = signal(SIGQUIT, SIG_IGN);
//         //             prevstop = signal(SIGTSTP, SIG_IGN);
//         //
//         //             if ((pid = fork()) == 0) {
//         //                 i = 0;
//         //
//         //                 close(STDERR_FILENO); /* Don't output error message. */
//         //                 ttymode_add_local_input(ISIG, 0);
//         //
//         //                 if ((cbuf = getenv("W3M_KITTY_TO_PNG")))
//         //                     argv[i++] = cbuf;
//         //                 else
//         //                     argv[i++] = "convert";
//         //
//         //                 if (is_anim) {
//         //                     buf = Strnew_charp(path);
//         //                     Strcat_charp(buf, "[0]");
//         //                     argv[i++] = buf->ptr;
//         //                 } else {
//         //                     argv[i++] = path;
//         //                 }
//         //                 argv[i++] = tmpf;
//         //                 argv[i++] = NULL;
//         //                 execvp(argv[0], argv);
//         //                 exit(0);
//         //             } else if (pid > 0) {
//         //                 waitpid(pid, &i, 0);
//         //                 ttymode_remove_local_input(ISIG, 0);
//         //                 signal(SIGINT, previntr);
//         //                 signal(SIGQUIT, prevquit);
//         //                 signal(SIGTSTP, prevstop);
//         //             }
//         //
//         //             pushText(fileToDelete, tmpf);
//         //         }
//         //         path = tmpf;
//     }
//
//     const f = std.Io.Dir.cwd().openFile(runtime.io, path, .{}) catch {
//         return;
//     };
//     defer f.close(runtime.io);
//
//     // MOVE(y, x);
//
//     //     cbuf = GC_MALLOC_ATOMIC(3072); /* base64-encoded chunks of 4096 bytes */
//     //     if (!cbuf)
//     //         goto cleanup;
//     //     i = 0;
//     //
//     //     while (i < 3072 && (c = fgetc(fp)) != EOF)
//     //         cbuf[i++] = c;
//     //
//     //     base64 = base64_encode(cbuf, i);
//     //
//     //     if (c == EOF)
//     //         m = 0;
//     //     else
//     //         m = 1;
//     //     buf = Sprintf("\x1b_Gf=%d,s=%d,v=%d,a=T,m=%d,x=%d,y=%d,w=%d,h=%d,c=%d,r=%d;"
//     //                   "%s\x1b\\",
//     //         t, w, h, m, sx, sy, sw, sh, cols, rows, base64->ptr);
//     //     writestr(buf->ptr);
//     //
//     //     if (m) {
//     //         i = 0;
//     //         j = 0;
//     //         while ((c = fgetc(fp)) != EOF) {
//     //             if (j) {
//     //                 base64 = base64_encode(cbuf, i);
//     //                 buf = Sprintf("\x1b_Gm=1;%s\x1b\\", base64->ptr);
//     //                 writestr(buf->ptr);
//     //                 i = 0;
//     //                 j = 0;
//     //             }
//     //             cbuf[i++] = c;
//     //             if (i == 3072)
//     //                 j = 1;
//     //         }
//     //
//     //         if (i) {
//     //             base64 = base64_encode(cbuf, i);
//     //             buf = Sprintf("\x1b_Gm=0;%s\x1b\\", base64->ptr);
//     //             writestr(buf->ptr);
//     //         }
//     //     }
//     // cleanup:
//     //     fclose(fp);
//     //     MOVE(Currentbuf->cursorY, Currentbuf->cursorX);
// }

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

// export fn put_image_sixel(
//     url: [*c]const u8,
//     x: c_int,
//     y: c_int,
//     w: c_int,
//     h: c_int,
//     sx: c_int,
//     sy: c_int,
//     sw: c_int,
//     sh: c_int,
//     n_terminal_image: c_int,
// ) void {
//     _ = url;
//     _ = x;
//     _ = y;
//     _ = w;
//     _ = h;
//     _ = sx;
//     _ = sy;
//     _ = sw;
//     _ = sh;
//     _ = n_terminal_image;
//     //     pid_t pid;
//     //     int do_anim;
//     //     MySignalHandler (*volatile previntr)(SIGNAL_ARG);
//     //     MySignalHandler (*volatile prevquit)(SIGNAL_ARG);
//     //     MySignalHandler (*volatile prevstop)(SIGNAL_ARG);
//     //
//     //     MOVE(y, x);
//     //     tty_flush();
//     //
//     //     do_anim = (n_terminal_image == 1 && x == 0 && y == 0 && sx == 0 && sy == 0);
//     //
//     //     previntr = signal(SIGINT, SIG_IGN);
//     //     prevquit = signal(SIGQUIT, SIG_IGN);
//     //     prevstop = signal(SIGTSTP, SIG_IGN);
//     //
//     //     if ((pid = fork()) == 0) {
//     //         char* env;
//     //         int n = 0;
//     //         char* argv[20];
//     //         char digit[2][11 + 1];
//     //         char clip[44 + 3 + 1];
//     //         Str str_url;
//     //
//     //         close(STDERR_FILENO); /* Don't output error message. */
//     //         if (do_anim) {
//     //             writestr("\x1b[?80h");
//     //         } else if (!strstr(url, "://") && strcmp(url + strlen(url) - 4, ".gif") == 0 && (str_url = save_first_animation_frame(url))) {
//     //             url = str_url->ptr;
//     //         }
//     //         ttymode_add_local_input(ISIG, 0);
//     //
//     //         if ((env = getenv("W3M_IMG2SIXEL"))) {
//     //             char* p;
//     //             env = Strnew_charp(env)->ptr;
//     //             while (n < 8 && (p = strchr(env, ' '))) {
//     //                 *p = '\0';
//     //                 if (*env != '\0') {
//     //                     argv[n++] = env;
//     //                 }
//     //                 env = p + 1;
//     //             }
//     //             if (*env != '\0') {
//     //                 argv[n++] = env;
//     //             }
//     //         } else {
//     //             argv[n++] = "img2sixel";
//     //         }
//     //         argv[n++] = "-l";
//     //         argv[n++] = do_anim ? "auto" : "disable";
//     //         argv[n++] = "-w";
//     //         sprintf(digit[0], "%d", w);
//     //         argv[n++] = digit[0];
//     //         argv[n++] = "-h";
//     //         sprintf(digit[1], "%d", h);
//     //         argv[n++] = digit[1];
//     //         argv[n++] = "-c";
//     //         sprintf(clip, "%dx%d+%d+%d", sw, sh, sx, sy);
//     //         argv[n++] = clip;
//     //         argv[n++] = url;
//     //         if (getenv("TERM") && strcmp(getenv("TERM"), "screen") == 0 && (!getenv("SCREEN_VARIANT") || strcmp(getenv("SCREEN_VARIANT"), "sixel") != 0)) {
//     //             argv[n++] = "-P";
//     //         }
//     //         argv[n++] = NULL;
//     //         execvp(argv[0], argv);
//     //         exit(0);
//     //     } else if (pid > 0) {
//     //         int status;
//     //         waitpid(pid, &status, 0);
//     //         ttymode_remove_local_input(ISIG, 0);
//     //         signal(SIGINT, previntr);
//     //         signal(SIGQUIT, prevquit);
//     //         signal(SIGTSTP, prevstop);
//     //         if (do_anim) {
//     //             writestr("\x1b[?80l");
//     //         }
//     //     }
//     //
//     //     MOVE(Currentbuf->cursorY, Currentbuf->cursorX);
// }
//
// void put_image_osc5379(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh)
// {
//     Str buf;
//     char* size;
//
//     if (w > 0 && h > 0)
//         size = Sprintf("%dx%d", w, h)->ptr;
//     else
//         size = "";
//
//     MOVE(&tty_write1, &terminfo, y, x);
//     buf = Sprintf("\x1b]5379;show_picture %s %s %dx%d+%d+%d\x07", url, size, sw, sh, sx, sy);
//     writestr(&tty_write1, buf->ptr);
//     MOVE(&tty_write1, &terminfo, Currentbuf->cursorY, Currentbuf->cursorX);
// }

// void put_image_iterm2(const char* url, int x, int y, int w, int h)
// {
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
//     MOVE(&tty_write1, &terminfo, y, x);
//
//     writestr(&tty_write1, buf->ptr);
//
//     cbuf = GC_MALLOC_ATOMIC(3072);
//     if (!cbuf)
//         goto cleanup;
//     i = 0;
//     while ((c = fgetc(fp)) != EOF) {
//         cbuf[i++] = c;
//         if (i == 3072) {
//             buf = base64_encode(cbuf, i);
//             writestr(&tty_write1, buf->ptr);
//             i = 0;
//         }
//     }
//
//     if (i) {
//         buf = base64_encode(cbuf, i);
//         writestr(&tty_write1, buf->ptr);
//     }
//
// cleanup:
//     fclose(fp);
//     writestr(&tty_write1, "\a");
//     MOVE(&tty_write1, &terminfo, Currentbuf->cursorY, Currentbuf->cursorX);
// }

// static void
// save_gif(const char* path, uint8_t* header, size_t header_size, uint8_t* body, size_t body_size)
// {
//     int fd = open(path, O_WRONLY | O_CREAT, 0600);
//     if (fd >= 0) {
//         write(fd, header, header_size);
//         write(fd, body, body_size);
//         write(fd, "\x3b", 1);
//         close(fd);
//     }
// }

// static Str
// save_first_animation_frame(const char* path)
// {
//     int fd;
//     struct stat st;
//     uint8_t* header;
//     size_t header_size;
//     uint8_t* body;
//     uint8_t* p;
//     ssize_t len;
//     Str new_path;
//
//     new_path = Strnew_charp(path);
//     Strcat_charp(new_path, "-1");
//     if (stat(new_path->ptr, &st) == 0) {
//         return new_path;
//     }
//
//     if ((fd = open(path, O_RDONLY)) < 0) {
//         return NULL;
//     }
//
//     if (fstat(fd, &st) != 0 || !(header = malloc(st.st_size))) {
//         close(fd);
//         return NULL;
//     }
//
//     len = read(fd, header, st.st_size);
//     close(fd);
//
//     /* Header */
//
//     if (len != st.st_size || strncmp((char*)header, "GIF89a", 6) != 0) {
//         return NULL;
//     }
//
//     p = skip_gif_header(header);
//     header_size = p - header;
//
//     /* Application Extension */
//     if (p[0] == 0x21 && p[1] == 0xff) {
//         p += 19;
//     }
//
//     /* Other blocks */
//     body = NULL;
//     while (p + 2 < header + st.st_size) {
//         if (*(p++) == 0x21 && *(p++) == 0xf9 && *(p++) == 0x04) {
//             if (body) {
//                 /* Graphic Control Extension */
//                 save_gif(new_path->ptr, header, header_size, body, p - 3 - body);
//                 return new_path;
//             } else {
//                 /* skip the first frame. */
//             }
//             body = p - 3;
//         }
//     }
//
//     return NULL;
// }
//
// void put_image_sixel(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int n_terminal_image)
// {
//     pid_t pid;
//     int do_anim;
//
//     MOVE(&tty_write1, &terminfo, y, x);
//     tty_flush();
//
//     do_anim = (n_terminal_image == 1 && x == 0 && y == 0 && sx == 0 && sy == 0);
//
//     SignalFunc previntr = signal(SIGINT, SIG_IGN);
//     SignalFunc prevquit = signal(SIGQUIT, SIG_IGN);
//     SignalFunc prevstop = signal(SIGTSTP, SIG_IGN);
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
//             writestr(&tty_write1, "\x1b[?80h");
//         } else if (!strstr(url, "://") && strcmp(url + strlen(url) - 4, ".gif") == 0 && (str_url = save_first_animation_frame(url))) {
//             url = str_url->ptr;
//         }
//         ttymode_add(ISIG, 0);
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
//         argv[n++] = (char*)url;
//         if (getenv("TERM") && strcmp(getenv("TERM"), "screen") == 0 && (!getenv("SCREEN_VARIANT") || strcmp(getenv("SCREEN_VARIANT"), "sixel") != 0)) {
//             argv[n++] = "-P";
//         }
//         argv[n++] = NULL;
//         execvp(argv[0], argv);
//         exit(0);
//     } else if (pid > 0) {
//         int status;
//         waitpid(pid, &status, 0);
//         ttymode_remove(ISIG, 0);
//         signal(SIGINT, previntr);
//         signal(SIGQUIT, prevquit);
//         signal(SIGTSTP, prevstop);
//         if (do_anim) {
//             writestr(&tty_write1, "\x1b[?80l");
//         }
//     }
//
//     MOVE(&tty_write1, &terminfo, Currentbuf->cursorY, Currentbuf->cursorX);
// }
