const std = @import("std");

extern fn tgetent(bp: [*]u8, name: [*:0]const u8) c_int;
extern fn tgetnum(cap: [*:0]const u8) c_int;
extern fn tgetflag(cap: [*:0]const u8) c_int;
extern fn tgetstr(cap: [*:0]const u8, buf: [*][*]u8) [*:0]u8;

fn print_seq(writer: anytype, _p: *const u8) void {
    var p = _p;
    while (p.* != 0) : (p = @ptrFromInt(@intFromPtr(p) + 1)) {
        //
        switch (p.*) {
            0x1b => {
                writer.print("[esc]", .{}) catch @panic("[esc]");
            },
            else => {
                if (std.ascii.isPrint(p.*)) {
                    writer.print("{c}", .{p.*}) catch @panic("");
                } else {
                    writer.print("0x{x}", .{p.*}) catch @panic("");
                }
            },
        }
    }
}

pub fn StringCapability(args: struct {
    termcap: [:0]const u8,
    desc: [:0]const u8,
}) type {
    return struct {
        var termcap = args.termcap;
        var desc = args.desc;
        value: ?[:0]const u8 = null,

        fn get(self: *@This(), p: **u8) ?[:0]const u8 {
            self.value = std.mem.span(tgetstr(termcap, @ptrCast(p)));
            return self.value;
        }

        pub fn format(
            self: @This(),
            comptime fmt: []const u8,
            options: std.fmt.FormatOptions,
            writer: anytype,
        ) !void {
            _ = fmt;
            _ = options;

            writer.print(
                "[str]{s}: {s} => ",
                .{ termcap, desc },
            ) catch unreachable;
            if (self.value) |value| {
                print_seq(writer, @ptrCast(value));
            } else {
                writer.print("null", .{}) catch @panic("null");
            }
        }
    };
}

pub const IntCapability = struct {
    desc: [:0]const u8,
    termcap: [:0]const u8,
    value: ?i32 = null,

    fn get(self: *@This()) void {
        self.value = tgetnum(self.termcap);
    }

    pub fn format(
        self: @This(),
        comptime fmt: []const u8,
        options: std.fmt.FormatOptions,
        writer: anytype,
    ) !void {
        _ = fmt;
        _ = options;

        writer.print(
            "[int]{s}: {s} => {any}",
            .{ self.termcap, self.desc, self.value },
        ) catch unreachable;
    }
};

pub const TermCapabilities = @This();

// initialize
term_init: StringCapability(.{
    .desc = "terminal init",
    .termcap = "ti",
}) = .{},
term_end: StringCapability(.{
    .desc = "terminal end",
    .termcap = "te",
}) = .{},
// clear
clear_screen: StringCapability(.{
    .termcap = "cl",
    .desc = "clear screen",
}) = .{},
clear_to_eod: StringCapability(.{
    .termcap = "cd",
    .desc = "clear to the end of display",
}) = .{},
clear_to_eol: StringCapability(.{
    .desc = "clear to the end of line",
    .termcap = "ce",
}) = .{},
// keypad
cursor_right: StringCapability(.{
    .desc = "cursor right",
    .termcap = "kr",
}) = .{},
cursor_left: StringCapability(.{
    .desc = "cursor left",
    .termcap = "kl",
}) = .{},
// cursor local
move_right_one_space: StringCapability(.{
    .desc = "move right one space",
    .termcap = "nd",
}) = .{},
carriage_return: StringCapability(.{
    .desc = "carriage return",
    .termcap = "cr",
}) = .{},
// tab
back_tab: StringCapability(.{
    .desc = "back tab",
    .termcap = "bt",
}) = .{},
tab: StringCapability(.{
    .desc = "tab",
    .termcap = "ta",
}) = .{},
// cursor absolute
cursor_move: StringCapability(.{
    .desc = "cursor move",
    .termcap = "cm",
}) = .{},
cursor_save: StringCapability(.{
    .desc = "save cursor",
    .termcap = "sc",
}) = .{},
cursor_restore: StringCapability(.{
    .desc = "restore cursor",
    .termcap = "rc",
}) = .{},
// effect
standout_start: StringCapability(.{
    .desc = "standout mode",
    .termcap = "so",
}) = .{},
standout_end: StringCapability(.{
    .desc = "standout mode end",
    .termcap = "se",
}) = .{},
underline_start: StringCapability(.{
    .desc = "underline mode",
    .termcap = "us",
}) = .{},
underline_end: StringCapability(.{
    .desc = "underline mode end",
    .termcap = "ue",
}) = .{},
bold_start: StringCapability(.{
    .desc = "bold mode",
    .termcap = "md",
}) = .{},
bold_end: StringCapability(.{
    .desc = "bold mode end",
    .termcap = "me",
}) = .{},
// edit
append_line: StringCapability(.{
    .desc = "append line",
    .termcap = "al",
}) = .{},
// scroll
scroll_reverse: StringCapability(.{
    .desc = "scroll reverse",
    .termcap = "sr",
}) = .{},
// charset
alternative_charset_start: StringCapability(.{
    .desc = "alternative (graphic) charset start",
    .termcap = "as",
}) = .{},
alternative_charset_end: StringCapability(.{
    .desc = "alternative (graphic) charset end",
    .termcap = "ae",
}) = .{},
alternative_charset_enable: StringCapability(.{
    .desc = "enable alternative charset",
    .termcap = "eA",
}) = .{},
line_graphic: StringCapability(.{
    .desc = "graphics charset pairs",
    .termcap = "ac",
}) = .{},
// color
color_default: StringCapability(.{
    .desc = "set default color pair to its original value",
    .termcap = "op",
}) = .{},

LINES: IntCapability = .{
    .desc = "number of line",
    .termcap = "li",
},
COLS: IntCapability = .{
    .desc = "number of column",
    .termcap = "co",
},
bp: [1024]u8 = undefined,
funcstr: [256]u8 = undefined,

pub fn isGraphOk(self: @This()) bool {
    return self.alternative_charset_start.value != null and
        self.alternative_charset_end.value != null and
        self.line_graphic.value != null;
}

pub fn load(t: *@This(), name: [*:0]const u8) bool {
    if (tgetent(@ptrCast(&t.bp[0]), name) != 1) {
        return false;
    }

    var pt = &t.funcstr[0];
    _ = t.clear_to_eol.get(&pt);
    _ = t.clear_to_eod.get(&pt);
    if (t.move_right_one_space.get(&pt)) |value| {
        t.cursor_right.value = value;
    } else {
        _ = t.cursor_right.get(&pt);
    }

    if (tgetflag("bs") != 0) {
        t.cursor_left.value = "\x08";
    } else {
        t.cursor_left.value = std.mem.span(tgetstr("le", @ptrCast(&pt)));
        if (t.cursor_left.value == null) {
            t.cursor_left.value = std.mem.span(tgetstr("kb", @ptrCast(&pt)));
        }
        if (t.cursor_left.value == null) {
            t.cursor_left.value = std.mem.span(tgetstr("kl", @ptrCast(&pt)));
        }
    }
    _ = t.carriage_return.get(&pt);
    _ = t.tab.get(&pt);
    _ = t.cursor_save.get(&pt);
    _ = t.cursor_restore.get(&pt);
    _ = t.standout_start.get(&pt);
    _ = t.standout_end.get(&pt);
    _ = t.underline_start.get(&pt);
    _ = t.underline_end.get(&pt);
    _ = t.bold_start.get(&pt);
    _ = t.bold_end.get(&pt);
    _ = t.clear_screen.get(&pt);
    _ = t.cursor_move.get(&pt);
    _ = t.append_line.get(&pt);
    _ = t.scroll_reverse.get(&pt);
    _ = t.term_init.get(&pt);
    _ = t.term_end.get(&pt);
    _ = t.move_right_one_space.get(&pt);
    _ = t.alternative_charset_enable.get(&pt);
    _ = t.alternative_charset_start.get(&pt);
    _ = t.alternative_charset_end.get(&pt);
    _ = t.line_graphic.get(&pt);
    _ = t.color_default.get(&pt);
    t.LINES.get();
    t.COLS.get();
    return true;
}

pub const xterm = @This(){
    .term_init = .{ .value = "\x1b[?1049h\x1b[22;0;0t" },
    // [str]te: terminal end => [esc][?1049l[esc][23;0;0t
    // [str]cl: clear screen => [esc][H[esc][2J
    // [str]cd: clear to the end of display => [esc][J
    // [str]ce: clear to the end of line => [esc][K
    // [str]kr: cursor right => [esc][C
    // [str]kl: cursor left => 0x8
    // [str]nd: move right one space => [esc][C
    // [str]cr: carriage return => 0xd
    // [str]bt: back tab => null
    // [str]ta: tab => 0x9
    // [str]cm: cursor move => [esc][%i%p1%d;%p2%dH
    // [str]sc: save cursor => [esc]7
    // [str]rc: restore cursor => [esc]8
    // [str]so: standout mode => [esc][7m
    // [str]se: standout mode end => [esc][27m
    // [str]us: underline mode => [esc][4m
    // [str]ue: underline mode end => [esc][24m
    // [str]md: bold mode => [esc][1m
    // [str]me: bold mode end => [esc][0m
    // [str]al: append line => [esc][L
    // [str]sr: scroll reverse => [esc]M
    // [str]as: alternative (graphic) charset start => [esc](0
    // [str]ae: alternative (graphic) charset end => [esc](B
    // [str]eA: enable alternative charset => null
    // [str]ac: graphics charset pairs => ``aaffggiijjkkllmmnnooppqqrrssttuuvvwwxxyyzz{{||}}~~
    // [str]op: set default color pair to its original value => [esc][39;49m
    // [int]li: number of line => 24
    // [int]co: number of column => 80
};
