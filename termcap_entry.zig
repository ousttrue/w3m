const std = @import("std");
const c = @cImport({
    @cInclude("termcap.h");
});

fn GETSTR(dst: *[*:0]const u8, p: **u8, s: [*c]const u8) bool {
    const suc = c.tgetstr(s, @ptrCast(p));
    if (suc != null) {
        dst.* = suc;
        return true;
    } else {
        dst.* = "";
        return false;
    }
}
const TermCapEntry = extern struct {
    bp: [1024]u8 = undefined,
    funcstr: [256]u8 = undefined,
    /// clear to the end of display
    T_cd: [*:0]const u8 = "",
    /// clear to the end of line
    T_ce: [*:0]const u8 = "",
    /// cursor right
    T_kr: [*:0]const u8 = "",
    /// cursor left
    T_kl: [*:0]const u8 = "",
    /// carriage return
    T_cr: [*:0]const u8 = "",
    T_bt: [*:0]const u8 = "",
    /// tab
    T_ta: [*:0]const u8 = "",
    /// save cursor
    T_sc: [*:0]const u8 = "",
    /// restore cursor
    T_rc: [*:0]const u8 = "",
    /// standout mode
    T_so: [*:0]const u8 = "",
    /// standout mode end
    T_se: [*:0]const u8 = "",
    /// underline mode
    T_us: [*:0]const u8 = "",
    /// underline mode end
    T_ue: [*:0]const u8 = "",
    /// clear screen
    T_cl: [*:0]const u8 = "",
    /// cursor move
    T_cm: [*:0]const u8 = "",
    /// append line
    T_al: [*:0]const u8 = "",
    /// scroll reverse
    T_sr: [*:0]const u8 = "",
    /// bold mode
    T_md: [*:0]const u8 = "",
    /// bold mode end
    T_me: [*:0]const u8 = "",
    /// terminal init
    T_ti: [*:0]const u8 = "",
    /// terminal end
    T_te: [*:0]const u8 = "",
    /// move right one space
    T_nd: [*:0]const u8 = "",
    /// alternative (graphic) charset start
    T_as: [*:0]const u8 = "",
    /// alternative (graphic) charset end
    T_ae: [*:0]const u8 = "",
    /// enable alternative charset
    T_eA: [*:0]const u8 = "",
    /// graphics charset pairs
    T_ac: [*:0]const u8 = "",
    /// set default color pair to its original value
    T_op: [*:0]const u8 = "",
    LINES: i32 = 0,
    COLS: i32 = 0,

    fn load(self: *@This(), ent: [:0]const u8) bool {
        if (c.tgetent(&self.bp[0], &ent[0]) != 1) {
            return false;
        }

        var pt = &self.funcstr[0];
        _ = GETSTR(&self.T_ce, &pt, "ce");
        _ = GETSTR(&self.T_cd, &pt, "cd");
        if (!GETSTR(&self.T_kr, &pt, "nd")) {
            _ = GETSTR(&self.T_kr, &pt, "kr");
        }
        if (c.tgetflag("bs") != 0) {
            self.T_kl = "\x08"; // "\b";
        } else {
            if (!GETSTR(&self.T_kl, &pt, "le")) {
                if (!GETSTR(&self.T_kl, &pt, "kb")) {
                    _ = GETSTR(&self.T_kl, &pt, "kl");
                }
            }
        }
        _ = GETSTR(&self.T_cr, &pt, "cr");
        _ = GETSTR(&self.T_ta, &pt, "ta");
        _ = GETSTR(&self.T_sc, &pt, "sc");
        _ = GETSTR(&self.T_rc, &pt, "rc");
        _ = GETSTR(&self.T_so, &pt, "so");
        _ = GETSTR(&self.T_se, &pt, "se");
        _ = GETSTR(&self.T_us, &pt, "us");
        _ = GETSTR(&self.T_ue, &pt, "ue");
        _ = GETSTR(&self.T_md, &pt, "md");
        _ = GETSTR(&self.T_me, &pt, "me");
        _ = GETSTR(&self.T_cl, &pt, "cl");
        _ = GETSTR(&self.T_cm, &pt, "cm");
        _ = GETSTR(&self.T_al, &pt, "al");
        _ = GETSTR(&self.T_sr, &pt, "sr");
        _ = GETSTR(&self.T_ti, &pt, "ti");
        _ = GETSTR(&self.T_te, &pt, "te");
        _ = GETSTR(&self.T_nd, &pt, "nd");
        _ = GETSTR(&self.T_eA, &pt, "eA");
        _ = GETSTR(&self.T_as, &pt, "as");
        _ = GETSTR(&self.T_ae, &pt, "ae");
        _ = GETSTR(&self.T_ac, &pt, "ac");
        _ = GETSTR(&self.T_op, &pt, "op");
        self.LINES = c.tgetnum("li");
        self.COLS = c.tgetnum("co");
        return true;
    }
};

pub fn main() void {
    const name = "vt100";
    var e = TermCapEntry{};
    if (!e.load(name)) {
        return;
    }

    std.debug.print("{s}\n", .{name});
}
