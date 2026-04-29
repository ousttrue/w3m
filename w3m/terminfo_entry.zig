const std = @import("std");
const c = @import("c.zig").c;

const g = @import("global.zig");
const runtime = @import("runtime.zig");

extern fn tgetent(bp: [*c]u8, name: [*c]const u8) c_int;

// extern int tgetnum(char*);
extern fn tgetflag(name: [*c]const u8) c_int;
extern fn tgetstr(name: [*c]const u8, bp: [*c]u8) [*c]u8;

fn setgraphchar(ti: *c.TermInfo) void {
    for (0..96) |i| {
        ti.gcmap[i] = @intCast(i + ' ');
    }

    if (ti.T_ac != null) {
        // TODO:
        //         int n = strlen(ti.T_ac);
        //         for (int i = 0; i < n - 1; i += 2) {
        //             uint8_t c = (uint8_t)ti.T_ac[i] - ' ';
        //             if (c >= 0 && c < 96)
        //                 ti.gcmap[c] = ti.T_ac[i + 1];
        //         }
    }
}

export fn getTCstr(ti: *c.TermInfo) void {
    const ent = runtime.environ_map.get("TERM") orelse {
        @panic("TERM is not set");
    };

    const r = tgetent(&ti.bp[0], ent.ptr);
    if (r != 1) {
        // Can't find termcap entry
        @panic("Can't find termcap entry");
    }

    var pt = ti.funcstr;
    ti.T_ce = tgetstr("ce", &pt);
    ti.T_cd = tgetstr("cd", &pt);
    ti.T_kr = tgetstr("nd", &pt);
    if (null == ti.T_kr)
        ti.T_kr = tgetstr("kr", &pt);
    if (0 != tgetflag("bs")) {
        ti.T_kl = 8; //"\b";
    } else {
        ti.T_kl = tgetstr("le", &pt);
        if (null == ti.T_kl)
            ti.T_kl = tgetstr("kb", &pt);
        if (null == ti.T_kl)
            ti.T_kl = tgetstr("kl", &pt);
    }
    ti.T_cr = tgetstr("cr", &pt);
    ti.T_ta = tgetstr("ta", &pt);
    ti.T_sc = tgetstr("sc", &pt);
    ti.T_rc = tgetstr("rc", &pt);
    ti.T_so = tgetstr("so", &pt);
    ti.T_se = tgetstr("se", &pt);
    ti.T_us = tgetstr("us", &pt);
    ti.T_ue = tgetstr("ue", &pt);
    ti.T_md = tgetstr("md", &pt);
    ti.T_me = tgetstr("me", &pt);
    ti.T_cl = tgetstr("cl", &pt);
    ti.T_cm = tgetstr("cm", &pt);
    ti.T_al = tgetstr("al", &pt);
    ti.T_sr = tgetstr("sr", &pt);
    ti.T_ti = tgetstr("ti", &pt);
    ti.T_te = tgetstr("te", &pt);
    ti.T_nd = tgetstr("nd", &pt);
    ti.T_eA = tgetstr("eA", &pt);
    ti.T_as = tgetstr("as", &pt);
    ti.T_ae = tgetstr("ae", &pt);
    ti.T_ac = tgetstr("ac", &pt);
    ti.T_op = tgetstr("op", &pt);

    setgraphchar(ti);
}

export fn writestr(f: c.PutC, s: [*c]const u8) void {
    _ = c.tputs(s, 1, f);
}

export fn terminfo_reset(f: c.PutC, ti: *c.TermInfo, do_not_use_ti_te: bool) void {
    // turn off
    writestr(f, ti.T_op);
    writestr(f, ti.T_me);
    if (!do_not_use_ti_te) {
        if (ti.T_te != null and ti.T_te[0] != 0) {
            writestr(f, ti.T_te);
        } else {
            writestr(f, ti.T_cl);
        }
    }
    // reset terminal
    writestr(f, ti.T_se);
}

export fn MOVE(f: c.PutC, ti: *c.TermInfo, line: c_int, column: c_int) void {
    _ = c.tputs(c.tgoto(ti.T_cm, column, line), 1, f);
}

export fn graph_ok(_ti: ?*c.TermInfo) bool {
    if (g.UseGraphicChar != c.GRAPHIC_CHAR_DEC)
        return false;
    const ti = _ti orelse {
        return false;
    };
    return ti.T_as[0] != 0 and ti.T_ae[0] != 0 and ti.T_ac[0] != 0;
}
