const std = @import("std");
const c = @import("c_include.zig").c;
const PutcStatus = @import("PutcStatus.zig");
const defuns = @import("defun.zig");
const option = @import("option.zig");

const BOOKMARK = "bookmark.html";
const RC_DIR = "~/.w3m";

const Term = @import("Term.zig");
var g_allocator: std.mem.Allocator = undefined;

fn get_locale() ?[*c]const u8 {
    const keys = [_][*:0]const u8{ "LC_ALL", "LC_CTYPE", "LANG" };
    for (keys) |key| {
        if (c.getenv(key)) |env| {
            return env;
        }
    }
    return null;
}

const Args = struct {
    args: [][]const u8,
    auto_detect: c.wc_uint8 = undefined,
    default_type: [*c]u8 = null,
    open_new_tab: bool = false,
    load_argv: std.ArrayList([*c]const u8) = .{},
    // load_argc: usize = 0,

    fn init(allocator: std.mem.Allocator) !@This() {
        var this = @This(){
            .args = try allocator.alloc([]const u8, std.os.argv.len),
        };
        errdefer allocator.free(this.args);
        for (std.os.argv, 0..) |arg, i| {
            this.args[i] = std.mem.span(arg);
        }

        if (!c.tty_init_termcap()) {
            return error.termcap;
        }

        // struct Buffer* newbuf = NULL;
        // char* p;
        // // int c;
        // int i;
        // char* line_str = NULL;
        // int load_bookmark = FALSE;
        // int visual_start = FALSE;
        // // char search_header = FALSE;
        // char* post_file = NULL;
        // Str err_msg;
        // if (!getenv("GC_LARGE_ALLOC_WARN_INTERVAL"))
        //     set_environ("GC_LARGE_ALLOC_WARN_INTERVAL", "30000");
        // GC_INIT();
        // GC_oom_fn = die_oom;
        //
        // setlocale(LC_ALL, "");

        c.getRuntime().*.fileToDelete = c.newTextList();

        // load_argv = New_N(char*, argc - 1);

        c.getRuntime().*.CurrentDir = c.currentdir();
        c.getRuntime().*.CurrentPid = c.getpid();
        c.getRuntime().*.BookmarkFile = 0;
        c.getRuntime().*.config_file = 0;

        {
            var hostname: [c.HOST_NAME_MAX + 2:0]u8 = undefined;
            if (c.gethostname(&hostname[0], hostname.len) == 0) {
                // size_t hostname_len;
                // Don't use hostname if it is truncated.
                // hostname[HOST_NAME_MAX + 1] = 0;
                // const hostname_len = std.mem.span(&hostname[0]);
                // if (hostname_len <= HOST_NAME_MAX && hostname_len < STR_SIZE_MAX)
                c.getRuntime().*.HostName = c.allocStr(&hostname[0], @intCast(std.mem.span((&hostname).ptr).len));
            }
        }

        if (this.parse_args(this.args)) {
            return this;
        } else |e| {
            switch (e) {
                error.help => {
                    help();
                },
                error.version => {
                    fversion();
                },
            }
            return e;
        }
    }

    fn deinit(this: *@This(), allocator: std.mem.Allocator) void {
        allocator.free(this.args);
    }

    fn parse_args(this: *@This(), argv: [][]const u8) !void {
        // argument search 1
        var i: usize = 1;
        while (i < argv.len) {
            if (argv[i][0] == '-') {
                if (std.mem.eql(u8, "-config", argv[i])) {
                    // "-config x" replace to "-dummy -dummy"
                    argv[i] = "-dummy";
                    i += 1;
                    if (i >= argv.len) {
                        // usage
                        return error.help;
                    }
                    c.getRuntime().*.config_file = argv[i].ptr;
                    argv[i] = "-dummy";
                } else if (std.mem.eql(u8, "-h", argv[i]) or std.mem.eql(u8, "-help", argv[i])) {
                    // help();
                    return error.help;
                } else if (std.mem.eql(u8, "-V", argv[i]) or std.mem.eql(u8, "-version", argv[i])) {
                    return error.version;
                }
            }
        }

        const Locale = get_locale();
        if (Locale) |locale| {
            c.getRuntime().*.DisplayCharset = c.wc_guess_locale_charset(locale, c.getRuntime().*.DisplayCharset);
            c.getRuntime().*.DocumentCharset = c.wc_guess_locale_charset(locale, c.getRuntime().*.DocumentCharset);
            c.getRuntime().*.SystemCharset = c.wc_guess_locale_charset(locale, c.getRuntime().*.SystemCharset);
        }

        // initializations
        option.opt_init_alloc(g_allocator);
        init_rc();
        c.opt_init();
        c.open_rc();

        if (Locale) |locale| {
            if (c.getRuntime().*.FollowLocale != 0) {
                c.getRuntime().*.DisplayCharset = c.wc_guess_locale_charset(locale, c.getRuntime().*.DisplayCharset);
                c.getRuntime().*.SystemCharset = c.wc_guess_locale_charset(locale, c.getRuntime().*.SystemCharset);
            }
        }

        this.auto_detect = c.WcOption.auto_detect;
        c.getRuntime().*.BookmarkCharset = c.getRuntime().*.DocumentCharset;

        // TODO
        // if (!non_null(getRuntime()->HTTP_proxy) && ((p = getenv("HTTP_PROXY")) || (p = getenv("http_proxy")) || (p = getenv("HTTP_proxy"))))
        //     getRuntime()->HTTP_proxy = p;
        // if (!non_null(getRuntime()->HTTPS_proxy) && ((p = getenv("HTTPS_PROXY")) || (p = getenv("https_proxy")) || (p = getenv("HTTPS_proxy"))))
        //     getRuntime()->HTTPS_proxy = p;
        // if (getRuntime()->HTTPS_proxy == NULL && non_null(getRuntime()->HTTP_proxy))
        //     getRuntime()->HTTPS_proxy = getRuntime()->HTTP_proxy;
        // if (!non_null(getRuntime()->FTP_proxy) && ((p = getenv("FTP_PROXY")) || (p = getenv("ftp_proxy")) || (p = getenv("FTP_proxy"))))
        //     getRuntime()->FTP_proxy = p;
        // if (!non_null(getRuntime()->NO_proxy) && ((p = getenv("NO_PROXY")) || (p = getenv("no_proxy")) || (p = getenv("NO_proxy"))))
        //     getRuntime()->NO_proxy = p;

        // TODO
        // if (!non_null(getRuntime()->Editor) && (p = getenv("EDITOR")) != NULL)
        //     getRuntime()->Editor = p;
        // if (!non_null(getRuntime()->Mailer) && (p = getenv("MAILER")) != NULL)
        //     getRuntime()->Mailer = p;

        // argument search 2
        i = 1;
        while (i < argv.len) : (i += 1) {
            //     if (*argv[i] == '-') {
            //         if (!strcmp("-t", argv[i])) {
            //             if (++i >= argc)
            //                 usage();
            //             if (atoi(argv[i]) > 0)
            //                 getRuntime()->Tabstop = atoi(argv[i]);
            //         } else if (!strcmp("-r", argv[i]))
            //             getRuntime()->ShowEffect = false;
            //         else if (!strcmp("-l", argv[i])) {
            //             if (++i >= argc)
            //                 usage();
            //             if (atoi(argv[i]) > 0)
            //                 getRuntime()->PagerMax = atoi(argv[i]);
            //         } else if (!strncmp("-I", argv[i], 2)) {
            //             if (argv[i][2] != '\0')
            //                 p = argv[i] + 2;
            //             else {
            //                 if (++i >= argc)
            //                     usage();
            //                 p = argv[i];
            //             }
            //             getRuntime()->DocumentCharset = wc_guess_charset_short(p, getRuntime()->DocumentCharset);
            //             WcOption.auto_detect = WC_OPT_DETECT_OFF;
            //             getRuntime()->UseContentCharset = FALSE;
            //         } else if (!strncmp("-O", argv[i], 2)) {
            //             if (argv[i][2] != '\0')
            //                 p = argv[i] + 2;
            //             else {
            //                 if (++i >= argc)
            //                     usage();
            //                 p = argv[i];
            //             }
            //             getRuntime()->DisplayCharset = wc_guess_charset_short(p, getRuntime()->DisplayCharset);
            //         } else if (!strcmp("-graph", argv[i]))
            //             UseGraphicChar = GRAPHIC_CHAR_DEC;
            //         else if (!strcmp("-no-graph", argv[i]))
            //             UseGraphicChar = GRAPHIC_CHAR_ASCII;
            //         else if (!strcmp("-T", argv[i])) {
            //             if (++i >= argc)
            //                 usage();
            //             getRuntime()->DefaultType = default_type = argv[i];
            //         }
            //         // else if (!strcmp("-m", argv[i]))
            //         //     SearchHeader = search_header = TRUE;
            //         else if (!strcmp("-v", argv[i]))
            //             visual_start = TRUE;
            //         else if (!strcmp("-N", argv[i]))
            //             open_new_tab = TRUE;
            //
            //         else if (!strcmp("-M", argv[i]))
            //             getRuntime()->useColor = FALSE;
            //         else if (!strcmp("-H", argv[i]))
            //             getRuntime()->highIntensityColors = TRUE;
            //
            //         else if (!strcmp("-B", argv[i]))
            //             load_bookmark = TRUE;
            //         else if (!strcmp("-bookmark", argv[i])) {
            //             if (++i >= argc)
            //                 usage();
            //             getRuntime()->BookmarkFile = argv[i];
            //             if (getRuntime()->BookmarkFile[0] != '~' && getRuntime()->BookmarkFile[0] != '/') {
            //                 Str tmp = Strnew_charp(getRuntime()->CurrentDir);
            //                 if (Strlastchar(tmp) != '/')
            //                     Strcat_char(tmp, '/');
            //                 Strcat_charp(tmp, getRuntime()->BookmarkFile);
            //                 getRuntime()->BookmarkFile = cleanupName(tmp->ptr);
            //             }
            //         } else if (!strcmp("-F", argv[i]))
            //             getRuntime()->RenderFrame = TRUE;
            //         else if (!strcmp("-W", argv[i])) {
            //             if (getRuntime()->WrapDefault)
            //                 getRuntime()->WrapDefault = FALSE;
            //             else
            //                 getRuntime()->WrapDefault = TRUE;
            //         } else if (!strcmp("-cols", argv[i])) {
            //             if (++i >= argc)
            //                 usage();
            //             tty_set_cols(atoi(argv[i]));
            //         } else if (!strcmp("-ppc", argv[i])) {
            //             double ppc;
            //             if (++i >= argc)
            //                 usage();
            //             ppc = atof(argv[i]);
            //             if (ppc >= MINIMUM_PIXEL_PER_CHAR && ppc <= MAXIMUM_PIXEL_PER_CHAR) {
            //                 getRuntime()->pixel_per_char = ppc;
            //                 getRuntime()->set_pixel_per_char = true;
            //             }
            //         }
            //
            //         else if (!strcmp("-ppl", argv[i])) {
            //             double ppc;
            //             if (++i >= argc)
            //                 usage();
            //             ppc = atof(argv[i]);
            //             if (ppc >= MINIMUM_PIXEL_PER_CHAR && ppc <= MAXIMUM_PIXEL_PER_CHAR * 2) {
            //                 getRuntime()->pixel_per_line = ppc;
            //                 getRuntime()->set_pixel_per_line = TRUE;
            //             }
            //         }
            //
            //         else if (!strcmp("-ri", argv[i])) {
            //             getRuntime()->enable_inline_image = INLINE_IMG_OSC5379;
            //         } else if (!strcmp("-sixel", argv[i])) {
            //             getRuntime()->enable_inline_image = INLINE_IMG_SIXEL;
            //         } else if (!strcmp("-num", argv[i]))
            //             getRuntime()->showLineNum = TRUE;
            //         else if (!strcmp("-no-proxy", argv[i]))
            //             getRuntime()->use_proxy = FALSE;
            //         else if (!strcmp("-4", argv[i]) || !strcmp("-6", argv[i]))
            //             set_param_option(Sprintf("dns_order=%c", argv[i][1])->ptr);
            //         else if (!strcmp("-post", argv[i])) {
            //             if (++i >= argc)
            //                 usage();
            //             post_file = argv[i];
            //         } else if (!strcmp("-header", argv[i])) {
            //             Str hs;
            //             if (++i >= argc)
            //                 usage();
            //             if ((hs = make_optional_header_string(argv[i])) != NULL) {
            //                 if (getRuntime()->header_string == NULL)
            //                     getRuntime()->header_string = hs;
            //                 else
            //                     Strcat(getRuntime()->header_string, hs);
            //             }
            //             while (argv[i][0]) {
            //                 argv[i][0] = '\0';
            //                 argv[i]++;
            //             }
            //         } else if (!strcmp("-no-cookie", argv[i])) {
            //             getRuntime()->use_cookie = FALSE;
            //             getRuntime()->accept_cookie = FALSE;
            //         } else if (!strcmp("-cookie", argv[i])) {
            //             getRuntime()->use_cookie = TRUE;
            //             getRuntime()->accept_cookie = TRUE;
            //         } else if (!strcmp("-s", argv[i]))
            //             getRuntime()->squeezeBlankLine = TRUE;
            //         else if (!strcmp("-X", argv[i]))
            //             getRuntime()->Do_not_use_ti_te = TRUE;
            //         else if (!strcmp("-title", argv[i]))
            //             getRuntime()->displayTitleTerm = getenv("TERM");
            //         else if (!strncmp("-title=", argv[i], 7))
            //             getRuntime()->displayTitleTerm = argv[i] + 7;
            //         else if (!strcmp("-insecure", argv[i])) {
            //             set_param_option("ssl_cipher=ALL:eNULL");
            //             set_param_option("ssl_forbid_method=");
            //             set_param_option("ssl_verify_server=0");
            //         } else if (!strcmp("-o", argv[i]) || !strcmp("-show-option", argv[i])) {
            //             if (!strcmp("-show-option", argv[i]) || ++i >= argc || !strcmp(argv[i], "?")) {
            //                 show_params(stdout);
            //                 exit(0);
            //             }
            //             if (!set_param_option(argv[i])) {
            //                 /* option set failed */
            //                 /* FIXME: gettextize? */
            //                 fprintf(stderr, "%s: bad option\n", argv[i]);
            //                 show_params_p = 1;
            //                 usage();
            //             }
            //         } else if (!strcmp("-", argv[i]) || !strcmp("-dummy", argv[i])) {
            //             /* do nothing */
            //         }
            //         // else if (!strcmp("-reqlog", argv[i])) {
            //         //     w3m_reqlog = rcFile("request.log");
            //         // }
            //         // #if defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE)
            //         //             else if (!strcmp("-$$getimage", argv[i])) {
            //         //                 ++i;
            //         //                 getimage_args = argv + i;
            //         //                 i += 4;
            //         //                 if (i > argc)
            //         //                     usage();
            //         //             }
            //         // #endif /* defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE) */
            //         else {
            //             usage();
            //         }
            //     } else if (*argv[i] == '+') {
            //         line_str = argv[i] + 1;
            //     } else {
            //         load_argv[load_argc++] = argv[i];
            // }
        }

        if (c.getRuntime().*.BookmarkFile == null)
            c.getRuntime().*.BookmarkFile = c.rcFile(BOOKMARK);
    }

    fn fversion() void {
        // fprintf(f, "w3m version %s, options %s\n", w3m_version,
        //     "lang=en"
        //     ",m17n"
        //     ",image"
        //     ",color"
        //     ",ansi-color"
        //     ",menu"
        //     ",cookie"
        //     ",ssl"
        //     ",ssl-verify"
        //     ",external-uri-loader"
        //     ",ipv6"
        //     ",alarm"
        //     ",mark");
    }

    fn help() void {
        // fversion(f);
        // /* FIXME: gettextize? */
        // fprintf(f, "usage: w3m [options] [URL or content.filename]\noptions:\n");
        // fprintf(f, "    -t tab           set tab width\n");
        // fprintf(f, "    -r               ignore backspace effect\n");
        // fprintf(f, "    -l line          # of preserved line (default 10000)\n");
        // fprintf(f, "    -I charset       document charset\n");
        // fprintf(f, "    -O charset       display/output charset\n");
        // fprintf(f, "    -B               load bookmark\n");
        // fprintf(f, "    -bookmark file   specify bookmark file\n");
        // fprintf(f, "    -T type          specify content-type\n");
        // fprintf(f, "    -m               internet message mode\n");
        // fprintf(f, "    -v               visual startup mode\n");
        // fprintf(f, "    -M               monochrome display\n");
        // fprintf(f, "    -H               use high-intensity colors\n");
        // fprintf(f,
        //     "    -N               open URL of command line on each new tab\n");
        // fprintf(f, "    -F               automatically render frames\n");
        // fprintf(f,
        //     "    -cols width      specify column width (used with -dump)\n");
        // fprintf(f,
        //     "    -ppc count       specify the number of pixels per character (4.0...32.0)\n");
        // fprintf(f,
        //     "    -ppl count       specify the number of pixels per line (4.0...64.0)\n");
        // fprintf(f, "    -dump            dump formatted page into stdout\n");
        // fprintf(f,
        //     "    -dump_head       dump response of HEAD request into stdout\n");
        // fprintf(f, "    -dump_source     dump page source into stdout\n");
        // fprintf(f, "    -dump_both       dump HEAD and source into stdout\n");
        // fprintf(f,
        //     "    -dump_extra      dump HEAD, source, and extra information into stdout\n");
        // fprintf(f, "    -post file       use POST method with file content\n");
        // fprintf(f, "    -header string   insert string as a header\n");
        // fprintf(f, "    +<num>           goto <num> line\n");
        // fprintf(f, "    -num             show line number\n");
        // fprintf(f, "    -no-proxy        don't use proxy\n");
        // fprintf(f, "    -4               IPv4 only (-o dns_order=4)\n");
        // fprintf(f, "    -6               IPv6 only (-o dns_order=6)\n");
        // fprintf(f, "    -insecure        use insecure SSL config options\n");
        // fprintf(f, "    -cookie          use cookie (-no-cookie: don't use cookie)\n");
        // fprintf(f, "    -graph           use DEC special graphics for border of table and menu\n");
        // fprintf(f, "    -no-graph        use ASCII character for border of table and menu\n");
        // fprintf(f, "    -s               squeeze multiple blank lines\n");
        // fprintf(f, "    -W               toggle search wrap mode\n");
        // fprintf(f, "    -X               don't use termcap init/deinit\n");
        // fprintf(f,
        //     "    -title[=TERM]    set buffer name to terminal title string\n");
        // fprintf(f, "    -o opt=value     assign value to config option\n");
        // fprintf(f, "    -show-option     print all config options\n");
        // fprintf(f, "    -config file     specify config file\n");
        // fprintf(f, "    -debug           use debug mode (only for debugging)\n");
        // fprintf(f, "    -reqlog          write request logfile\n");
        // fprintf(f, "    -help            print this usage message\n");
        // fprintf(f, "    -version         print w3m version\n");
        // if (show_params_p)
        //     show_params(f);
        // exit(err);
    }
};

export fn init_rc() void {
    c.getRuntime().*.LoadHist = c.newHist();
    c.getRuntime().*.SaveHist = c.newHist();
    c.getRuntime().*.ShellHist = c.newHist();
    c.getRuntime().*.TextHist = c.newHist();
    c.getRuntime().*.URLHist = c.newHist();
    if (c.getRuntime().*.UseHistory != 0) {
        _ = c.loadHistory(c.getRuntime().*.URLHist);
    }

    if (c.getRuntime().*.rc_dir != null) {
        return;
    }

    c.getRuntime().*.rc_dir = c.allocStr(c.getenv("W3M_DIR"), -1);
    if (c.getRuntime().*.rc_dir == null or c.getRuntime().*.rc_dir[0] == 0)
        c.getRuntime().*.rc_dir = c.allocStr(RC_DIR, -1);
    if (c.getRuntime().*.rc_dir == null or c.getRuntime().*.rc_dir[0] == 0) {
        c.getRuntime().*.no_rc_dir = 1;
        return;
    }
    c.getRuntime().*.rc_dir = c.expandPath(c.getRuntime().*.rc_dir);

    const i = std.mem.span(c.getRuntime().*.rc_dir).len;
    if (i > 1 and c.getRuntime().*.rc_dir[i - 1] == '/')
        c.getRuntime().*.rc_dir[i - 1] = 0;

    c.getRuntime().*.tmp_dir = c.getRuntime().*.rc_dir;

    if (c.do_recursive_mkdir(c.getRuntime().*.rc_dir) == -1) {
        c.getRuntime().*.no_rc_dir = 1;
        return;
    }

    c.getRuntime().*.no_rc_dir = 0;

    if (c.getRuntime().*.config_file == null)
        c.getRuntime().*.config_file = c.rcFile(c.CONFIG_FILE);
}

const GC_WARN_KEEP_MAX = (20);
var orig_GC_warn_proc: c.GC_warn_proc = null;

export fn wrap_GC_warn_proc(msg: [*c]u8, arg: c.GC_word) void {
    if (c.fmInitialized()) {
        // static struct {
        //     char* msg;
        //     GC_word arg;
        // } msg_ring[GC_WARN_KEEP_MAX];
        // static int i = 0;
        // static int n = 0;
        // static int lock = 0;
        // int j;
        //
        // j = (i + n) % (sizeof(msg_ring) / sizeof(msg_ring[0]));
        // msg_ring[j].msg = msg;
        // msg_ring[j].arg = arg;
        //
        // if (n < sizeof(msg_ring) / sizeof(msg_ring[0]))
        //     ++n;
        // else
        //     ++i;
        //
        // if (!lock) {
        //     lock = 1;
        //
        //     for (; n > 0; --n, ++i) {
        //         i %= sizeof(msg_ring) / sizeof(msg_ring[0]);
        //
        //         printf(msg_ring[i].msg, (unsigned long)msg_ring[i].arg);
        //         // sleep_till_anykey(1, 1);
        //     }
        //
        //     lock = 0;
        // }
    } else if (orig_GC_warn_proc) |proc| {
        proc(msg, arg);
    } else {
        _ = c.fprintf(c.stderr, msg, arg);
    }
}

fn get_home_env() ?[*]u8 {
    if (c.getenv("HTTP_HOME")) |env| {
        return env;
    }
    if (c.getenv("WWW_HOME")) |env| {
        return env;
    }
    return null;
}

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.detectLeaks();
    g_allocator = gpa.allocator();

    Term.g_term = Term.init(g_allocator, std.fs.File.stdin()) catch
        @panic("Term.init");
    defer Term.g_term.deinit();

    const ws = try Term.g_term.getWinsize();
    c.screen_setup(ws.row, ws.col);

    var args = try Args.init(g_allocator);
    defer args.deinit();

    // init
    enterRawMode();
    c.sync_with_option();
    c.initCookie();
    // mySignal(SIGPIPE, SigPipe);
    orig_GC_warn_proc = c.GC_get_warn_proc();
    c.GC_set_warn_proc(wrap_GC_warn_proc);

    const err_msg = c.Strnew();
    if (args.load_argv.items.len == 0) {
        // no URL specified
        if (c.isatty(0) == 0) {
            //         // not impl
            //         assert(false);
            //         // redin = newFileStream(fdopen(dup(0), "rb"), (void (*)())pclose);
            //         // newbuf = openGeneralPagerBuffer(redin);
            //         // dup2(1, 0);
        }
        // else if (load_bookmark) {
        //         struct Content* content = get_content_cache(getRuntime()->BookmarkFile, NULL,
        //             (struct LoadOption) { .base_url = NULL, .referer = NO_REFERER, .flag = 0 });
        //         if (!content)
        //             Strcat_charp(err_msg, "w3m: Can't load bookmark.\n");
        //         newbuf = buf_new(content);
        // }
        // else if (visual_start) {
        //         Str s_page;
        //         s_page = Strnew_charp("<title>W3M startup page</title><center><b>Welcome to ");
        //         Strcat_charp(s_page, "<a href='http://w3m.sourceforge.net/'>");
        //         Strcat_m_charp(s_page,
        //             "w3m</a>!<p><p>This is w3m version ",
        //             w3m_version,
        //             "<br>Written by <a href='mailto:aito@fw.ipsj.or.jp'>Akinori Ito</a>",
        //             NULL);
        //         newbuf = loadHTMLString(s_page);
        //         if (newbuf == NULL)
        //             Strcat_charp(err_msg, "w3m: Can't load string.\n");
        //         else
        //             newbuf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
        // }
        else if (get_home_env()) |p| {
            if (c.get_content_cache(p, null, .{})) |content| {
                _ = c.pushHashHist(c.getRuntime().*.URLHist, c.parsedURL2Str(&content.*.url).*.ptr);
                const newbuf = c.buf_new(content);
                _ = c.tabs_append(newbuf);
            } else {
                c.Strcat(err_msg, c.Sprintf("w3m: Can't load %s.\n", p));
            }
        } else {
            //         if (fmInitialized())
            //             exitRawMode();
            //         usage();
        }
        //     if (newbuf == NULL) {
        //         if (fmInitialized())
        //             exitRawMode();
        //         if (err_msg->length)
        //             fprintf(stderr, "%s", err_msg->ptr);
        //         w3m_exit(2);
        //     }
        //     i = -1;
    }

    for (args.load_argv.items) |argv| {
        // SearchHeader = search_header;
        c.getRuntime().*.DefaultType = args.default_type;
        var url = argv;

        if (c.getURLScheme(&url) == c.SCM_MISSING and 0 == c.getRuntime().*.ArgvIsURL) {
            //         retry_as_local_file:
            url = c.file_to_url(argv);
        } else {
            url = c.url_encode(c.conv_from_system(argv), null, 0);
        }

        var request: ?*c.FormList = null;
        // if (post_file && i == 0) {
        //                 FILE* fp;
        //                 Str body;
        //                 if (!strcmp(post_file, "-"))
        //                     fp = stdin;
        //                 else
        //                     fp = fopen(post_file, "r");
        //                 if (fp == NULL) {
        //                     Strcat(err_msg,
        //                         Sprintf("w3m: Can't open %s.\n", post_file));
        //                     continue;
        //                 }
        //                 body = Strfgetall(fp);
        //                 if (fp != stdin)
        //                     fclose(fp);
        //                 request = newFormList(NULL, "post", NULL, NULL, NULL, NULL,
        //                     NULL);
        //                 request->body = body->ptr;
        //                 request->boundary = NULL;
        //                 request->length = body->length;
        //             } else
        {
            request = null;
        }
        const content = c.get_content_cache(url, request, .{});
        const newbuf = c.buf_new(content);

        //             if (getRuntime()->ArgvIsURL && !retry) {
        //                 retry = 1;
        //                 goto retry_as_local_file;
        //             }
        //             /* FIXME: gettextize? */
        //             Strcat(err_msg,
        //                 Sprintf("w3m: Can't load %s.\n", load_argv[i]));
        //             continue;

        if (c.nTab() == 0) {
            _ = c.tabs_append(newbuf);
            //         // getRuntime()->FirstTab = getRuntime()->LastTab = getRuntime()->CurrentTab = tab_new();
            //         // if (!FirstTab()) {
            //         //     fprintf(stderr, "%s\n", "Can't allocated memory");
            //         //     exit(1);
            //         // }
            //         // getRuntime()->nTab = 1;
            //         // Firstbuf = Currentbuf = newbuf;
        } else if (args.open_new_tab) {
            //         tabs_append(newbuf);
        } else {
            //         tab_push_buffer(CurrentTab(), newbuf);
        }
    }

    // if (!FirstTab() || !Firstbuf) {
    //     if (fmInitialized())
    //         exitRawMode();
    //     if (err_msg->length)
    //         fprintf(stderr, "%s", err_msg->ptr);
    //     w3m_exit(2);
    // }

    // if (err_msg->length)
    //     disp_message_nsec(err_msg->ptr, FALSE, 1, TRUE, FALSE);

    c.getRuntime().*.DefaultType = null;
    c.getRuntime().*.UseContentCharset = 1;
    c.WcOption.auto_detect = args.auto_detect;

    //
    // main loop
    //
    while (true) {
        if (c.currentBufferSubmit()) {
            continue;
        }

        if (c.eventUpdate()) {
            continue;
        }

        if (Term.g_term.getch()) |ch| {
            c.w3m_on_key(ch);
            c.w3m_end_frame();
        }
        onFrame();
    }
}

//
// input(rawmode)
//

export fn fmInitialized() bool {
    return Term.g_term.is_rawmode;
}

export fn enterRawMode() void {
    if (!Term.g_term.is_rawmode) {
        // term_raw();
        // term_noecho();
        Term.g_term.enterRawMode() catch @panic("enterRawMode");
        c.initscr();
        c.initImage();
    }
}

export fn exitRawMode() void {
    if (Term.g_term.is_rawmode) {
        c.screen_move(.{ .y = c.LASTLINE(), .x = 0 });
        c.screen_clrtoeolx();
        c.tty_write_screen();
        c.loadImage(c.IMG_FLAG_STOP);
        reset_tty();
    }
}

export fn reset_tty() void {
    const g_runtime: *c.Runtime = c.getRuntime() orelse {
        unreachable;
    };
    writestr(g_runtime.termcap._op); // turn off
    writestr(g_runtime.termcap._me);
    if (g_runtime.Do_not_use_ti_te) {
        if (g_runtime.termcap._te != null and g_runtime.termcap._te[0] != 0) {
            writestr(g_runtime.termcap._te);
        } else {
            writestr(g_runtime.termcap._cl);
        }
    }
    writestr(g_runtime.termcap._se); // reset terminal
    flush_tty();
    // tcsetattr(g_runtime.tty_input, TCSANOW, &d_ioval);
    Term.g_term.exitRawMode();
}

export fn tty_add_ISIG() void {
    // ttymode_set(ISIG, 0);
}

export fn tty_remove_ISIG() void {
    // ttymode_reset(ISIG, 0);
}

var cline: ?*c.Line = null;
var ccolumn: c_int = -1;

export fn onFrame() void {
    // if (mode == B_FORCE_REDRAW && (buf->check_url & CHK_URL)) {
    //     chkURLBuffer(buf);
    // }

    const buf: *c.Buffer = c.CurrentTab().*.currentBuffer;

    if (buf.doc == null) {
        c.reshapeBuffer(buf);
    }

    c.bufferPosition(buf);

    // check viewport ?
    if (buf.*.doc.*.lineUpdated //
    or cline != buf.*.doc.*.topLine or ccolumn != buf.doc.*.currentColumn) {
        cline = buf.doc.*.topLine;
        ccolumn = buf.doc.*.currentColumn;

        // render
        c.screen_from_lines(buf.doc, c.baseURL(buf));
        tty_write_screen();

        c.loadImage(c.IMG_FLAG_NEXT);
    }
    buf.*.doc.*.lineUpdated = false;

    c.displayMsg(buf);

    const pos = screen_position();
    c.drawAnchorCursor(buf.*.doc, c.baseURL(buf));

    if (buf.doc.*.img.nanchor > 0) {
        // && buf->image_loaded
        c.drawImage(buf);
    }

    c.tty_MOVE(@intCast(pos.y), @intCast(pos.x));
    flush_tty();

    // idle timer event
    //     if (Currentbuf->event) {
    //         if (Currentbuf->event->status != AL_UNSET) {
    //             CurrentAlarm = Currentbuf->event;
    //             if (CurrentAlarm->sec == 0) { /* refresh (0sec) */
    //                 Currentbuf->event = NULL;
    //                 CurrentKey = -1;
    //                 CurrentKeyData = NULL;
    //                 CurrentCmdData = (char*)CurrentAlarm->data;
    //                 w3mFuncList[CurrentAlarm->cmd].func();
    //                 CurrentCmdData = NULL;
    //                 continue;
    //             }
    //         } else
    //             Currentbuf->event = NULL;
    //     }
    //     if (!Currentbuf->event)
    //         CurrentAlarm = &DefaultAlarm;
    //
    //     if (CurrentAlarm->sec > 0) {
    //         mySignal(SIGALRM, SigAlarm);
    //         alarm(CurrentAlarm->sec);
    //     }
}

// void ttymode_set(int mode, int imode)
// {
//     struct termios ioval;
//     tcgetattr(g_runtime.tty_input, &ioval);
//     ioval.c_lflag |= mode;
//     ioval.c_iflag |= imode;
//     while (tcsetattr(g_runtime.tty_input, TCSANOW, &ioval) == -1) {
//         if (errno == EINTR || errno == EAGAIN)
//             continue;
//         printf("Error occurred while set %x: errno=%d\n", mode, errno);
//         reset_error_exit(SIGNAL_ARGLIST);
//     }
// }
//
// void ttymode_reset(int mode, int imode)
// {
//     struct termios ioval;
//     tcgetattr(g_runtime.tty_input, &ioval);
//     ioval.c_lflag &= ~mode;
//     ioval.c_iflag &= ~imode;
//     while (tcsetattr(g_runtime.tty_input, TCSANOW, &ioval) == -1) {
//         if (errno == EINTR || errno == EAGAIN)
//             continue;
//         printf("Error occurred while reset %x: errno=%d\n", mode, errno);
//         reset_error_exit(SIGNAL_ARGLIST);
//     }
// }
//
// void set_cc(int spec, int val)
// {
//     struct termios ioval;
//     tcgetattr(g_runtime.tty_input, &ioval);
//     ioval.c_cc[spec] = val;
//     while (tcsetattr(g_runtime.tty_input, TCSANOW, &ioval) == -1) {
//         if (errno == EINTR || errno == EAGAIN)
//             continue;
//         printf("Error occurred: errno=%d\n", errno);
//         reset_error_exit(SIGNAL_ARGLIST);
//     }
// }

// void crmode(void)
// {
//     ttymode_reset(ICANON, IXON);
//     ttymode_set(ISIG, 0);
//     set_cc(VMIN, 1);
// }
//
// void nocrmode(void)
// {
//     ttymode_set(ICANON, 0);
//     set_cc(VMIN, 4);
// }
//
// void term_echo(void)
// {
//     ttymode_set(ECHO, 0);
// }
//
// void term_noecho(void)
// {
//     ttymode_reset(ECHO, 0);
// }

// #define TTY_MODE ISIG | ICANON | ECHO | IEXTEN
// export fn term_raw() void {
//     ttymode_reset(TTY_MODE, IXON | IXOFF | INLCR | IGNCR | ICRNL);
//     set_cc(VMIN, 1);
// }

export fn tty_cbreak(enable: bool) void {
    if (enable) {
        Term.g_term.cbreakMode() catch @panic("tty_cbreak");
    } else {
        Term.g_term.enterRawMode() catch @panic("tty_cbreak");
    }
}

export fn getch() c_int {
    if (Term.g_term.getch()) |ch| {
        return ch;
    } else {
        return 0;
    }
}

//
// output
//
export fn getOutputHandle() c_int {
    return std.fs.File.stdout().handle;
}

export fn flush_tty() void {
    // if (tty_output_f){
    //     fflush(tty_output_f);
    // }
}

export fn write1(ch: c_int) c_int {
    // putc(c, tty_output_f);
    // return write(g_runtime.tty_output, &c, 1);
    const p: [*]const u8 = @ptrCast(&ch);
    const result = std.fs.File.stdout().write(p[0..1]) catch @panic("write1");
    return @intCast(result);
}

// size_t writeN(const uint8_t *p, size_t n)
// {
//     // return fwrite(p, 1, n, tty_output_f);
//     return write(tty_output, p, n);
// }

export fn writestr(s: [*c]const u8) void {
    _ = c.tputs(s, 1, c.write1);
}

//
// size
//

export fn setlinescols() void {
    // struct winsize wins;
    // int i = ioctl(g_runtime.tty_input, TIOCGWINSZ, &wins);
    // if (i >= 0 and wins.ws_row != 0 and wins.ws_col != 0) {
    //     g_runtime.lines = wins.ws_row;
    //     g_runtime.cols = wins.ws_col;
    // }

    if (Term.g_term.getWinsize()) |ws| {
        const rt = c.getRuntime();
        rt.*.lines = ws.row;
        rt.*.cols = ws.col;
    } else |e| {
        @panic(@errorName(e));
    }
}

//
// image
//

export fn get_pixel_per_cell(ppc: *c_int, ppl: *c_int) bool {
    _ = ppc;
    _ = ppl;
    //     fd_set rfd;
    //     struct timeval tval;
    //     char buf[100];
    //     char* p;
    //     ssize_t len;
    //     ssize_t left;
    //     int wp, hp, wc, hc;
    //     int i;
    //
    //     struct winsize ws;
    //     if (ioctl(g_runtime.tty_input, TIOCGWINSZ, &ws) == 0 and ws.ws_ypixel > 0 and ws.ws_row > 0 and ws.ws_xpixel > 0 and ws.ws_col > 0) {
    //         *ppc = ws.ws_xpixel / ws.ws_col;
    //         *ppl = ws.ws_ypixel / ws.ws_row;
    //         return 1;
    //     }
    //
    //     const char* str = "\x1b[14t\x1b[18t";
    //     // fputs(, tty_output_f);
    //     write(g_runtime.tty_output, str, strlen(str));
    //     flush_tty();
    //
    //     p = buf;
    //     left = sizeof(buf) - 1;
    //     for (i = 0; i < 10; i++) {
    //         tval.tv_usec = 200000; /* 0.2 sec * 10 */
    //         tval.tv_sec = 0;
    //         FD_ZERO(&rfd);
    //         FD_SET(g_runtime.tty_input, &rfd);
    //         if (select(g_runtime.tty_input + 1, &rfd, NULL, NULL, &tval) <= 0 || !FD_ISSET(g_runtime.tty_input, &rfd))
    //             continue;
    //
    //         if ((len = read(g_runtime.tty_input, p, left)) <= 0)
    //             continue;
    //         p[len] = '\0';
    //
    //         if (sscanf(buf, "\x1b[4;%d;%dt\x1b[8;%d;%dt", &hp, &wp, &hc, &wc) == 4) {
    //             if (wp > 0 and wc > 0 and hp > 0 and hc > 0) {
    //                 *ppc = wp / wc;
    //                 *ppl = hp / hc;
    //                 return 1;
    //             } else {
    //                 return 0;
    //             }
    //         }
    //         p += len;
    //         left -= len;
    //     }
    //
    return false;
}

//
// screen
//
const SCREEN_SPACE = " ";

const CELL_CHAR_LEN = 14;

const ScreenCell = struct {
    str: [CELL_CHAR_LEN]u8,
    prop: c.ScreenCellProperty,

    fn set(
        this: *@This(),
        ch: [*c]const u8,
        len: usize,
        prop: c.ScreenCellProperty,
    ) void {
        std.mem.copyForwards(u8, this.str[0 .. len + 1], ch[0 .. len + 1]);
        this.prop = (this.prop & c.S_DIRTY) | prop;
    }
};

const ScreenLine = struct {
    cells: [*]ScreenCell,
    isdirty: c.ScreenLineFlags,
    eol: usize,
};

const Screen = struct {
    line_count: usize = 0,
    line_capacity: usize = 0,
    col_count: usize = 0,
    col_capacity: usize = 0,
    lines: [*]ScreenLine = undefined,
    y: usize = 0,
    x: usize = 0,
    tab_step: usize = 8,
    mode: c.ScreenCellProperty = 0,

    fn deinit(this: *@This(), allocator: std.mem.Allocator) void {
        if (this.line_capacity > 0) {
            for (this.lines[0..this.line_capacity]) |l| {
                allocator.free(l.cells[0..this.col_capacity]);
            }
            allocator.free(this.lines[0..this.line_capacity]);
        }
        this.line_capacity = 0;
        this.col_capacity = 0;
    }
};

var g_screen = Screen{};

export fn screen_position() c.Vec2 {
    return .{
        .x = g_screen.x,
        .y = g_screen.y,
    };
}

fn screen_need_redraw(
    c1: [*c]const u8,
    pr1: c.ScreenCellProperty,
    c2: [*c]const u8,
    pr2: c.ScreenCellProperty,
) bool {
    if (c1 != null and c2 != null and std.mem.eql(u8, std.mem.span(c1), std.mem.span(c2))) {
        if (c1[0] == ' ') {
            return ((pr1 ^ pr2) & M_SPACE & ~c.S_DIRTY) != 0;
        } else {
            return ((pr1 ^ pr2) & ~c.S_DIRTY) != 0;
        }
    } else {
        return true;
    }
}

export fn screen_setup(line_count: usize, col_count: usize) void {
    g_screen.deinit(g_allocator);

    g_screen.line_capacity = line_count + 1;
    g_screen.lines = (g_allocator.alloc(ScreenLine, g_screen.line_capacity) catch @panic("OOM")).ptr;
    g_screen.line_count = line_count;

    g_screen.col_capacity = col_count + 1;
    for (g_screen.lines[0..g_screen.line_capacity]) |*l| {
        l.cells = (g_allocator.alloc(ScreenCell, g_screen.col_capacity) catch @panic("OOM")).ptr;
    }
    g_screen.col_count = col_count;

    screen_clear();
}

fn screen_addmchz(pc: [*c]const u8, len: usize, width: usize) void {
    if (pc == null or len == 0) {
        return;
    }
    if (g_screen.x == g_screen.col_count)
        screen_wrap();
    if (g_screen.x >= g_screen.col_count)
        return;

    const ch = pc[0];

    var line = g_screen.lines[g_screen.y].cells[0..g_screen.col_count];
    if (line[g_screen.x].prop & c.S_EOL != 0) {
        if (ch == ' ' and 0 == (g_screen.mode & M_SPACE)) {
            // advnce cursor
            g_screen.x += 1;
            return;
        }
        // drop tail space
        var _i: i32 = @intCast(g_screen.x);
        while (_i >= 0) : (_i -= 1) {
            const i: usize = @intCast(_i);
            if (0 == (line[i].prop & c.S_EOL)) {
                break;
            }
            line[i].set(
                SCREEN_SPACE,
                1,
                (line[i].prop & M_CEOL) | c.C_ASCII,
            );
        }
    }

    if (ch == '\t' or ch == '\n' or ch == '\r' or ch == 0x8 //'\b'
    ) {
        SET_CHAR_MODE(&g_screen.mode, c.C_CTRL);
    } else if (len > 1) {
        SET_CHAR_MODE(&g_screen.mode, c.C_WCHAR1);
    } else if (!c.IS_CNTRL(ch)) {
        SET_CHAR_MODE(&g_screen.mode, c.C_ASCII);
    } else {
        return;
    }

    // Required to erase bold or underlined character for some terminal emulators.
    {
        var i = g_screen.x + width - 1;
        if (i < g_screen.col_count //
        and (((line[i].prop & c.S_BOLD) != 0 and screen_need_redraw(
            (&line[i].str).ptr,
            line[i].prop,
            pc,
            g_screen.mode,
        )) //
            or ((line[i].prop & c.S_UNDERLINE) != 0 and 0 == (g_screen.mode & c.S_UNDERLINE))))
        {
            screen_touch_line();
            i += 1;
            if (i < g_screen.col_count) {
                screen_touch_column(i);
                if (line[i].prop & c.S_EOL != 0) {
                    line[i].set(SCREEN_SPACE, 1, (line[i].prop & M_CEOL) | c.C_ASCII);
                } else {
                    i += 1;
                    while (i < g_screen.col_count //
                    and CHAR_MODE(line[i].prop) == c.C_WCHAR2) : (i += 1) {
                        screen_touch_column(i);
                    }
                }
            }
        }
    }

    if (g_screen.x + width > g_screen.col_count) {
        // 全角 身切れ
        screen_touch_line();
        for (g_screen.x..g_screen.col_count) |i| {
            line[i].set(SCREEN_SPACE, 1, (line[i].prop & ~c.C_WHICHCHAR) | c.C_ASCII);
            screen_touch_column(i);
        }

        // new line and get first cell
        screen_wrap();
        if (g_screen.x + width > g_screen.col_count)
            return;
        line = g_screen.lines[g_screen.y].cells[0..g_screen.col_count];
    }

    if (CHAR_MODE(line[g_screen.x].prop) == c.C_WCHAR2) {
        // 全角文字の先頭以外。前の文字をクリア
        screen_touch_line();
        var _i: i32 = @intCast(g_screen.x - 1);
        while (_i >= 0) : (_i -= 1) {
            const i: usize = @intCast(_i);
            const l = CHAR_MODE(line[i].prop);
            line[i].set(SCREEN_SPACE, 1, (line[i].prop & ~c.C_WHICHCHAR) | c.C_ASCII);
            screen_touch_column(i);
            if (l != c.C_WCHAR2)
                break;
        }
    }

    if (CHAR_MODE(g_screen.mode) != c.C_CTRL) {
        if (screen_need_redraw(
            (&line[g_screen.x].str).ptr,
            line[g_screen.x].prop,
            pc,
            g_screen.mode,
        )) {
            line[g_screen.x].set(pc, len, g_screen.mode);
            screen_touch_line();
            screen_touch_column(g_screen.x);
            SET_CHAR_MODE(&g_screen.mode, c.C_WCHAR2);
            var i = g_screen.x + 1;
            while (i < g_screen.x + width) : (i += 1) {
                // 全角文字の後続cell
                line[i].set(SCREEN_SPACE, 1, (line[g_screen.x].prop & ~c.C_WHICHCHAR) | c.C_WCHAR2);
                screen_touch_column(i);
            }
            while (i < g_screen.col_count and CHAR_MODE(line[i].prop) == c.C_WCHAR2) : (i += 1) {
                // 下にあった全角文字の後続を消す
                line[i].set(SCREEN_SPACE, 1, (line[i].prop & ~c.C_WHICHCHAR) | c.C_ASCII);
                screen_touch_column(i);
            }
        }
        g_screen.x += width;
    } else if (ch == '\t') {
        var dest = (g_screen.x + g_screen.tab_step) / g_screen.tab_step * g_screen.tab_step;
        if (dest >= g_screen.col_count) {
            screen_wrap();
            screen_touch_line();
            dest = g_screen.tab_step;
            line = g_screen.lines[g_screen.y].cells[0..g_screen.col_count];
        }
        var i = g_screen.x;
        while (i < dest) : (i += 1) {
            if (screen_need_redraw(
                (&line[i].str).ptr,
                line[i].prop,
                SCREEN_SPACE,
                g_screen.mode,
            )) {
                line[i].set(SCREEN_SPACE, 1, g_screen.mode);
                screen_touch_line();
                screen_touch_column(i);
            }
        }
        g_screen.x = i;
    } else if (ch == '\n') {
        screen_wrap();
    } else if (ch == '\r') { // Carriage return
        g_screen.x = 0;
    } else if (ch == 0x8 //'\b'
    and g_screen.x > 0) { // Backspace
        g_screen.x -= 1;
        while (g_screen.x > 0 and CHAR_MODE(line[g_screen.x].prop) == c.C_WCHAR2)
            g_screen.x -= 1;
    }
}

export fn screen_addmch(pc: [*c]const u8, len: usize, width: usize) void {
    // copy for zero terminate
    var buf: [CELL_CHAR_LEN]u8 = undefined;
    std.debug.assert(len < @sizeOf(@TypeOf(buf)));
    std.mem.copyForwards(u8, &buf, pc[0..len]);
    buf[len] = 0;
    screen_addmchz((&buf).ptr, len, width);
}

export fn screen_move(pos: c.Vec2) void {
    if (pos.y < g_screen.line_count)
        g_screen.y = pos.y;
    if (pos.x < g_screen.col_count)
        g_screen.x = pos.x;
}

fn CHAR_MODE(prop: c.ScreenCellProperty) c.ScreenCellProperty {
    return (prop & c.C_WHICHCHAR);
}

fn SET_CHAR_MODE(prop: *c.ScreenCellProperty, mode: c.ScreenCellProperty) void {
    prop.* = (prop.* & ~c.C_WHICHCHAR) | mode;
}

export fn screen_add_tab() void {
    c.screen_addmch("\t", 1, g_screen.tab_step);
}

fn screen_wrap() void {
    if (g_screen.y == g_screen.line_count - 1)
        return;
    g_screen.y += 1;
    g_screen.x = 0;
}

fn screen_touch_column(col: usize) void {
    if (col >= 0 and col < g_screen.col_count) {
        g_screen.lines[g_screen.y].cells[col].prop |= c.S_DIRTY;
    }
}

fn screen_touch_line() void {
    if (0 == (g_screen.lines[g_screen.y].isdirty & c.L_DIRTY)) {
        for (0..g_screen.col_count) |i| {
            g_screen.lines[g_screen.y].cells[i].prop &= ~c.S_DIRTY;
        }
        g_screen.lines[g_screen.y].isdirty |= c.L_DIRTY;
    }
}

export fn screen_standout() void {
    g_screen.mode |= c.S_STANDOUT;
}

export fn screen_standend() void {
    g_screen.mode &= ~c.S_STANDOUT;
}

export fn screen_toggle_stand() void {
    const p = g_screen.lines[g_screen.y].cells;
    p[g_screen.x].prop ^= c.S_STANDOUT;
    if (CHAR_MODE(p[g_screen.x].prop) != c.C_WCHAR2) {
        var i = g_screen.x + 1;
        while (CHAR_MODE(p[i].prop) == c.C_WCHAR2) : (i += 1) {
            p[i].prop ^= c.S_STANDOUT;
        }
    }
}

const M_SPACE = (c.S_SCREENPROP | c.S_COLORED | c.S_BCOLORED | c.S_GRAPHICS);
const M_CEOL = (~(M_SPACE | c.C_WHICHCHAR));

export fn screen_bold() void {
    g_screen.mode |= c.S_BOLD;
}

export fn screen_boldend() void {
    g_screen.mode &= ~c.S_BOLD;
}

export fn screen_underline() void {
    g_screen.mode |= c.S_UNDERLINE;
}

export fn screen_underlineend() void {
    g_screen.mode &= ~c.S_UNDERLINE;
}

export fn screen_graphstart() void {
    g_screen.mode |= c.S_GRAPHICS;
}

export fn screen_graphend() void {
    g_screen.mode &= ~c.S_GRAPHICS;
}

export fn screen_setfcolor(color: u16) void {
    g_screen.mode &= ~c.COL_FCOLOR;
    if ((color & 0xf) <= 7)
        g_screen.mode |= (((color & 7) | 8) << 8);
}

export fn screen_setbcolor(color: u16) void {
    g_screen.mode &= ~c.COL_BCOLOR;
    if ((color & 0xf) <= 7)
        g_screen.mode |= (((color & 7) | 8) << 12);
}

export fn screen_clear() void {
    var i: usize = 0;
    while (i < g_screen.line_count) : (i += 1) {
        for (g_screen.lines[i].cells[0..g_screen.col_capacity]) |*cell| {
            cell.* = .{
                .str = [1]u8{0} ** CELL_CHAR_LEN,
                .prop = c.S_EOL,
            };
        }
        g_screen.lines[i].cells[0].prop = c.S_EOL;
        g_screen.lines[i].isdirty = 0;
    }
    while (i < g_screen.line_capacity) : (i += 1) {
        g_screen.lines[i].isdirty = c.L_UNUSED;
    }

    c.screen_move(.{ .y = 0, .x = 0 });
    g_screen.mode = c.C_ASCII;
}

// XXX: conflicts with curses's clrtoeol(3) ?
// Clear to the end of line
export fn screen_clrtoeol() void {
    const p = g_screen.lines[g_screen.y].cells;

    if (p[g_screen.x].prop & c.S_EOL != 0)
        return;

    if (0 == (g_screen.lines[g_screen.y].isdirty & (c.L_NEED_CE | c.L_CLRTOEOL)) or
        g_screen.lines[g_screen.y].eol > g_screen.x)
        g_screen.lines[g_screen.y].eol = g_screen.x;

    g_screen.lines[g_screen.y].isdirty |= c.L_CLRTOEOL;
    screen_touch_line();
    for (g_screen.x..g_screen.col_count) |i| {
        if (p[i].prop & c.S_EOL != 0) {
            break;
        }
        p[i].prop = c.S_EOL | c.S_DIRTY;
    }
}

fn screen_clrtoeol_with_bcolor() void {
    if (0 == (g_screen.mode & c.S_BCOLORED)) {
        c.screen_clrtoeol();
        return;
    }
    const cli = g_screen.y;
    const cco = g_screen.x;
    const pr = g_screen.mode;
    g_screen.mode = (g_screen.mode & (M_CEOL | c.S_BCOLORED)) | c.C_ASCII;
    for (g_screen.x..g_screen.col_count) |_| {
        screen_add_whitespace();
    }
    c.screen_move(.{ .y = cli, .x = cco });
    g_screen.mode = pr;
}

export fn screen_clrtoeolx() void {
    screen_clrtoeol_with_bcolor();
}

fn screen_clrtobot_eol(clrtoeol: fn () callconv(.c) void) void {
    const y = g_screen.y;
    const x = g_screen.x;
    clrtoeol();
    g_screen.x = 0;
    g_screen.y += 1;
    while (g_screen.y < g_screen.line_count) : (g_screen.y += 1) {
        clrtoeol();
    }
    g_screen.y = y;
    g_screen.x = x;
}

export fn screen_clrtobotx() void {
    screen_clrtobot_eol(screen_clrtoeolx);
}

export fn screen_touch_cursor() void {
    // int i;
    screen_touch_line();
    {
        var i = g_screen.x;
        while (i >= 0) : (i -= 1) {
            screen_touch_column(i);
            if (CHAR_MODE(g_screen.lines[g_screen.y].cells[i].prop) != c.C_WCHAR2)
                break;
        }
    }
    for (g_screen.x + 1..g_screen.col_count) |i| {
        if (CHAR_MODE(g_screen.lines[g_screen.y].cells[i].prop) != c.C_WCHAR2)
            break;
        screen_touch_column(i);
    }
}

//
// write screen
//

const RefreshStatus = enum {
    RF_NEED_TO_MOVE,
    RF_CR_OK,
    RF_NONEED_TO_MOVE,
};

const M_MEND = (c.S_STANDOUT | c.S_UNDERLINE | c.S_BOLD | c.S_COLORED | c.S_BCOLORED | c.S_GRAPHICS);

fn color_seq(buf: []u8, colmode: c_int, highIntensityColors: bool) [:0]const u8 {
    // static char seqbuf[32];
    // sprintf(seqbuf, "\033[%dm", ((colmode >> 8) & 7) + (highIntensityColors ? 90 : 30));
    // return seqbuf;
    var val: c_int = (if (highIntensityColors) 90 else 30);
    val += ((colmode >> 8) & 7);
    return std.fmt.bufPrintZ(buf, "\x1b[{}m", .{val}) catch @panic("color_seq");
}

fn bcolor_seq(buf: []u8, colmode: c_int) [:0]const u8 {
    // static char seqbuf[32];
    // sprintf(seqbuf, "\033[%dm", ((colmode >> 12) & 7) + 40);
    // return seqbuf;
    const val = ((colmode >> 12) & 7) + 40;
    return std.fmt.bufPrintZ(buf, "\x1b[{}m", .{val}) catch @panic("bcolor_seq");
}

export fn tty_write_screen() void {
    // int line, col;
    var pline: usize = g_screen.y;
    var moved = RefreshStatus.RF_NEED_TO_MOVE;
    var mode: c.ScreenCellProperty = 0;
    var color = c.COL_FTERM;
    var bcolor = c.COL_BTERM;
    var graph_enabled = false;

    const g_runtime: *c.Runtime = c.getRuntime();
    var putc_status = PutcStatus.init(g_runtime.InnerCharset, g_runtime.DisplayCharset);

    defer putc_status.end(getOutputHandle());

    for (0..g_screen.line_count) |line| {
        const pLine = &g_screen.lines[line];
        const p = pLine.cells;
        const dirty = &pLine.isdirty;
        if (dirty.* & c.L_DIRTY != 0) {
            dirty.* &= ~c.L_DIRTY;
            var col: usize = 0;
            while (col < g_screen.col_count) : (col += 1) {
                if (p[col].prop & c.S_EOL != 0) {
                    break;
                }
                if (dirty.* & c.L_NEED_CE != 0 and col >= g_screen.lines[line].eol) {
                    if (screen_need_redraw(
                        &p[col].str[0],
                        p[col].prop,
                        SCREEN_SPACE,
                        0,
                    ))
                        break;
                } else {
                    if (p[col].prop & c.S_DIRTY != 0)
                        break;
                }
            }
            var pcol: usize = undefined;
            if (dirty.* & (c.L_NEED_CE | c.L_CLRTOEOL) != 0) {
                pcol = g_screen.lines[line].eol;
                if (pcol >= g_screen.col_count) {
                    dirty.* &= ~(c.L_NEED_CE | c.L_CLRTOEOL);
                    pcol = col;
                }
            } else {
                pcol = col;
            }
            if (line < g_screen.line_count - 2 and (line > 0 and pline == line - 1) and pcol == 0) {
                switch (moved) {
                    .RF_NEED_TO_MOVE => {
                        c.tty_MOVE(@intCast(line), 0);
                        moved = .RF_CR_OK;
                    },
                    .RF_CR_OK => {
                        _ = write1('\n');
                        _ = write1('\r');
                    },
                    .RF_NONEED_TO_MOVE => {
                        moved = .RF_CR_OK;
                    },
                }
            } else {
                c.tty_MOVE(@intCast(line), @intCast(pcol));
                moved = .RF_CR_OK;
            }
            if (dirty.* & (c.L_NEED_CE | c.L_CLRTOEOL) != 0) {
                writestr(g_runtime.termcap._ce);
                if (col != pcol)
                    c.tty_MOVE(@intCast(line), @intCast(col));
            }
            pline = line;
            pcol = col;
            while (col < g_runtime.cols) : (col += 1) {
                if (p[col].prop & c.S_EOL != 0)
                    break;

                // some terminal emulators do linefeed when a
                // character is put on COLS-th column. this behavior
                // is different from one of vt100, but such terminal
                // emulators are used as vt100-compatible
                // emulators. This behaviour causes scroll when a
                // character is drawn on (COLS-1,LINES-1) point.  To
                // avoid the scroll, I prohibit to draw character on
                // (COLS-1,LINES-1).
                if ((0 == (p[col].prop & c.S_STANDOUT) and (mode & c.S_STANDOUT) != 0) //
                or (0 == (p[col].prop & c.S_UNDERLINE) and (mode & c.S_UNDERLINE) != 0) //
                or (0 == (p[col].prop & c.S_BOLD) and (mode & c.S_BOLD) != 0) //
                or (0 == (p[col].prop & c.S_COLORED) and (mode & c.S_COLORED) != 0) //
                or (0 == (p[col].prop & c.S_BCOLORED) and (mode & c.S_BCOLORED) != 0) //
                or (0 == (p[col].prop & c.S_GRAPHICS) and (mode & c.S_GRAPHICS) != 0)) {
                    if ((mode & c.S_COLORED) != 0 or (mode & c.S_BCOLORED) != 0)
                        writestr(g_runtime.termcap._op);
                    if (mode & c.S_GRAPHICS != 0)
                        writestr(g_runtime.termcap._ae);
                    writestr(g_runtime.termcap._me);
                    mode &= ~M_MEND;
                }
                if (if (dirty.* & c.L_NEED_CE != 0 and col >= g_screen.lines[line].eol)
                    screen_need_redraw(
                        &p[col].str[0],
                        p[col].prop,
                        SCREEN_SPACE,
                        0,
                    )
                else
                    (p[col].prop & c.S_DIRTY != 0))
                {
                    if (col > 0 and pcol == col - 1) {
                        writestr(g_runtime.termcap._nd);
                    } else if (pcol != col) {
                        c.tty_MOVE(@intCast(line), @intCast(col));
                    }

                    if ((p[col].prop & c.S_STANDOUT != 0) and 0 == (mode & c.S_STANDOUT)) {
                        writestr(g_runtime.termcap._so);
                        mode |= c.S_STANDOUT;
                    }
                    if ((p[col].prop & c.S_UNDERLINE != 0) and 0 == (mode & c.S_UNDERLINE)) {
                        writestr(g_runtime.termcap._us);
                        mode |= c.S_UNDERLINE;
                    }
                    if ((p[col].prop & c.S_BOLD != 0) and 0 == (mode & c.S_BOLD)) {
                        writestr(g_runtime.termcap._md);
                        mode |= c.S_BOLD;
                    }
                    if ((p[col].prop & c.S_COLORED != 0) and (p[col].prop ^ mode) & c.COL_FCOLOR != 0) {
                        color = (p[col].prop & c.COL_FCOLOR);
                        mode = ((mode & ~c.COL_FCOLOR) | color);
                        var buf: [32]u8 = undefined;
                        writestr(color_seq(&buf, color, g_runtime.highIntensityColors != 0).ptr);
                    }
                    if ((p[col].prop & c.S_BCOLORED != 0) and (p[col].prop ^ mode) & c.COL_BCOLOR != 0) {
                        bcolor = (p[col].prop & c.COL_BCOLOR);
                        mode = ((mode & ~c.COL_BCOLOR) | bcolor);
                        var buf: [32]u8 = undefined;
                        writestr(bcolor_seq(&buf, bcolor).ptr);
                    }

                    if ((p[col].prop & c.S_GRAPHICS != 0) and 0 == (mode & c.S_GRAPHICS)) {
                        putc_status.end(getOutputHandle());
                        if (!graph_enabled) {
                            graph_enabled = true;
                            writestr(g_runtime.termcap._eA);
                        }
                        writestr(g_runtime.termcap._as);
                        mode |= c.S_GRAPHICS;
                    }
                    if (p[col].prop & c.S_GRAPHICS != 0) {
                        _ = write1(c.graphchar(p[col].str[0]));
                    } else if (CHAR_MODE(p[col].prop) != c.C_WCHAR2) {
                        putc_status.putc(&p[col].str[0], getOutputHandle());
                    }
                    pcol = col + 1;
                }
            }
            if (col == g_runtime.cols)
                moved = .RF_NEED_TO_MOVE;
            while (col < g_runtime.cols and 0 == (p[col].prop & c.S_EOL)) : (col += 1) {
                p[col].prop |= c.S_EOL;
            }
        }
        dirty.* &= ~(c.L_NEED_CE | c.L_CLRTOEOL);
        if (mode & M_MEND != 0) {
            if (mode & (c.S_COLORED | c.S_BCOLORED) != 0)
                writestr(g_runtime.termcap._op);
            if (mode & c.S_GRAPHICS != 0) {
                writestr(g_runtime.termcap._ae);
                putc_status.clear();
            }
            writestr(g_runtime.termcap._me);
            mode &= ~M_MEND;
        }
    }
}

//
// child process
//
const ArgSplitter = struct {
    seq: []const u8,
    pos: usize = 0,

    fn init(seq: []const u8) @This() {
        return .{
            .seq = seq,
        };
    }

    fn isEnd(this: @This()) bool {
        return this.pos >= this.seq.len;
    }

    fn next(this: *@This()) ?[]const u8 {
        if (this.pos >= this.seq.len) {
            return null;
        }

        // skip white space
        while (this.pos < this.seq.len //
        and std.ascii.isWhitespace(this.seq[this.pos])) {
            this.pos += 1;
        }
        const begin = this.pos;

        // search end
        while (this.pos < this.seq.len //
        and !std.ascii.isWhitespace(this.seq[this.pos])) {
            this.pos += 1;
        }
        const end = this.pos;

        if (begin < end) {
            return this.seq[begin..end];
        } else {
            return null;
        }
    }
};

test "ArgSplitter" {
    {
        var sp = ArgSplitter.init("nvim \"hoge.txt\"");
        try std.testing.expectEqualSlices(u8, "nvim", sp.next().?);
        try std.testing.expectEqualSlices(u8, "\"hoge.txt\"", sp.next().?);
        try std.testing.expectEqual(null, sp.next());
        try std.testing.expect(sp.isEnd());
    }
    {
        var sp = ArgSplitter.init(" a b c ");
        try std.testing.expectEqualSlices(u8, "a", sp.next().?);
        try std.testing.expectEqualSlices(u8, "b", sp.next().?);
        try std.testing.expectEqualSlices(u8, "c", sp.next().?);
        try std.testing.expectEqual(null, sp.next());
        try std.testing.expect(sp.isEnd());
    }
}

fn allocArgv(allocator: std.mem.Allocator, src: []const u8) ![]const []const u8 {
    var argv: std.ArrayList([]const u8) = .{};
    defer argv.deinit(allocator);

    var it = ArgSplitter.init(src);
    while (it.next()) |arg| {
        try argv.append(allocator, arg);
    }

    return try argv.toOwnedSlice(allocator);
}

test "allocArgv" {
    const cmd = "vim hoge";
    const cmd_argv: []const []const u8 = &.{ "vim", "hoge" };
    const argv = try allocArgv(std.testing.allocator, cmd);
    defer std.testing.allocator.free(argv);

    // try std.testing.expectEqual(cmd_argv.len, argv.len);
    for (cmd_argv, argv) |l, r| {
        try std.testing.expectEqualSlices(u8, l, r);
    }
}

export fn blockChild(cmd: [*c]const u8) u8 {
    const argv = allocArgv(g_allocator, std.mem.span(cmd)) catch @panic("blockChild");
    defer g_allocator.free(argv);
    var child = std.process.Child.init(argv, g_allocator);
    child.stdin_behavior = .Inherit;
    child.stdout_behavior = .Inherit;
    child.stderr_behavior = .Inherit;

    exitRawMode();
    defer enterRawMode();

    if (child.spawnAndWait()) |ret| {
        return ret.Exited;
    } else |_| {
        std.debug.print("\n[Hit any key]", .{});
        flush_tty();
        _ = getch();
        return 1;
    }
}

export fn exec_cmd(cmd: [*c]const u8) c_int {
    exitRawMode();
    const rv = c.system(cmd);
    if (rv == 0) {
        // success
        enterRawMode();
        return 0;
    } else {
        // error
        std.debug.print("\n[Hit any key]", .{});
        flush_tty();
        _ = getch();
        enterRawMode();
        return rv;
    }
}

fn screen_add_whitespace() void {
    const white_space = " ";
    screen_addmch(white_space, 1, 1);
}

export fn screen_wc_addstr(_s: [*c]const u8) void {
    var s = _s;
    while (s[0] != 0) {
        const len = c.wtf_len(s);
        const width = c.wtf_width(s);
        screen_addmch(s, len, width);
        s += len;
    }
}

export fn screen_wc_addstr_width(_s: [*c]const u8, n: usize) void {
    var s = _s;
    var i: usize = 0;
    while (s[0] != 0) {
        const width = c.wtf_width(s);
        if (i + width > n)
            break;
        const len = c.wtf_len(s);
        screen_addmch(s, len, width);
        s += len;
        i += width;
    }
}

export fn screen_wc_addnstr_sup(_s: [*c]const u8, n: usize) void {
    var i: usize = 0;
    var s = _s;
    while (s[0] != 0) {
        const width: usize = @intCast(c.wtf_width(s));
        if (i + width > n)
            break;
        const len = c.wtf_len(s);
        screen_addmch(s, len, width);
        s += len;
        i += width;
    }
    while (i < n) : (i += 1) {
        screen_add_whitespace();
    }
}

const LineInput = @import("LineInput.zig");
var g_linein = LineInput{};

export fn inputLineHistSearch(
    prompt: [*c]const u8,
    def_str: [*c]const u8,
    flag: c.LineInputFlags,
    hist: ?*c.Hist,
    incrfunc: c.IncrFunc,
    doc: *c.Document,
) [*c]const u8 {
    return g_linein.input(.{
        .prompt = prompt,
        .def_str = def_str,
        .flag = flag,
        .hist = hist,
        .incrfunc = incrfunc,
        .doc = doc,
    });
}

export fn inputAnswer(prompt: [*c]const u8) [*c]const u8 {
    if (c.getRuntime().*.QuietMessage != 0)
        return "n";

    if (fmInitialized()) {
        c.enterRawMode();
        return c.inputChar(prompt);
    } else {
        c.writestr(prompt);
        c.flush_tty();
        return c.Strfgets(c.stdin).*.ptr;
    }
}

// static Str
// make_optional_header_string(char* s)
// {
//     if (strchr(s, '\n') || strchr(s, '\r'))
//         return NULL;
//
//     const char* p;
//     for (p = s; *p && *p != ':'; p++)
//         ;
//     if (*p != ':' || p == s)
//         return NULL;
//
//     Str hs = Strnew_size(strlen(s) + 3);
//     Strcopy_charp_n(hs, s, p - s);
//     if (!Strcasecmp_charp(hs, "content-type"))
//         getRuntime()->override_content_type = TRUE;
//     if (!Strcasecmp_charp(hs, "user-agent"))
//         getRuntime()->override_user_agent = TRUE;
//     Strcat_charp(hs, ": ");
//     if (*(++p)) { /* not null header */
//         p = skip_blanks(p); /* skip white spaces */
//         Strcat_charp(hs, p);
//     }
//     Strcat_charp(hs, "\r\n");
//     return hs;
// }
//
// static void*
// die_oom(size_t bytes)
// {
//     fprintf(stderr, "Out of memory: %lu bytes unavailable!\n", (unsigned long)bytes);
//     exit(1);
//     /*
//      * Suppress compiler warning: function might return no value
//      * This code is never reached.
//      */
//     return NULL;
// }
