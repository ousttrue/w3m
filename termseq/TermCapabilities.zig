const std = @import("std");

extern fn tgetent(bp: [*]u8, name: [*:0]const u8) c_int;
extern fn tgetnum(cap: [*:0]const u8) c_int;
extern fn tgetflag(cap: [*:0]const u8) c_int;
extern fn tgetstr(cap: [*:0]const u8, buf: [*][*]u8) ?[*:0]u8;

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
            if (tgetstr(termcap, @ptrCast(p))) |value| {
                self.value = std.mem.span(value);
            }
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
cursor_right_one_space: StringCapability(.{
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
    if (t.cursor_right_one_space.get(&pt)) |value| {
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
    _ = t.cursor_right_one_space.get(&pt);
    _ = t.alternative_charset_enable.get(&pt);
    _ = t.alternative_charset_start.get(&pt);
    _ = t.alternative_charset_end.get(&pt);
    _ = t.line_graphic.get(&pt);
    _ = t.color_default.get(&pt);
    return true;
}

pub const xterm = @This(){
    .term_init = .{ .value = "\x1b[?1049h\x1b[22;0;0t" },
    .term_end = .{ .value = "\x1b[?1049l\x1b[23;0;0t" },
    .clear_screen = .{ .value = "\x1b[H\x1b[2J" },
    .clear_to_eod = .{ .value = "\x1b[J" },
    .clear_to_eol = .{ .value = "\x1b[K" },
    .cursor_right = .{ .value = "\x1b[C" },
    .cursor_left = .{ .value = "0x8" },
    .cursor_right_one_space = .{ .value = "\x1b[C" },
    .carriage_return = .{ .value = "\x0d" },
    .tab = .{ .value = "\x09" },
    // https://gist.github.com/Orc/e1caee6695ebd3eab5320171c7073d33
    .cursor_move = .{ .value = "\x1b[%i%p1%d;%p2%dH" },
    .cursor_save = .{ .value = "\x1b7" },
    .cursor_restore = .{ .value = "\x1b8" },
    .standout_start = .{ .value = "\x1b[7m" },
    .standout_end = .{ .value = "\x1b[27m" },
    .underline_start = .{ .value = "\x1b[4m" },
    .underline_end = .{ .value = "\x1b[24m" },
    .bold_start = .{ .value = "\x1b[1m" },
    .bold_end = .{ .value = "\x1b[0m" },
    .append_line = .{ .value = "\x1b[L" },
    .scroll_reverse = .{ .value = "\x1bM" },
    .alternative_charset_start = .{ .value = "\x1b(0" },
    .alternative_charset_end = .{ .value = "\x1b(B" },
    .line_graphic = .{ .value = "``aaffggiijjkkllmmnnooppqqrrssttuuvvwwxxyyzz{{||}}~~" },
    .color_default = .{ .value = "\x1b[39;49m" },
};
