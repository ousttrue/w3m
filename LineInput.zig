const std = @import("std");
const c = @import("c_include.zig").c;

const Options = struct {
    prompt: [*c]const u8,
    def_str: [*c]const u8,
    flag: c.LineInputFlags,
    hist: ?*c.Hist,
    incrfunc: c.IncFunc,
};

const CPL_MODE = enum {
    CPL_NEVER,
    CPL_OFF,
    CPL_ON,
    CPL_ALWAYS,
    CPL_URL,
};

const STR_LEN = 1024;

const KeyFunc = *const fn (this: *@This(), ch: u8) void;

strBuf: c.Str = null,
strProp: [STR_LEN]c.Lineprop = undefined,

// static Str CompleteBuf;
// static Str CFileName;
// static Str CBeforeBuf;
// static Str CAfterBuf;
// static Str CDirBuf;
// static char** CFileBuf = NULL;
// static int NCFileBuf;
// static int NCFileOffset;

CPos: usize = 0,
CLen: usize = 0,
offset: usize = 0,
i_cont: bool = false,
i_broken: bool = false,
i_quote: bool = false,
cm_mode: std.EnumSet(CPL_MODE) = .{},
cm_next: bool = false,
cm_clear: bool = false,
cm_disp_next: i32 = -1,
cm_disp_clear: bool = false,
need_redraw: bool = false,
is_passwd: bool = false,
move_word: bool = true,
CurrentHist: ?*c.Hist = null,
strCurrentBuf: c.Str = null,
use_hist: bool = false,

InputKeymap: [32]KeyFunc = .{
    // C-@  C-a   C-b   C-c     C-d   C-e   C-f   C-g
    _compl, _mvB, _mvL, _inbrk, delC, _mvE, _mvR, _inbrk, //
    // C-h C-i  C-j     C-k    C-l    C-m     C-n    C-o
    _bs, iself, _enter, killn, iself, _enter, _next, _editor, //
    // C-p C-q   C-r   C-s    C-t    C-u    C-v   C-w
    _prev,   _quo,  _bsw,  iself, _mvLw, killb, _quo,  _bsw, //
    // C-x C-y   C-z   C-[    C-\    C-]    C-^   C-_
    _tcompl, _mvRw, iself, _esc,  iself, iself, iself, iself,
},

pub fn input(this: *@This(), opts: Options) [*c]const u8 {
    this.is_passwd = false;
    this.move_word = true;
    this.CurrentHist = opts.hist;
    if (opts.hist != null) {
        this.use_hist = true;
        this.strCurrentBuf = null;
    } else {
        this.use_hist = false;
    }

    if (opts.flag & c.IN_URL != 0) {
        this.cm_mode = .initMany(&.{ .CPL_ALWAYS, .CPL_URL });
    } else if (opts.flag & c.IN_FILENAME != 0) {
        this.cm_mode = .initOne(.CPL_ALWAYS);
    } else if (opts.flag & c.IN_PASSWORD != 0) {
        this.cm_mode = .initOne(.CPL_NEVER);
        this.is_passwd = true;
        this.move_word = false;
    } else if (opts.flag & c.IN_COMMAND != 0) {
        this.cm_mode = .initOne(.CPL_ON);
    } else {
        this.cm_mode = .initOne(.CPL_OFF);
    }
    const opos = c.get_strwidth(opts.prompt);
    const _epos = c.TTY_COLS() - 2 - opos;
    const epos: usize = @intCast(@max(0, _epos));
    const lpos: usize = @intCast(@divTrunc(epos, 3));
    const rpos: usize = @intCast(@divTrunc(epos * 2, 3));
    this.offset = 0;

    if (opts.def_str) |def_str| {
        this.strBuf = c.Strnew_charp(def_str);
        this.CPos = this.setStrType(this.strBuf, &this.strProp);
        this.CLen = this.CPos;
    } else {
        this.strBuf = c.Strnew();
        this.CLen = 0;
        this.CPos = 0;
    }

    this.i_cont = true;
    this.i_broken = false;
    this.i_quote = false;
    this.cm_next = false;
    this.cm_disp_next = -1;
    this.need_redraw = false;

    c.wc_char_conv_init(
        c.wc_guess_8bit_charset(c.getRuntime().*.DisplayCharset),
        c.getRuntime().*.InnerCharset,
    );

    while (this.i_cont) {
        const x = c.calcPosition(
            this.strBuf.*.ptr,
            (&this.strProp).ptr,
            @intCast(this.CLen),
            @intCast(this.CPos),
            0,
            c.CP_FORCE,
        );
        if (x > this.offset + rpos) {
            const y = c.calcPosition(
                this.strBuf.*.ptr,
                (&this.strProp).ptr,
                @intCast(this.CLen),
                @intCast(this.CLen),
                0,
                c.CP_AUTO,
            );
            if (y - epos > x - rpos) {
                this.offset = x - rpos;
            } else if (y - epos > 0) {
                this.offset = y - epos;
            }
        } else if (x < this.offset + lpos) {
            if (x > lpos) {
                this.offset = x - lpos;
            } else {
                this.offset = 0;
            }
        }
        c.screen_move(c.LASTLINE(), 0);
        c.screen_wc_addstr(opts.prompt);
        var w: c.LineWriter = .{};
        if (this.is_passwd) {
            c.addPasswd(
                &w,
                this.strBuf.*.ptr,
                (&this.strProp).ptr,
                @intCast(this.CLen),
                @intCast(this.offset),
                c.TTY_COLS() - opos,
            );
        } else {
            c.addStr(
                &w,
                this.strBuf.*.ptr,
                (&this.strProp).ptr,
                @intCast(this.CLen),
                @intCast(this.offset),
                c.TTY_COLS() - opos,
            );
        }
        c.screen_clrtoeolx();
        c.screen_move(c.LASTLINE(), @as(usize, @intCast(opos)) + x - this.offset);
        c.tty_refresh();

        // next_char:
        while (true) {
            const ch: u8 = @intCast(c.getch());
            this.cm_clear = true;
            this.cm_disp_clear = true;
            if (!this.i_quote and //
                (((this.cm_mode.contains(.CPL_ALWAYS)) and (ch == c.CTRL_I or (c.getRuntime().*.space_autocomplete != 0 and ch == ' '))) or ((this.cm_mode.contains(.CPL_ON)) and (ch == c.CTRL_I))))
            {
                if (c.getRuntime().*.emacs_like_lineedit != 0 and this.cm_next) {
                    this._dcompl();
                    this.need_redraw = true;
                } else {
                    this._compl(ch);
                    this.cm_disp_next = -1;
                }
            } else if (!this.i_quote and this.CLen == this.CPos and (this.cm_mode.contains(.CPL_ALWAYS) or this.cm_mode.contains(.CPL_ON)) and ch == c.CTRL_D) {
                if (c.getRuntime().*.emacs_like_lineedit == 0) {
                    this._dcompl();
                    this.need_redraw = true;
                }
            } else if (!this.i_quote and ch == c.DEL_CODE) {
                this._bs(0);
                this.cm_next = false;
                this.cm_disp_next = -1;
            } else if (!this.i_quote and ch < 0x20) {
                // Control code
                if (opts.incrfunc) |incrfunc| {
                    const incr_ch = incrfunc(ch, this.strBuf, (&this.strProp).ptr);
                    if (incr_ch < 0x20) {
                        this.InputKeymap[@intCast(incr_ch)](this, @intCast(incr_ch));
                    }
                    if (incr_ch != -1 and incr_ch != c.CTRL_J) {
                        _ = incrfunc(-1, this.strBuf, (&this.strProp).ptr);
                    }
                } else {
                    this.InputKeymap[@intCast(ch)](this, ch);
                }
                if (this.cm_clear)
                    this.cm_next = false;
                if (this.cm_disp_clear)
                    this.cm_disp_next = -1;
            } else {
                const tmp = c.wc_char_conv(@intCast(ch));
                if (tmp == null) {
                    this.i_quote = true;
                    // goto next_char;
                    continue;
                }
                this.i_quote = false;
                this.cm_next = false;
                this.cm_disp_next = -1;
                if (this.CLen + tmp.*.length > STR_LEN or 0 == tmp.*.length) {
                    // goto next_char;
                    continue;
                }
                this.ins_char(tmp);
                if (opts.incrfunc) |incrfunc| {
                    _ = incrfunc(-1, this.strBuf, (&this.strProp).ptr);
                }
            }
            break;
        }
        if (this.CLen != 0 and (opts.flag & c.IN_CHAR) != 0)
            break;
    }

    if (this.i_broken)
        return null;

    c.screen_move(c.LASTLINE(), 0);
    c.tty_refresh();
    var p = this.strBuf.*.ptr;
    if (opts.flag & (c.IN_FILENAME | c.IN_COMMAND) != 0) {
        p = @constCast(c.skip_blanks(p));
    }
    if (this.use_hist and 0 == (opts.flag & c.IN_URL) and p[0] != 0) {
        const q = c.lastHist(opts.hist);
        if (q == null or !std.mem.eql(u8, std.mem.span(q), std.mem.span(p))) {
            _ = c.pushHist(opts.hist, p);
        }
    }
    if (opts.flag & c.IN_FILENAME != 0) {
        return c.expandPath(p);
    } else {
        return c.allocStr(p, -1);
    }

    return null;
}

fn setStrType(this: *@This(), str: c.Str, prop: []c.Lineprop) usize {
    var p = str.*.ptr;
    const ep = p + str.*.length;

    var i: usize = 0;
    while (@intFromPtr(p) < @intFromPtr(ep)) {
        var len = c.get_mclen(p);
        if (i + len > STR_LEN)
            break;
        var ctype = c.get_mctype(p);
        if (this.is_passwd) {
            if (ctype & c.PC_CTRL != 0)
                ctype = c.PC_ASCII;
            if (ctype & c.PC_UNKNOWN != 0)
                ctype = c.PC_WCHAR1;
        }
        prop[i] = ctype;
        i += 1;
        p += len;
        len -= 1;
        if (len != 0) {
            ctype = (ctype & ~c.PC_WCHAR1) | c.PC_WCHAR2;
            while (len != 0) : (len -= 1) {
                prop[i] = ctype;
                i += 1;
            }
        }
    }
    return i;
}

fn _bs(this: *@This(), _: u8) void {
    if (this.CPos > 0) {
        this._mvL(0);
        this.delC(0);
    }
}

fn delC(this: *@This(), _: u8) void {
    const i = this.CPos;
    var delta: usize = 1;

    if (this.CLen == this.CPos)
        return;
    while (i + delta < this.CLen and this.strProp[i + delta] & c.PC_WCHAR2 != 0)
        delta += 1;
    for (this.CPos..this.CLen) |j| {
        this.strProp[j] = this.strProp[j + delta];
    }
    c.Strdelete(this.strBuf, this.CPos, @intCast(delta));
    this.CLen -= delta;
}

fn _mvL(this: *@This(), _: u8) void {
    if (this.CPos > 0)
        this.CPos -= 1;
    while (this.CPos > 0 and this.strProp[this.CPos] & c.PC_WCHAR2 != 0)
        this.CPos -= 1;
}

fn _inbrk(this: *@This(), _: u8) void {
    this.i_cont = false;
    this.i_broken = true;
}

const iself = &insertself;

fn _enter(this: *@This(), _: u8) void {
    this.i_cont = false;
}

fn killn(this: *@This(), _: u8) void {
    this.CLen = this.CPos;
    c.Strtruncate(this.strBuf, @intCast(this.CLen));
}

fn _next(this: *@This(), _: u8) void {
    // struct Hist* hist = CurrentHist;
    // char* p;

    if (!this.use_hist)
        return;
    if (this.strCurrentBuf == null)
        return;
    const _p = c.nextHist(this.CurrentHist);
    if (_p) |x| {
        var p = x;
        if (c.getRuntime().*.DecodeURL != 0 and (this.cm_mode.contains(.CPL_URL)))
            p = c.url_decode2(p, null);
        this.strBuf = c.Strnew_charp(p);
    } else {
        //     strBuf = strCurrentBuf;
        //     strCurrentBuf = NULL;
    }
    // CLen = CPos = setStrType(strBuf, strProp);
    // offset = 0;
}

fn _editor(this: *@This(), _: u8) void {
    // char* p;

    if (this.is_passwd)
        return;

    // struct FormItemList fi;
    var fi = c.FormItemList{
        .readonly = c.FALSE,
        .value = c.Strdup(this.strBuf),
    };
    _ = c.Strcat_char(fi.value, '\n');
    c.input_textarea(&fi);

    this.strBuf = c.Strnew();
    var p = fi.value.*.ptr;
    while (p[0] != 0) : (p += 1) {
        if (p[0] == '\r' or p[0] == '\n')
            continue;
        _ = c.Strcat_char(this.strBuf, p[0]);
    }
    this.CPos = this.setStrType(this.strBuf, &this.strProp);
    this.CLen = this.CPos;
}

fn _prev(this: *@This(), _: u8) void {
    if (!this.use_hist)
        return;

    var p: [*c]const u8 = null;
    if (this.strCurrentBuf != null) {
        p = c.prevHist(this.CurrentHist);
        if (p == null)
            return;
    } else {
        p = c.lastHist(this.CurrentHist);
        if (p == null)
            return;
        this.strCurrentBuf = this.strBuf;
    }
    if (c.getRuntime().*.DecodeURL != 0 and (this.cm_mode.contains(.CPL_URL)))
        p = c.url_decode2(p, null);
    this.strBuf = c.Strnew_charp(p);
    this.CPos = this.setStrType(this.strBuf, &this.strProp);
    this.CLen = this.CPos;
    this.offset = 0;
}

fn terminated(ch: u8) bool {
    const termchar = [_]u8{ '/', '&', '?', ' ' };
    for (termchar) |t| {
        if (ch == t) {
            return true;
        }
    }
    return false;
}

fn _bsw(this: *@This(), _: u8) void {
    var t = false;
    while (this.CPos > 0 and !t) {
        this._mvL(0);
        t = this.move_word and terminated(this.strBuf.*.ptr[this.CPos - 1]);
        this.delC(0);
    }
}

fn _mvLw(this: *@This(), _: u8) void {
    var first = true;
    while (this.CPos > 0 and (first or !terminated(this.strBuf.*.ptr[this.CPos - 1]))) {
        this.CPos -= 1;
        first = false;
        if (this.CPos > 0 and this.strProp[this.CPos] & c.PC_WCHAR2 != 0)
            this.CPos -= 1;
        if (!this.move_word)
            break;
    }
}

fn _mvRw(this: *@This(), _: u8) void {
    var first = true;
    while (this.CPos < this.CLen and (first or !terminated(this.strBuf.*.ptr[this.CPos - 1]))) {
        this.CPos += 1;
        first = false;
        if (this.CPos < this.CLen and this.strProp[this.CPos] & c.PC_WCHAR2 != 0)
            this.CPos += 1;
        if (!this.move_word)
            break;
    }
}

fn _esc(this: *@This(), _: u8) void {
    const ch: u8 = @intCast(c.getch());
    switch (ch) {
        '[', 'O' => {
            const ch2 = c.getch();
            switch (ch2) {
                'A' => {
                    this._prev(0);
                },
                'B' => {
                    this._next(0);
                },
                'C' => {
                    this._mvR(0);
                },
                'D' => {
                    this._mvL(0);
                },
                else => {},
            }
        },
        c.CTRL_I, ' ' => {
            if (c.getRuntime().*.emacs_like_lineedit != 0) {
                this._rdcompl(0);
                this.cm_clear = false;
                this.need_redraw = true;
            } else {
                this._rcompl(0);
            }
        },
        c.CTRL_D => {
            if (0 == c.getRuntime().*.emacs_like_lineedit)
                this._rdcompl(0);
            this.need_redraw = true;
        },
        'f' => {
            if (c.getRuntime().*.emacs_like_lineedit != 0)
                this._mvRw(0);
        },
        'b' => {
            if (c.getRuntime().*.emacs_like_lineedit != 0)
                this._mvLw(0);
        },
        c.CTRL_H => {
            if (c.getRuntime().*.emacs_like_lineedit != 0)
                this._bsw(0);
        },
        else => {
            if (c.wc_char_conv(c.ESC_CODE) == null and c.wc_char_conv(ch) == null)
                this.i_quote = true;
        },
    }
}

fn _rcompl(this: *@This(), _: u8) void {
    this.next_compl(-1);
}

fn killb(this: *@This(), _: u8) void {
    while (this.CPos > 0) {
        this._bs(0);
    }
}

fn _quo(this: *@This(), _: u8) void {
    this.i_quote = true;
}

fn _tcompl(this: *@This(), _: u8) void {
    if (this.cm_mode.contains(.CPL_OFF)) {
        this.cm_mode = .initOne(.CPL_ON);
    } else if (this.cm_mode.contains(.CPL_ON)) {
        this.cm_mode = .initOne(.CPL_OFF);
    }
}

fn insertself(this: *@This(), ch: u8) void {
    if (this.CLen >= STR_LEN) {
        return;
    }
    this.insC();
    this.strBuf.*.ptr[this.CPos] = ch;
    this.strProp[this.CPos] = if (this.is_passwd) c.PC_ASCII else c.PC_CTRL;
    this.CPos += 1;
}

fn _mvE(this: *@This(), _: u8) void {
    this.CPos = this.CLen;
}

fn _mvR(this: *@This(), _: u8) void {
    if (this.CPos < this.CLen) {
        this.CPos += 1;
    }
    while (this.CPos < this.CLen and this.strProp[this.CPos] & c.PC_WCHAR2 != 0) {
        this.CPos += 1;
    }
}

fn ins_char(this: *@This(), str: c.Str) void {
    if (this.CLen + str.*.length >= STR_LEN)
        return;
    var p = str.*.ptr;
    const ep = p + str.*.length;
    while (@intFromPtr(p) < @intFromPtr(ep)) {
        var len = c.get_mclen(p);
        var ctype = c.get_mctype(p);
        if (this.is_passwd) {
            if (ctype & c.PC_CTRL != 0)
                ctype = c.PC_ASCII;
            if (ctype & c.PC_UNKNOWN != 0)
                ctype = c.PC_WCHAR1;
        }
        this.insC();
        this.strBuf.*.ptr[this.CPos] = p[0];
        p += 1;
        this.strProp[this.CPos] = ctype;
        this.CPos += 1;
        len -= 1;
        if (len != 0) {
            ctype = (ctype & ~c.PC_WCHAR1) | c.PC_WCHAR2;
            while (len != 0) : (len -= 0) {
                this.insC();
                this.strBuf.*.ptr[this.CPos] = p[0];
                p += 1;
                this.strProp[this.CPos] = ctype;
                this.CPos += 1;
            }
        }
    }
}

fn insC(this: *@This()) void {
    c.Strinsert_char(this.strBuf, @intCast(this.CPos), ' ');
    this.CLen = this.strBuf.*.length;
    var i = this.CLen;
    while (i > this.CPos) : (i -= 1) {
        this.strProp[i] = this.strProp[i - 1];
    }
}

//
// completion
//
fn _dcompl(this: *@This()) void {
    this.next_dcompl(1);
}

fn _compl(this: *@This(), _: u8) void {
    this.next_compl(1);
}

fn _mvB(this: *@This(), _: u8) void {
    this.CPos = 0;
}

fn next_dcompl(this: *@This(), next: c_int) void {
    _ = this;
    _ = next;
    //     static int col, row;
    //     static unsigned int len;
    //     static Str d;
    //     int i, j, n, y;
    //     Str f;
    //     char* p;
    //     struct stat st;
    //     int comment, nline;
    //
    //     if (cm_mode == CPL_NEVER || cm_mode & CPL_OFF)
    //         return;
    //     cm_disp_clear = FALSE;
    //     if (LASTLINE() >= 3) {
    //         comment = TRUE;
    //         nline = LASTLINE() - 2;
    //     } else if (LASTLINE()) {
    //         comment = FALSE;
    //         nline = LASTLINE();
    //     } else {
    //         return;
    //     }
    //
    //     if (cm_disp_next >= 0) {
    //         if (next == 1) {
    //             cm_disp_next += col * nline;
    //             if (cm_disp_next >= NCFileBuf)
    //                 cm_disp_next = 0;
    //         } else if (next == -1) {
    //             cm_disp_next -= col * nline;
    //             if (cm_disp_next < 0)
    //                 cm_disp_next = 0;
    //         }
    //         row = (NCFileBuf - cm_disp_next + col - 1) / col;
    //         goto disp_next;
    //     }
    //
    //     cm_next = FALSE;
    //     next_compl(0);
    //     if (NCFileBuf == 0)
    //         return;
    //     cm_disp_next = 0;
    //
    //     d = Str_conv_to_system(Strdup(CDirBuf));
    //     if (d.length > 0 && Strlastchar(d) != '/')
    //         Strcat_char(d, '/');
    //     if (cm_mode & CPL_URL && d.ptr[0] == 'f') {
    //         p = d.ptr;
    //         if (strncmp(p, "file://localhost/", 17) == 0)
    //             p = &p[16];
    //         else if (strncmp(p, "file:///", 8) == 0)
    //             p = &p[7];
    //         else if (strncmp(p, "file:/", 6) == 0 && p[6] != '/')
    //             p = &p[5];
    //         d = Strnew_charp(p);
    //     }
    //
    //     len = 0;
    //     for (i = 0; i < NCFileBuf; i++) {
    //         n = strlen(CFileBuf[i]) + 3;
    //         if (len < n)
    //             len = n;
    //     }
    //     if (len > 0 && TTY_COLS() > len)
    //         col = TTY_COLS() / len;
    //     else
    //         col = 1;
    //     row = (NCFileBuf + col - 1) / col;
    //
    // disp_next:
    //     if (comment) {
    //         if (row > nline) {
    //             row = nline;
    //             y = 0;
    //         } else
    //             y = nline - row + 1;
    //     } else {
    //         if (row >= nline) {
    //             row = nline;
    //             y = 0;
    //         } else
    //             y = nline - row - 1;
    //     }
    //     if (y) {
    //         screen_move(y - 1, 0);
    //         screen_clrtoeolx();
    //     }
    //     if (comment) {
    //         screen_move(y, 0);
    //         screen_clrtoeolx();
    //         screen_bold();
    //         /* FIXME: gettextize? */
    //         screen_wc_addstr("----- Completion list -----");
    //         screen_boldend();
    //         y++;
    //     }
    //     for (i = 0; i < row; i++) {
    //         for (j = 0; j < col; j++) {
    //             n = cm_disp_next + j * row + i;
    //             if (n >= NCFileBuf)
    //                 break;
    //             screen_move(y, j * len);
    //             screen_clrtoeolx();
    //             f = Strdup(d);
    //             Strcat_charp(f, CFileBuf[n]);
    //             screen_wc_addstr(conv_from_system(CFileBuf[n]));
    //             if (stat(expandPath(f.ptr), &st) != -1 && S_ISDIR(st.st_mode))
    //                 screen_wc_addstr("/");
    //         }
    //         y++;
    //     }
    //     if (comment && y == LASTLINE() - 1) {
    //         screen_move(y, 0);
    //         screen_clrtoeolx();
    //         screen_bold();
    //         if (getRuntime().emacs_like_lineedit)
    //             /* FIXME: gettextize? */
    //             screen_wc_addstr("----- Press TAB to continue -----");
    //         else
    //             /* FIXME: gettextize? */
    //             screen_wc_addstr("----- Press CTRL-D to continue -----");
    //         screen_boldend();
    //     }
}

fn next_compl(this: *@This(), next: c_int) void {
    _ = this;
    _ = next;
    //     int status;
    //     int b, a;
    //     Str buf;
    //     Str s;
    //
    //     if (cm_mode == CPL_NEVER || cm_mode & CPL_OFF)
    //         return;
    //     cm_clear = FALSE;
    //     if (!cm_next) {
    //         if (cm_mode & CPL_ALWAYS) {
    //             b = 0;
    //         } else {
    //             for (b = CPos - 1; b >= 0; b--) {
    //                 if ((strBuf.ptr[b] == ' ' || strBuf.ptr[b] == CTRL_I) && !((b > 0) && strBuf.ptr[b - 1] == '\\'))
    //                     break;
    //             }
    //             b++;
    //         }
    //         a = CPos;
    //         CBeforeBuf = Strsubstr(strBuf, 0, b);
    //         buf = Strsubstr(strBuf, b, a - b);
    //         CAfterBuf = Strsubstr(strBuf, a, strBuf.length - a);
    //         s = doComplete(buf, &status, next);
    //     } else {
    //         s = doComplete(strBuf, &status, next);
    //     }
    //     if (next == 0)
    //         return;
    //
    //     if (status != CPL_OK && status != CPL_MENU)
    //         bell();
    //     if (status == CPL_FAIL)
    //         return;
    //
    //     strBuf = Strnew_m_charp(CBeforeBuf.ptr, s.ptr, CAfterBuf.ptr, NULL);
    //     CLen = setStrType(strBuf, strProp);
    //     CPos = CBeforeBuf.length + s.length;
    //     if (CPos > CLen)
    //         CPos = CLen;
}

fn _rdcompl(this: *@This(), _: u8) void {
    this.next_dcompl(-1);
}

// /* Completion status. */
// #define CPL_OK 0
// #define CPL_AMBIG 1
// #define CPL_FAIL 2
// #define CPL_MENU 3

// // static Str
// // escape_spaces(Str s)
// // {
// //     Str tmp = NULL;
// //     char* p;
// //
// //     if (s == NULL)
// //         return s;
// //     for (p = s->ptr; *p; p++) {
// //         if (*p == ' ' || *p == CTRL_I) {
// //             if (tmp == NULL)
// //                 tmp = Strnew_charp_n(s->ptr, (int)(p - s->ptr));
// //             Strcat_char(tmp, '\\');
// //         }
// //         if (tmp)
// //             Strcat_char(tmp, *p);
// //     }
// //     if (tmp)
// //         return tmp;
// //     return s;
// // }

// // static Str
// // doComplete(Str ifn, int* status, int next)
// // {
// //     int fl, i;
// //     char *fn, *p;
// //     DIR* d;
// //     Directory* dir;
// //     struct stat st;
// //
// //     if (!cm_next) {
// //         NCFileBuf = 0;
// //         ifn = Str_conv_to_system(ifn);
// //         if (cm_mode & CPL_ON)
// //             ifn = unescape_spaces(ifn);
// //         CompleteBuf = Strdup(ifn);
// //         while (Strlastchar(CompleteBuf) != '/' && CompleteBuf->length > 0)
// //             Strshrink(CompleteBuf, 1);
// //         CDirBuf = Strdup(CompleteBuf);
// //         if (cm_mode & CPL_URL) {
// //             if (strncmp(CompleteBuf->ptr, "file://localhost/", 17) == 0)
// //                 Strdelete(CompleteBuf, 0, 16);
// //             else if (strncmp(CompleteBuf->ptr, "file:///", 8) == 0)
// //                 Strdelete(CompleteBuf, 0, 7);
// //             else if (strncmp(CompleteBuf->ptr, "file:/", 6) == 0 && CompleteBuf->ptr[6] != '/')
// //                 Strdelete(CompleteBuf, 0, 5);
// //             else {
// //                 CompleteBuf = Strdup(ifn);
// //                 *status = CPL_FAIL;
// //                 return Str_conv_to_system(CompleteBuf);
// //             }
// //         }
// //         if (CompleteBuf->length == 0) {
// //             Strcat_char(CompleteBuf, '.');
// //         }
// //         if (Strlastchar(CompleteBuf) == '/' && CompleteBuf->length > 1) {
// //             Strshrink(CompleteBuf, 1);
// //         }
// //         if ((d = opendir(expandPath(CompleteBuf->ptr))) == NULL) {
// //             CompleteBuf = Strdup(ifn);
// //             *status = CPL_FAIL;
// //             if (cm_mode & CPL_ON)
// //                 CompleteBuf = escape_spaces(CompleteBuf);
// //             return CompleteBuf;
// //         }
// //         fn = lastFileName(ifn->ptr);
// //         fl = strlen(fn);
// //         CFileName = Strnew();
// //         for (;;) {
// //             dir = readdir(d);
// //             if (dir == NULL)
// //                 break;
// //             if (fl == 0
// //                 && (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")))
// //                 continue;
// //             if (!strncmp(dir->d_name, fn, fl)) { /* match */
// //                 NCFileBuf++;
// //                 CFileBuf = New_Reuse(char*, CFileBuf, NCFileBuf);
// //                 CFileBuf[NCFileBuf - 1] = NewAtom_N(char, strlen(dir->d_name) + 1);
// //                 strcpy(CFileBuf[NCFileBuf - 1], dir->d_name);
// //                 if (NCFileBuf == 1) {
// //                     CFileName = Strnew_charp(dir->d_name);
// //                 } else {
// //                     for (i = 0; CFileName->ptr[i] == dir->d_name[i]; i++)
// //                         ;
// //                     Strtruncate(CFileName, i);
// //                 }
// //             }
// //         }
// //         closedir(d);
// //         if (NCFileBuf == 0) {
// //             CompleteBuf = Strdup(ifn);
// //             *status = CPL_FAIL;
// //             if (cm_mode & CPL_ON)
// //                 CompleteBuf = escape_spaces(CompleteBuf);
// //             return CompleteBuf;
// //         }
// //         qsort(CFileBuf, NCFileBuf, sizeof(CFileBuf[0]), strCmp);
// //         NCFileOffset = 0;
// //         if (NCFileBuf >= 2) {
// //             cm_next = TRUE;
// //             *status = CPL_AMBIG;
// //         } else {
// //             *status = CPL_OK;
// //         }
// //     } else {
// //         CFileName = Strnew_charp(CFileBuf[NCFileOffset]);
// //         NCFileOffset = (NCFileOffset + next + NCFileBuf) % NCFileBuf;
// //         *status = CPL_MENU;
// //     }
// //     CompleteBuf = Strdup(CDirBuf);
// //     if (CompleteBuf->length && Strlastchar(CompleteBuf) != '/')
// //         Strcat_char(CompleteBuf, '/');
// //     Strcat(CompleteBuf, CFileName);
// //     if (*status != CPL_AMBIG) {
// //         p = CompleteBuf->ptr;
// //         if (cm_mode & CPL_URL) {
// //             if (strncmp(p, "file://localhost/", 17) == 0)
// //                 p = &p[16];
// //             else if (strncmp(p, "file:///", 8) == 0)
// //                 p = &p[7];
// //             else if (strncmp(p, "file:/", 6) == 0 && p[6] != '/')
// //                 p = &p[5];
// //         }
// //         if (stat(expandPath(p), &st) != -1 && S_ISDIR(st.st_mode))
// //             Strcat_char(CompleteBuf, '/');
// //     }
// //     if (cm_mode & CPL_ON)
// //         CompleteBuf = escape_spaces(CompleteBuf);
// //     return Str_conv_from_system(CompleteBuf);
// // }


