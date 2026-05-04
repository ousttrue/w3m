const std = @import("std");
const c = @import("c.zig").c;
const g = @import("global.zig");
const runtime = @import("runtime.zig");
const tty = @import("tty.zig");
const screen = @import("screen.zig");

const TerminalImage = struct {
    cache: *c.ImageCache,
    x: c_int,
    y: c_int,
    sx: c_int,
    sy: c_int,
    width: c_int,
    height: c_int,
};

var terminal_image: std.ArrayList(TerminalImage) = .initBuffer(&.{});

export fn initImage() void {
    _ = getCharSize();
}

fn getCharSize() bool {
    c.set_environ("W3M_TTY", tty.ttyname_tty());

    if (g.enable_inline_image != 0) {
        var ppc: c_int = undefined;
        var ppl: c_int = undefined;
        if (tty.tty_pixel_per_cell(&ppc, &ppl)) {
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
    // FILE* f = popen(tmp.ptr, "r");
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

export fn deinitImage() void {
    clearImage();
}

export fn clearImage() void {
    terminal_image.clearRetainingCapacity();
}

export fn addImage(_cache: ?*c.ImageCache, x: c_int, y: c_int, sx: c_int, sy: c_int, w: c_int, h: c_int) void {
    const cache = _cache orelse {
        return;
    };

    terminal_image.append(runtime.allocator, .{
        .cache = cache,
        .x = x,
        .y = y,
        .sx = sx,
        .sy = sy,
        .width = w,
        .height = h,
    }) catch @panic("OOM");
}

export fn drawImage() void {
    if (0 == g.enable_inline_image) {
        return;
    }

    const io = runtime.io;

    for (terminal_image.items) |*i| {
        // So this shouldn't ever happen, but if it does then at least let's
        // not have external programs fetch images from the Internet...
        if (i.cache.touch == null) {
            return;
        }

        const url = std.mem.span(i.cache.file);
        _ = std.Io.Dir.cwd().statFile(io, url, .{}) catch {
            // cache not exists
            return;
        };

        // term position: col, line
        const x: usize = @intCast(@divTrunc(i.x, g.pixel_per_char_i));
        const y: usize = @intCast(@divTrunc(i.y, g.pixel_per_line_i));

        //         int w = i.cache.a_width > 0 ? (
        //                                             (i.cache.width + i.x % pixel_per_char_i + pixel_per_char_i - 1) / pixel_per_char_i)
        //                                       : 0;
        //         int h = i.cache.a_height > 0 ? (
        //                                              (i.cache.height + i.y % pixel_per_line_i + pixel_per_line_i - 1) / pixel_per_line_i)
        //                                        : 0;
        //
        //         int sx = i.sx / pixel_per_char_i;
        //         int sy = i.sy / pixel_per_line_i;

        const sw = @divTrunc((i.width + @mod(i.sx, g.pixel_per_char_i) + g.pixel_per_char_i - 1), g.pixel_per_char_i);
        const sh = @divTrunc((i.height + @mod(i.sy, g.pixel_per_line_i) + g.pixel_per_line_i - 1), g.pixel_per_line_i);

        switch (g.enable_inline_image) {
            c.INLINE_IMG_SIXEL => {
                // w = i.cache.a_width > 0 ? i.width : 0;
                // h = i.cache.a_height > 0 ? i.height : 0;
                // put_image_sixel(url, x, y, w, h, i.sx, i.sy, sw * pixel_per_char, sh * pixel_per_line_i, n_terminal_image);
            },
            c.INLINE_IMG_OSC5379 => {
                // put_image_osc5379(url, x, y, w, h, sx, sy, sw, sh);
            },
            c.INLINE_IMG_ITERM2 => {
                // put_image_iterm2(url, x, y, sw, sh);
            },
            c.INLINE_IMG_KITTY => {
                put_image_kitty(
                    runtime.allocator,
                    runtime.io,
                    url,
                    x,
                    y,
                    i.width,
                    i.height,
                    i.sx,
                    i.sy,
                    @intFromFloat(sw * g.pixel_per_char),
                    sh * g.pixel_per_line_i,
                    sw,
                    sh,
                ) catch |e| {
                    std.log.err("put_image_kitty => {}", .{e});
                };
            },
            else => unreachable,
        }
    }

    terminal_image.clearRetainingCapacity();

    // touch_cursor();
    c.tty_write_sc();
}

fn allocKittyConvertCmd(
    allocator: std.mem.Allocator,
    url: []const u8,
    content_type: []const u8,
    tmpf: []const u8,
) ![:0]const u8 {
    var arena: std.heap.ArenaAllocator = .init(allocator);
    defer arena.deinit();
    const arena_allocator = arena.allocator();

    var cmds: std.ArrayList([]const u8) = .initBuffer(&.{});

    // arg0
    const _cbuf = std.c.getenv("W3M_KITTY_TO_PNG");
    if (_cbuf) |cbuf| {
        try cmds.append(arena_allocator, std.mem.span(cbuf));
    } else {
        try cmds.append(arena_allocator, "magick");
        // try cmds.append(arena_allocator, "convert");
    }

    // arg1
    if (std.ascii.endsWithIgnoreCase(content_type, "image/gif")) {
        try cmds.append(arena_allocator, try std.fmt.allocPrint(arena_allocator, "{s}[0]", .{url}));
    } else {
        try cmds.append(arena_allocator, url);
    }

    // arg2
    try cmds.append(arena_allocator, tmpf);

    // join
    return try std.mem.joinZ(allocator, " ", cmds.items);
}

fn put_image_kitty(
    allocator: std.mem.Allocator,
    io: std.Io,
    _url: [:0]const u8,
    x: usize,
    y: usize,
    w: c_int,
    h: c_int,
    sx: c_int,
    sy: c_int,
    sw: c_int,
    sh: c_int,
    cols: c_int,
    rows: c_int,
) !void {
    var url = _url;

    const content_type = std.mem.span(c.guessContentType(url.ptr));

    const cwd = std.Io.Dir.cwd();
    if (!std.ascii.eqlIgnoreCase(content_type, "image/png")) {
        // not png. convert to tmpf
        const tmpf = try std.fmt.allocPrintSentinel(
            allocator,
            "{s}/{s}.png",
            .{ g.tmp_dir, c.fpath_basename(url) },
            0,
        );

        _ = cwd.statFile(io, tmpf, .{}) catch {
            // convert only if tmpf not exists yet
            const cmd = try allocKittyConvertCmd(allocator, url, content_type, tmpf);
            defer allocator.free(cmd);

            tty.tty_flush();
            const status = c.system(cmd.ptr);
            if (status != 0) {
                std.log.warn("system: {s} => {}", .{ cmd, status });
            }

            c.addDeleteFile(tmpf);
        };

        url = tmpf;
    }

    const fp = cwd.openFile(io, url, .{}) catch {
        return;
    };
    defer fp.close(io);

    var read_buf: [1]u8 = undefined;
    var r = fp.reader(io, &read_buf);

    screen.sc_move(y, x);
    // defer screen.sc_move(c.Currentbuf.cursorY, c.Currentbuf.cursorX);

    // base64-encoded chunks of 4096 bytes
    const cbuf = allocator.alloc(u8, 3072) catch @panic("OOM");
    defer allocator.free(cbuf);

    var i: usize = 0;
    var is_end = false;
    while (i < 3072) : (i += 1) {
        var buf: [1]u8 = undefined;
        r.interface.readSliceAll(&buf) catch |e| {
            switch (e) {
                error.EndOfStream => {
                    is_end = true;
                    break;
                },
                error.ReadFailed => {
                    @panic("ReadFailed");
                },
            }
        };
        cbuf[i] = buf[0];
    }

    const _base64 = c.base64_encode(cbuf.ptr, i);
    const base64 = _base64.*.ptr[0..@intCast(_base64.*.length)];

    const buf = try std.fmt.allocPrint(allocator, "\x1b_Gf={},s={},v={},a=T,m={},x={},y={},w={},h={},c={},r={};{s}\x1b\\", .{
        100,
        w,
        h,
        @as(c_int, if (is_end) 0 else 1),
        sx,
        sy,
        sw,
        sh,
        cols,
        rows,
        base64,
    });
    tty.tty_write(buf.ptr, buf.len);

    if (!is_end) {
        //         i = 0;
        //         int j = 0;
        //         while ((c = fgetc(fp)) != EOF) {
        //             if (j) {
        //                 base64 = base64_encode(cbuf, i);
        //                 buf = Sprintf("\x1b_Gm=1;%s\x1b\\", base64.ptr);
        //                 writestr(&tty_write1, buf.ptr);
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
        //             buf = Sprintf("\x1b_Gm=0;%s\x1b\\", base64.ptr);
        //             writestr(&tty_write1, buf.ptr);
        //         }
    }
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
    //     writestr(buf.ptr);
    //
    //     cbuf = GC_MALLOC_ATOMIC(3072);
    //     if (!cbuf)
    //         goto cleanup;
    //     i = 0;
    //     while ((c = fgetc(fp)) != EOF) {
    //         cbuf[i++] = c;
    //         if (i == 3072) {
    //             buf = base64_encode(cbuf, i);
    //             writestr(buf.ptr);
    //             i = 0;
    //         }
    //     }
    //
    //     if (i) {
    //         buf = base64_encode(cbuf, i);
    //         writestr(buf.ptr);
    //     }
    //
    // cleanup:
    //     fclose(fp);
    //     writestr("\a");
    //     MOVE(Currentbuf.cursorY, Currentbuf.cursorX);
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
//     //             url = str_url.ptr;
//     //         }
//     //         ttymode_add_local_input(ISIG, 0);
//     //
//     //         if ((env = getenv("W3M_IMG2SIXEL"))) {
//     //             char* p;
//     //             env = Strnew_charp(env).ptr;
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
//     //     MOVE(Currentbuf.cursorY, Currentbuf.cursorX);
// }
//
// void put_image_osc5379(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh)
// {
//     Str buf;
//     char* size;
//
//     if (w > 0 && h > 0)
//         size = Sprintf("%dx%d", w, h).ptr;
//     else
//         size = "";
//
//     MOVE(&tty_write1, &terminfo, y, x);
//     buf = Sprintf("\x1b]5379;show_picture %s %s %dx%d+%d+%d\x07", url, size, sw, sh, sx, sy);
//     writestr(&tty_write1, buf.ptr);
//     MOVE(&tty_write1, &terminfo, Currentbuf.cursorY, Currentbuf.cursorX);
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
//     writestr(&tty_write1, buf.ptr);
//
//     cbuf = GC_MALLOC_ATOMIC(3072);
//     if (!cbuf)
//         goto cleanup;
//     i = 0;
//     while ((c = fgetc(fp)) != EOF) {
//         cbuf[i++] = c;
//         if (i == 3072) {
//             buf = base64_encode(cbuf, i);
//             writestr(&tty_write1, buf.ptr);
//             i = 0;
//         }
//     }
//
//     if (i) {
//         buf = base64_encode(cbuf, i);
//         writestr(&tty_write1, buf.ptr);
//     }
//
// cleanup:
//     fclose(fp);
//     writestr(&tty_write1, "\a");
//     MOVE(&tty_write1, &terminfo, Currentbuf.cursorY, Currentbuf.cursorX);
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
//     if (stat(new_path.ptr, &st) == 0) {
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
//                 save_gif(new_path.ptr, header, header_size, body, p - 3 - body);
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
//             url = str_url.ptr;
//         }
//         ttymode_add(ISIG, 0);
//
//         if ((env = getenv("W3M_IMG2SIXEL"))) {
//             char* p;
//             env = Strnew_charp(env).ptr;
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
//     MOVE(&tty_write1, &terminfo, Currentbuf.cursorY, Currentbuf.cursorX);
// }

// static uint8_t*
// skip_gif_header(uint8_t* p)
// {
//     /* Header */
//     p += 10;
//
//     if (*(p) & 0x80) {
//         p += (3 * (2 << ((*p) & 0x7)));
//     }
//     p += 3;
//
//     return p;
// }

fn parseImageHeader(io: std.Io, path: []const u8) !c.Usize2 {
    const fp = std.Io.Dir.cwd().openFile(io, path, .{}) catch {
        return error.file_not_found;
    };
    defer fp.close(io);

    var read_buf: [8]u8 = undefined;
    var reader = fp.reader(io, &read_buf);

    const ch = reader.interface.peekByte() catch {
        return error.file_read_error;
    };

    return switch (ch) {
        0xff => parseJpegHeader(&reader.interface),
        'G' => parseGifHeader(&reader.interface),
        0x89 => parsePngHeader(&reader.interface),
        else => error.unknown_image,
    };
}

/// https://github.com/corkami/formats/blob/master/image/jpeg.md
///
/// SOI: 0xff 0xd8
/// [marker segments]*
fn parseJpegHeader(r: *std.Io.Reader) !c.Usize2 {
    var SOI: [2]u8 = undefined;
    try r.readSliceAll(&SOI);
    if (!std.mem.eql(u8, &SOI, &[2]u8{ 0xff, 0xd8 })) {
        return error.not_jpeg;
    }

    while (true) {
        var marker: [2]u8 = undefined;
        try r.readSliceAll(&marker);
        std.debug.assert(marker[0] == 0xff);

        var size: [1]u16 = undefined;
        try r.readSliceEndian(u16, &size, .big);

        switch (marker[1]) {
            0xC0, 0xC2 => {
                // SOF0: Baseline DCT
                // SOF2:
                try r.discardAll(1);
                var height: [1]u16 = undefined;
                try r.readSliceEndian(u16, &height, .big);
                var width: [1]u16 = undefined;
                try r.readSliceEndian(u16, &width, .big);
                return .{ .x = width[0], .y = height[0] };
            },
            else => {
                // skip marker segment body
                try r.discardAll(size[0] - 2);
            },
        }
    }

    unreachable;
}

// https://www.tohoho-web.com/wwwgif.htm#GIFHeader
fn parseGifHeader(r: *std.Io.Reader) !c.Usize2 {
    var magic: [3]u8 = undefined;
    try r.readSliceAll(&magic);
    if (!std.mem.eql(u8, &magic, "GIF")) {
        return error.not_gif;
    }

    var version: [3]u8 = undefined;
    try r.readSliceAll(&version);
    if (!std.mem.eql(u8, &version, "87a") and !std.mem.eql(u8, &version, "89a")) {
        return error.unknown_gif_version;
    }

    var width: [1]u16 = undefined;
    try r.readSliceEndian(u16, &width, .little);
    var height: [1]u16 = undefined;
    try r.readSliceEndian(u16, &height, .little);

    return .{ .x = width[0], .y = height[0] };
}

fn parsePngHeader(r: *std.Io.Reader) !c.Usize2 {
    var header: [8]u8 = undefined;
    try r.readSliceAll(&header);
    if (!std.mem.eql(u8, &header, "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a")) {
        return error.not_png;
    }

    // chunk_length(4), chunk_type(4)
    try r.discardAll(8);

    var width: [1]u32 = undefined;
    try r.readSliceEndian(u32, &width, .big);
    var height: [1]u32 = undefined;
    try r.readSliceEndian(u32, &height, .big);

    return .{ .x = width[0], .y = height[0] };
}

export fn getImageSize(_cache: ?*c.ImageCache) bool {
    const cache = _cache orelse {
        return false;
    };

    if (0 == (cache.loaded & c.IMG_FLAG_LOADED) or (cache.width > 0 and cache.height > 0)) {
        return false;
    }

    const size = parseImageHeader(runtime.io, std.mem.span(cache.file)) catch {
        return false;
    };

    var w: c_int = @intFromFloat((@as(f64, @floatFromInt(size.x)) * g.image_scale / 100.0 + 0.5));
    if (w == 0)
        w = 1;

    var h: c_int = @intFromFloat((@as(f64, @floatFromInt(size.y)) * g.image_scale / 100.0 + 0.5));
    if (h == 0)
        h = 1;

    if (cache.width < 0 and cache.height < 0) {
        cache.width = if (w > c.MAX_IMAGE_SIZE) c.MAX_IMAGE_SIZE else w;
        cache.height = if (h > c.MAX_IMAGE_SIZE) c.MAX_IMAGE_SIZE else h;
    } else if (cache.width < 0) {
        const tmp = @as(f64, @floatFromInt(cache.height * @divTrunc(w, h))) + 0.5;
        cache.width = if (tmp > c.MAX_IMAGE_SIZE) c.MAX_IMAGE_SIZE else @intFromFloat(tmp);
        cache.a_width = cache.width;
    } else if (cache.height < 0) {
        const tmp = @as(f64, @floatFromInt(cache.width * @divTrunc(h, w))) + 0.5;
        cache.height = if (tmp > c.MAX_IMAGE_SIZE) c.MAX_IMAGE_SIZE else @intFromFloat(tmp);
        cache.a_height = cache.height;
    }
    if (cache.width == 0)
        cache.width = 1;
    if (cache.height == 0)
        cache.height = 1;

    // Str tmp = Sprintf("%d;%d;%s", cache.width, cache.height, cache.url);
    // putHash_sv(image_hash, tmp.ptr, (void*)cache);

    return true;
}
