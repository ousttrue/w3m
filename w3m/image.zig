const std = @import("std");
const c = @import("c.zig").c;
const g = @import("global.zig");
const lib = @import("lib.zig");

export fn initImage() void {
    if (0 == g.activeImage) {
        if (getCharSize()) {
            g.activeImage = 1;
        }
    }
}

export fn get_pixel_per_cell(ppc: *c_int, ppl: *c_int) bool {
    if (lib.getTermSize()) |ws| {
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

    return false;
}

fn getCharSize() bool {
    c.set_environ("W3M_TTY", lib.ttyname_tty());

    if (g.enable_inline_image != 0) {
        var ppc: c_int = undefined;
        var ppl: c_int = undefined;
        if (get_pixel_per_cell(&ppc, &ppl)) {
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

// struct TerminalImage {
//     struct ImageCache* cache;
//     short x;
//     short y;
//     short sx;
//     short sy;
//     short width;
//     short height;
// };
//
// static struct TerminalImage* terminal_image = NULL;
var n_terminal_image: usize = 0;
// static int max_terminal_image = 0;

export fn termImage() void {
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
    // refresh();
}
