const std = @import("std");
const c = @import("c.zig").c;
const g = @import("global.zig");
pub const runtime = @import("runtime.zig");
pub const global = @import("global.zig");
// pub const keybind = @import("keybind.zig");
const TtyLinux = @import("TtyLinux.zig");
const content_type = @import("content_type.zig");
const guessContentType = content_type.guessContentType;
const terminfo_entry = @import("terminfo_entry.zig");
const defun = @import("defun.zig");
const Epoll = @import("Epoll.zig");
const config = @import("config");

const w3m_task = switch (config.task_backend) {
    .coroutine => @import("task_coroutine.zig"),
    .thread => @import("task_thread.zig"),
};

var tty: TtyLinux = undefined;
// blocking tty stdout
var tty_writer: std.Io.File.Writer = undefined;
var write_buf: [256]u8 = undefined;
var tty_in: std.Io.File = undefined;
// var tty_reader: std.Io.File.Reader = undefined;
// var read_buf: [16]u8 = undefined;
var peek_queue: std.Deque(u8) = .initBuffer(&.{});

// var evented: std.Io.Evented = undefined;
var evented: std.Io.Threaded = undefined;

var epoll: Epoll = undefined;

// var key_input_queue: std.Io.Queue(u8) = .init(&.{});
// fn producer(
//     io: std.Io,
//     queue: *std.Io.Queue(u8),
//     val: u8,
// ) !void {
//     try queue.putOne(io, val);
// }
// fn consumer(
//     io: std.Io,
//     queue: *std.Io.Queue(u8),
// ) ![]const u8 {
//     return queue.getOne(io);
// }

pub export fn _dummy_() void {
    // export symbols ?
    std.log.debug("{}", .{global});
    std.log.debug("{}", .{terminfo_entry});
    std.log.debug("{}", .{content_type});
}

comptime {
    std.testing.refAllDecls(@This());
}

pub fn init(process_init: std.process.Init) void {
    runtime.init(process_init);

    tty = .init(std.Io.File.stdin());

    tty_writer = std.Io.File.stdout().writer(process_init.io, &write_buf);

    // not work
    // evented.init(runtime.allocator, .{
    //     .argv0 = .empty,
    //     // .environ = runtime.environ_map,
    //     .backing_allocator_needs_mutex = false,
    // }) catch @panic("evented.init");
    evented = .init(runtime.allocator, .{});

    tty_in = std.Io.File.stdin();
    // tty_reader = std.Io.File.stdin().reader(evented.io(), &read_buf);

    w3m_task.tasks_init();

    epoll = .init();
    epoll.add_fd(tty_in.handle);
}

pub fn deinit() void {
    w3m_task.tasks_deinit();

    evented.deinit();
    flush_tty();
    runtime.deinit();
}

// write interfaces

fn putc(ch: u8) !void {
    try tty_writer.interface.writeByte(ch);
}

fn puts(str: []const u8) !void {
    return try tty_writer.interface.writeAll(str);
}

extern fn checkDownloadList() bool;
extern fn submitCurrentBuffer(args: *c.CmdArgs) bool;
extern fn processCurrentEvent() bool;
extern fn processCurrentBufferEvent() bool;
extern fn processResizeAndImage(args: *c.CmdArgs) void;
extern var GlobalKeymap: [128][*c]const u8;
extern fn setupCurrentBuffer() void;

export fn w3m_loop() c_int {
    root.begin();

    while (root.state() != .DEAD) {
        const has_input = epoll.next(80) catch {
            // error ?
            break;
        };

        if (has_input) |_| {
            var buf: [1]u8 = undefined;
            const readsize = std.posix.read(
                tty_in.handle,
                &buf,
            ) catch @panic("getch");
            std.debug.assert(readsize == 1);

            if (w3m_task.tasks_current()) |task| {
                task.enqueue(buf[0]);
                if (task.state() == .DEAD) {
                    w3m_task.tasks_pop();
                }
            } else {
                // root
                const ch: u8 = buf[0];
                //         last_key = c;
                //         if (CurrentAlarm->sec > 0) {
                //             alarm(0);
                //         }
                if (std.ascii.isAscii(ch)) {
                    if (('0' <= ch) and (ch <= '9') and (g.prec_num != 0 or std.mem.eql(u8, std.mem.span(GlobalKeymap[ch]), "NOTHING"))) {
                        g.prec_num = g.prec_num * 10 + (ch - '0');
                        if (g.prec_num > c.PREC_LIMIT)
                            g.prec_num = c.PREC_LIMIT;
                    } else {
                        setupCurrentBuffer();
                        g.CurrentKey = ch;
                        w3mFunc(GlobalKeymap[ch]);

                        g.prec_num = 0;
                    }
                }
                // g.prev_key = g.CurrentKey;
                g.CurrentKey = -1;
                g.CurrentKeyData = null;
            }
        } else {
            // timeout
            if (w3m_task.tasks_current()) |task| {
                task.enqueue(0);
                if (task.state() == .DEAD) {
                    w3m_task.tasks_pop();
                }
            } else {
                root.enqueue(0);
            }
        }
    }

    return 0;
}

export fn co_root(_args: ?*c.CmdArgs) void {
    const args: *c.CmdArgs = _args.?;
    while (g.is_running) {
        if (checkDownloadList()) {
            c.ldDL(null);
        }
        if (submitCurrentBuffer(args)) {
            continue;
        }
        if (processCurrentEvent()) {
            continue;
        }
        if (processCurrentBufferEvent()) {
            continue;
        }

        processResizeAndImage(args);

        const task: *w3m_task.W3mTask = @alignCast(@fieldParentPtr("args", args));
        _ = task.block(.fromMilliseconds(100), args);
    }
}

var root: w3m_task.W3mTask = .{
    .func = .{
        .func = co_root,
        .desc = "loop coroutine",
    },
    .args = .{},
    .co_id = -1,
};

export fn w3mFunc(cmd: [*c]const u8) void {
    const func = defun.getFunc(std.mem.span(cmd)) orelse {
        std.log.warn("{s} not found", .{cmd});
        return;
    };

    w3m_task.tasks_push(func, .{});
}

export fn ttyname_tty() [*c]const u8 {
    return c.ttyname(tty.stdin.handle);
}

export fn setlinescols() void {
    var wins: c.winsize = undefined;
    const i = c.ioctl(tty.stdin.handle, c.TIOCGWINSZ, &wins);
    if (i >= 0 and wins.ws_row != 0 and wins.ws_col != 0) {
        g.LINES = wins.ws_row;
        g.COLS = wins.ws_col;
    }
}

export fn ttymode_add(mode: c_int, imode: c_int) void {
    _ = mode;
    _ = imode;
}

export fn ttymode_remove(mode: c_int, imode: c_int) void {
    _ = mode;
    _ = imode;
}

export fn crmode() void {
    tty.crmode(true) catch {};
}

export fn term_echo() void {
    tty.echo(true) catch {};
}

export fn term_noecho() void {
    tty.echo(false) catch {};
}

export fn term_raw() void {
    tty.raw() catch {};
}

export fn term_cbreak() void {
    tty.cooked(.{ .echo = false }) catch {};
}

export fn term_title(s: [*c]const u8) void {
    // @panic("not impl");
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
    tty_writer.flush() catch {};
}

export fn clear_tty() void {
    flush_tty();
    tty.restore();
}

export fn bell() void {
    putc(7) catch @panic("putc");
}

export fn writer(str: [*]const u8, len: usize) void {
    puts(str[0..len]) catch {};
    tty_writer.flush() catch {};
}

export fn write1(ch: u8) c_int {
    putc(ch) catch @panic("write1");
    tty_writer.flush() catch {};
    return 0;
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
//     //     flush_tty();
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
//         //             flush_tty();
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

// fn getch_async(io: std.Io) u8 {
//     var reader_buf: [1]u8 = undefined;
//     var tty_reader = tty_in.reader(io, &reader_buf);
//     var buf: [1]u8 = undefined;
//     if (tty_reader.interface.readSliceAll(&buf)) {
//         return buf[0];
//     } else |_| {
//         // canceled ?
//         return 0;
//     }
// }

// fn _getch_internal() u8 {
//     if (peek_queue.popFront()) |ch| {
//         return ch;
//     }
//
//     const io = runtime.io;
//     var future = io.async(getch_async, .{io});
//     return future.await(io);
// }

fn sleep_async(io: std.Io, duration: std.Io.Duration) void {
    io.sleep(duration, .awake) catch {};
}

/// return -1 if timeout
/// call from coroutine
export fn getch_timeout(ms: u32, args: ?*c.CmdArgs) c_int {
    if (w3m_task.tasks_current()) |task| {
        return task.block(.fromMilliseconds(ms), args.?);
    } else {
        return root.block(.fromMilliseconds(ms), args.?);
    }
    // _ = args;
    //
    //
    // const io = evented.io();
    //
    // const InputOrTimeout = std.Io.Select(union(enum) {
    //     input: u8,
    //     timeout: void,
    // });
    // var buf: [1]InputOrTimeout.Union = undefined;
    // var select: InputOrTimeout = .init(io, &buf);
    //
    // select.async(.input, getch_async, .{io});
    // select.async(.timeout, sleep_async, .{ io, std.Io.Duration.fromMilliseconds(ms) });
    //
    // const winner = select.await() catch @panic("select.await");
    // defer select.cancelDiscard(); // cancel remaining, discard results
    // return switch (winner) {
    //     .input => |ch| ch,
    //     .timeout => -1,
    // };
}

export fn unget(ch: c_int) void {
    if (ch > 0) {
        peek_queue.pushBack(runtime.allocator, @intCast(ch)) catch @panic("peek_queue.pushBack");
    }
}
