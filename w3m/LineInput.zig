const std = @import("std");
const c = @import("c.zig").c;
const g = @import("global.zig");
const history = @import("history.zig");

const STR_LEN = 1024;

const CompletionFlags = enum {
    CPL_OFF,
    CPL_ON,
    CPL_ALWAYS,
    CPL_URL,
};

const CompletionStatus = enum {
    CPL_OK,
    CPL_AMBIG,
    CPL_FAIL,
    CPL_MENU,
};

allocator: std.mem.Allocator,

use_hist: bool = false,
CurrentHist: c.HistoryType = c.HistoryNone,

is_passwd: bool = false,
move_word: bool = true,

strBuf: c.Str = null,
strProp: [STR_LEN]c.Lineprop = undefined,
CLen: usize = 0,
CPos: usize = 0,
offset: usize = 0,

strCurrentBuf: c.Str = null,
//     Str CBeforeBuf;
//     Str CAfterBuf;
//     int NCFileBuf;
//     Str CompleteBuf;
//     Str CDirBuf;
//     Str CFileName;
//     char** CFileBuf; // = NULL;
//     int NCFileOffset;

need_redraw: bool = false,
i_broken: bool = false,
i_cont: bool = true,
i_quote: bool = false,
cm_next: bool = false,
cm_clear: bool = true,
cm_disp_next: ?usize = null,
cm_disp_clear: bool = true,
cm_mode: std.enums.EnumFieldStruct(CompletionFlags, bool, false) = .{},

const InputFunc = fn (li: *@This(), args: *c.CmdArgs) c_int;

const InputKeymap: []const *const InputFunc = &.{
    //  C-@     C-a     C-b     C-c     C-d     C-e     C-f     C-g
    &_compl,  &_mvB,  &_mvL,   &_inbrk, &delC,  &_mvE,   &_mvR,  &_inbrk,
    //  C-h     C-i     C-j     C-k     C-l     C-m     C-n     C-o
    &_bs,     &iself, &_enter, &killn,  &iself, &_enter, &_next, &_editor,
    //  C-p     C-q     C-r     C-s     C-t     C-u     C-v     C-w
    &_prev,   &_quo,  &_bsw,   &iself,  &_mvLw, &killb,  &_quo,  &_bsw,
    //  C-x     C-y     C-z     C-[     C-\     C-]     C-^     C-_
    &_tcompl, &_mvRw, &iself,  &_esc,   &iself, &iself,  &iself, &iself,
};

pub fn init(allocator: std.mem.Allocator, def_str: []const u8, flag: c.InputLineFlags, hist: c.HistoryType) !@This() {
    var li: @This() = .{
        .allocator = allocator,
    };
    if (def_str.len > 0) {
        li.strBuf = c.Strnew_charp_n(def_str.ptr, @intCast(def_str.len));
        li.setStrType();
    } else {
        li.strBuf = c.Strnew();
    }

    li.CurrentHist = hist;
    if (hist != c.HistoryNone) {
        li.use_hist = true;
        // li.strCurrentBuf = NULL;
    } else {
        li.use_hist = false;
    }

    if (flag & c.IN_URL != 0) {
        li.cm_mode.CPL_ALWAYS = true;
        li.cm_mode.CPL_URL = true;
    } else if (flag & c.IN_FILENAME != 0) {
        li.cm_mode.CPL_ALWAYS = true;
    } else if (flag & c.IN_PASSWORD != 0) {
        // li.cm_mode = .CPL_NEVER;
        li.is_passwd = true;
        li.move_word = false;
    } else if (flag & c.IN_COMMAND != 0) {
        li.cm_mode.CPL_ON = true;
    } else {
        li.cm_mode.CPL_OFF = true;
    }

    return li;
}

pub fn deinit(this: *@This()) void {
    _ = this;
    // this.strBuf.deinit(this.allocator);
}

fn ins_char(this: *@This(), args: *c.CmdArgs, str: c.Str) void {
    if (this.CLen + @as(usize, @intCast(str.*.length)) >= STR_LEN)
        return;
    var pos: usize = 0;
    while (pos < str.*.length) {
        const p = &str.*.ptr[pos];
        var len = c.get_mclen(p);
        var ctype = c.get_mctype(p);
        if (this.is_passwd) {
            if (ctype & c.PC_CTRL != 0) {
                ctype = c.PC_ASCII;
            }
            if (ctype & c.PC_UNKNOWN != 0) {
                ctype = c.PC_WCHAR1;
            }
        }
        _ = this.insC(args);
        this.strBuf.*.ptr[this.CPos] = str.*.ptr[pos];
        pos += 1;
        this.strProp[this.CPos] = ctype;
        this.CPos += 1;
        len -= 1;
        if (len > 0) {
            ctype = @intCast((ctype & ~c.PC_WCHAR1) | c.PC_WCHAR2);
            while (len > 0) : (len -= 0) {
                _ = this.insC(args);
                this.strBuf.*.ptr[this.CPos] = str.*.ptr[pos];
                pos += 1;
                this.strProp[this.CPos] = ctype;
                this.CPos += 1;
            }
        }
    }
}

fn setStrType(this: *@This()) void {
    var i: usize = 0;
    var pos: usize = 0;
    while (pos < this.strBuf.*.length) {
        const p = &this.strBuf.*.ptr[pos];
        var len = c.get_mclen(p);
        if (i + len > STR_LEN)
            break;
        var ctype: c.Lineprop = c.get_mctype(p);
        if (this.is_passwd) {
            if (ctype & c.PC_CTRL != 0)
                ctype = c.PC_ASCII;
            if (ctype & c.PC_UNKNOWN != 0)
                ctype = c.PC_WCHAR1;
        }
        this.strProp[i] = ctype;
        i += 1;
        pos += len;
        len -= 1;
        if (len != 0) {
            ctype = @intCast((ctype & ~c.PC_WCHAR1) | c.PC_WCHAR2);
            while (len > 0) {
                len -= 1;
                this.strProp[i] = ctype;
                i += 1;
            }
        }
    }
    this.CLen = i;
    this.CPos = i;
}

// static Str
// escape_spaces(Str s)
// {
//     Str tmp = NULL;
//     char* p;
//
//     if (s == NULL)
//         return s;
//     for (p = s->ptr; *p; p++) {
//         if (*p == ' ' || *p == CTRL_I) {
//             if (tmp == NULL)
//                 tmp = Strnew_charp_n(s->ptr, (int)(p - s->ptr));
//             Strcat_char(tmp, '\\');
//         }
//         if (tmp)
//             Strcat_char(tmp, *p);
//     }
//     if (tmp)
//         return tmp;
//     return s;
// }
// static Str
// doComplete(struct LineInput* li, Str ifn, enum CompletionStatus* status, int next)
// {
//     int fl, i;
//     char *fn, *p;
//     DIR* d;
//     Directory* dir;
//     struct stat st;
//
//     if (!this.cm_next) {
//         this.NCFileBuf = 0;
//         ifn = Str_conv_to_system(ifn->ptr, ifn->length);
//         if (this.cm_mode & CPL_ON)
//             ifn = unescape_spaces(ifn);
//         this.CompleteBuf = Strdup(ifn);
//         while (Strlastchar(this.CompleteBuf) != '/' && this.CompleteBuf->length > 0)
//             Strshrink(this.CompleteBuf, 1);
//         this.CDirBuf = Strdup(this.CompleteBuf);
//         if (this.cm_mode & CPL_URL) {
//             if (strncmp(this.CompleteBuf->ptr, "file://localhost/", 17) == 0)
//                 Strdelete(this.CompleteBuf, 0, 16);
//             else if (strncmp(this.CompleteBuf->ptr, "file:///", 8) == 0)
//                 Strdelete(this.CompleteBuf, 0, 7);
//             else if (strncmp(this.CompleteBuf->ptr, "file:/", 6) == 0 && this.CompleteBuf->ptr[6] != '/')
//                 Strdelete(this.CompleteBuf, 0, 5);
//             else {
//                 this.CompleteBuf = Strdup(ifn);
//                 *status = CPL_FAIL;
//                 return Str_conv_to_system(this.CompleteBuf->ptr, this.CompleteBuf->length);
//             }
//         }
//         if (this.CompleteBuf->length == 0) {
//             Strcat_char(this.CompleteBuf, '.');
//         }
//         if (Strlastchar(this.CompleteBuf) == '/' && this.CompleteBuf->length > 1) {
//             Strshrink(this.CompleteBuf, 1);
//         }
//         if ((d = opendir(expandPath(this.CompleteBuf->ptr))) == NULL) {
//             this.CompleteBuf = Strdup(ifn);
//             *status = CPL_FAIL;
//             if (this.cm_mode & CPL_ON)
//                 this.CompleteBuf = escape_spaces(this.CompleteBuf);
//             return this.CompleteBuf;
//         }
//         fn = lastFileName(ifn->ptr);
//         fl = strlen(fn);
//         this.CFileName = Strnew();
//         for (;;) {
//             dir = readdir(d);
//             if (dir == NULL)
//                 break;
//             if (fl == 0
//                 && (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")))
//                 continue;
//             if (!strncmp(dir->d_name, fn, fl)) { /* match */
//                 this.NCFileBuf++;
//                 this.CFileBuf = New_Reuse(char*, this.CFileBuf, this.NCFileBuf);
//                 this.CFileBuf[this.NCFileBuf - 1] = NewAtom_N(char, strlen(dir->d_name) + 1);
//                 strcpy(this.CFileBuf[this.NCFileBuf - 1], dir->d_name);
//                 if (this.NCFileBuf == 1) {
//                     this.CFileName = Strnew_charp(dir->d_name);
//                 } else {
//                     for (i = 0; this.CFileName->ptr[i] == dir->d_name[i]; i++)
//                         ;
//                     Strtruncate(this.CFileName, i);
//                 }
//             }
//         }
//         closedir(d);
//         if (this.NCFileBuf == 0) {
//             this.CompleteBuf = Strdup(ifn);
//             *status = CPL_FAIL;
//             if (this.cm_mode & CPL_ON)
//                 this.CompleteBuf = escape_spaces(this.CompleteBuf);
//             return this.CompleteBuf;
//         }
//         qsort(this.CFileBuf, this.NCFileBuf, sizeof(this.CFileBuf[0]), strCmp);
//         this.NCFileOffset = 0;
//         if (this.NCFileBuf >= 2) {
//             this.cm_next = true;
//             *status = CPL_AMBIG;
//         } else {
//             *status = CPL_OK;
//         }
//     } else {
//         this.CFileName = Strnew_charp(this.CFileBuf[this.NCFileOffset]);
//         this.NCFileOffset = (this.NCFileOffset + next + this.NCFileBuf) % this.NCFileBuf;
//         *status = CPL_MENU;
//     }
//     this.CompleteBuf = Strdup(this.CDirBuf);
//     if (this.CompleteBuf->length && Strlastchar(this.CompleteBuf) != '/')
//         Strcat_char(this.CompleteBuf, '/');
//     Strcat(this.CompleteBuf, this.CFileName);
//     if (*status != CPL_AMBIG) {
//         p = this.CompleteBuf->ptr;
//         if (this.cm_mode & CPL_URL) {
//             if (strncmp(p, "file://localhost/", 17) == 0)
//                 p = &p[16];
//             else if (strncmp(p, "file:///", 8) == 0)
//                 p = &p[7];
//             else if (strncmp(p, "file:/", 6) == 0 && p[6] != '/')
//                 p = &p[5];
//         }
//         if (stat(expandPath(p), &st) != -1 && S_ISDIR(st.st_mode))
//             Strcat_char(this.CompleteBuf, '/');
//     }
//     if (this.cm_mode & CPL_ON)
//         this.CompleteBuf = escape_spaces(this.CompleteBuf);
//     return Str_conv_from_system(this.CompleteBuf->ptr, this.CompleteBuf->length);
// }

fn next_compl(this: *@This(), next: c_int) void {
    _ = this;
    _ = next;
    //     enum CompletionStatus status;
    //     int b, a;
    //     Str buf;
    //     Str s;
    //
    //     if (this.cm_mode == CPL_NEVER || this.cm_mode & CPL_OFF)
    //         return;
    //     this.cm_clear = false;
    //     if (!this.cm_next) {
    //         if (this.cm_mode & CPL_ALWAYS) {
    //             b = 0;
    //         } else {
    //             for (b = this.CPos - 1; b >= 0; b--) {
    //                 if ((this.strBuf->ptr[b] == ' ' || this.strBuf->ptr[b] == CTRL_I) && !((b > 0) && this.strBuf->ptr[b - 1] == '\\'))
    //                     break;
    //             }
    //             b++;
    //         }
    //         a = this.CPos;
    //         this.CBeforeBuf = Strsubstr(this.strBuf, 0, b);
    //         buf = Strsubstr(this.strBuf, b, a - b);
    //         this.CAfterBuf = Strsubstr(this.strBuf, a, this.strBuf->length - a);
    //         s = doComplete(li, buf, &status, next);
    //     } else {
    //         s = doComplete(li, this.strBuf, &status, next);
    //     }
    //     if (next == 0)
    //         return;
    //
    //     if (status != CPL_OK && status != CPL_MENU)
    //         tty_bell();
    //     if (status == CPL_FAIL)
    //         return;
    //
    //     this.strBuf = Strnew_m_charp(this.CBeforeBuf->ptr, s->ptr, this.CAfterBuf->ptr, NULL);
    //     setStrType(li);
    //     this.CPos = this.CBeforeBuf->length + s->length;
    //     if (this.CPos > this.CLen)
    //         this.CPos = this.CLen;
}

fn next_dcompl(this: *@This(), _: *c.CmdArgs, next: c_int) void {
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
    //     if (this.cm_mode == CPL_NEVER || this.cm_mode & CPL_OFF)
    //         return;
    //     this.cm_disp_clear = false;
    //     if (CurrentTab)
    //         displayBuffer(args, B_FORCE_REDRAW);
    //     if ((LINES - 1) >= 3) {
    //         comment = true;
    //         nline = (LINES - 1) - 2;
    //     } else if ((LINES - 1)) {
    //         comment = false;
    //         nline = (LINES - 1);
    //     } else {
    //         return;
    //     }
    //
    //     if (this.cm_disp_next >= 0) {
    //         if (next == 1) {
    //             this.cm_disp_next += col * nline;
    //             if (this.cm_disp_next >= this.NCFileBuf)
    //                 this.cm_disp_next = 0;
    //         } else if (next == -1) {
    //             this.cm_disp_next -= col * nline;
    //             if (this.cm_disp_next < 0)
    //                 this.cm_disp_next = 0;
    //         }
    //         row = (this.NCFileBuf - this.cm_disp_next + col - 1) / col;
    //         goto disp_next;
    //     }
    //
    //     this.cm_next = false;
    //     next_compl(li, 0);
    //     if (this.NCFileBuf == 0)
    //         return;
    //     this.cm_disp_next = 0;
    //
    //     d = Str_conv_to_system(this.CDirBuf->ptr, this.CDirBuf->length);
    //     if (d->length > 0 && Strlastchar(d) != '/')
    //         Strcat_char(d, '/');
    //     if (this.cm_mode & CPL_URL && d->ptr[0] == 'f') {
    //         p = d->ptr;
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
    //     for (i = 0; i < this.NCFileBuf; i++) {
    //         n = strlen(this.CFileBuf[i]) + 3;
    //         if (len < n)
    //             len = n;
    //     }
    //     if (len > 0 && COLS > len)
    //         col = COLS / len;
    //     else
    //         col = 1;
    //     row = (this.NCFileBuf + col - 1) / col;
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
    //         move(y - 1, 0);
    //         sc_clrtoeolx();
    //     }
    //     if (comment) {
    //         move(y, 0);
    //         sc_clrtoeolx();
    //         bold();
    //         /* FIXME: gettextize? */
    //         addstr("----- Completion list -----");
    //         boldend();
    //         y++;
    //     }
    //     for (i = 0; i < row; i++) {
    //         for (j = 0; j < col; j++) {
    //             n = this.cm_disp_next + j * row + i;
    //             if (n >= this.NCFileBuf)
    //                 break;
    //             move(y, j * len);
    //             sc_clrtoeolx();
    //             f = Strdup(d);
    //             Strcat_charp(f, this.CFileBuf[n]);
    //             addstr(conv_from_system(this.CFileBuf[n]));
    //             if (stat(expandPath(f->ptr), &st) != -1 && S_ISDIR(st.st_mode))
    //                 addstr("/");
    //         }
    //         y++;
    //     }
    //     if (comment && y == (LINES - 1) - 1) {
    //         move(y, 0);
    //         sc_clrtoeolx();
    //         bold();
    //         if (emacs_like_lineedit)
    //             /* FIXME: gettextize? */
    //             addstr("----- Press TAB to continue -----");
    //         else
    //             /* FIXME: gettextize? */
    //             addstr("----- Press CTRL-D to continue -----");
    //         boldend();
    //     }
}

fn iself(this: *@This(), args: *c.CmdArgs) c_int {
    if (this.CLen >= STR_LEN)
        return 0;
    _ = this.insC(args);
    this.strBuf.*.ptr[this.CPos] = @intCast(args.ch);
    this.strProp[this.CPos] = if (this.is_passwd) c.PC_ASCII else c.PC_CTRL;
    this.CPos += 1;
    return 0;
}

fn _mvR(this: *@This(), _: *c.CmdArgs) c_int {
    if (this.CPos < this.CLen) {
        this.CPos += 1;
    }
    while (this.CPos < this.CLen and (this.strProp[this.CPos] & c.PC_WCHAR2 != 0)) {
        this.CPos += 1;
    }
    return 0;
}

fn _mvL(this: *@This(), _: *c.CmdArgs) c_int {
    if (this.CPos > 0) {
        this.CPos -= 1;
    }
    while (this.CPos > 0 and (this.strProp[this.CPos] & c.PC_WCHAR2 != 0)) {
        this.CPos -= 1;
    }
    return 0;
}

fn terminated(ch: u8) bool {
    const termchar: []const u8 = &.{ '/', '&', '?', ' ' };
    for (termchar) |tp| {
        if (ch == tp) {
            return true;
        }
    }
    return false;
}

fn _mvRw(this: *@This(), _: *c.CmdArgs) c_int {
    var first = true;
    while (this.CPos < this.CLen and (first or !terminated(this.strBuf.*.ptr[this.CPos - 1]))) {
        this.CPos += 1;
        first = false;
        if (this.CPos < this.CLen and (this.strProp[this.CPos] & c.PC_WCHAR2 != 0)) {
            this.CPos += 1;
        }
        if (!this.move_word)
            break;
    }
    return 0;
}

fn _mvLw(this: *@This(), _: *c.CmdArgs) c_int {
    var first = true;
    while (this.CPos > 0 and (first or !terminated(this.strBuf.*.ptr[this.CPos - 1]))) {
        this.CPos -= 1;
        first = false;
        if (this.CPos > 0 and (this.strProp[this.CPos] & c.PC_WCHAR2 != 0)) {
            this.CPos -= 1;
        }
        if (!this.move_word)
            break;
    }
    return 0;
}

fn delC(this: *@This(), _: *c.CmdArgs) c_int {
    if (this.CLen == this.CPos)
        return 0;

    var delta: usize = 1;
    {
        const i = this.CPos;
        while (i + delta < this.CLen and (this.strProp[i + delta] & c.PC_WCHAR2 != 0)) {
            delta += 1;
        }
    }
    for (this.CPos..this.CLen) |i| {
        this.strProp[i] = this.strProp[i + delta];
    }
    c.Strdelete(this.strBuf, @intCast(this.CPos), @intCast(delta));
    this.CLen -= delta;
    return 0;
}

fn insC(this: *@This(), _: *c.CmdArgs) c_int {
    c.Strinsert_char(this.strBuf, @intCast(this.CPos), ' ');
    this.CLen = @intCast(this.strBuf.*.length);
    var i = this.CLen;
    while (i > this.CPos) : (i -= 1) {
        this.strProp[i] = this.strProp[i - 1];
    }
    return 0;
}

fn _mvB(this: *@This(), _: *c.CmdArgs) c_int {
    this.CPos = 0;
    return 0;
}

fn _mvE(this: *@This(), _: *c.CmdArgs) c_int {
    this.CPos = this.CLen;
    return 0;
}

fn _enter(this: *@This(), _: *c.CmdArgs) c_int {
    this.i_cont = false;
    return 0;
}

fn _quo(this: *@This(), _: *c.CmdArgs) c_int {
    this.i_quote = true;
    return 0;
}

fn _bs(this: *@This(), args: *c.CmdArgs) c_int {
    if (this.CPos > 0) {
        _ = this._mvL(args);
        _ = this.delC(args);
    }
    return 0;
}

fn _bsw(this: *@This(), args: *c.CmdArgs) c_int {
    var t = false;
    while (this.CPos > 0 and !t) {
        _ = this._mvL(args);
        t = (this.move_word and terminated(this.strBuf.*.ptr[this.CPos - 1]));
        _ = this.delC(args);
    }
    return 0;
}

fn killn(this: *@This(), _: *c.CmdArgs) c_int {
    this.CLen = this.CPos;
    c.Strtruncate(this.strBuf, @intCast(this.CLen));
    return 0;
}

fn killb(this: *@This(), args: *c.CmdArgs) c_int {
    while (this.CPos > 0) {
        _ = this._bs(args);
    }
    return 0;
}

fn _inbrk(this: *@This(), _: *c.CmdArgs) c_int {
    this.i_cont = false;
    this.i_broken = true;
    return 0;
}

fn _esc(this: *@This(), args: *c.CmdArgs) c_int {
    args.ch = c.getch(args);
    switch (args.ch) {
        '[', 'O' => {
            args.ch = c.getch(args);
            switch (args.ch) {
                'A' => {
                    _ = this._prev(args);
                },
                'B' => {
                    _ = this._next(args);
                },
                'C' => {
                    _ = this._mvR(args);
                },
                'D' => {
                    _ = this._mvL(args);
                },
                else => unreachable,
            }
        },
        c.CTRL_I, ' ' => {
            if (g.emacs_like_lineedit != 0) {
                _ = this._rdcompl(args);
                this.cm_clear = false;
                this.need_redraw = true;
            } else {
                _ = this._rcompl(args);
            }
        },
        c.CTRL_D => {
            if (0 == g.emacs_like_lineedit) {
                _ = this._rdcompl(args);
            }
            this.need_redraw = true;
        },
        'f' => {
            if (g.emacs_like_lineedit != 0) {
                _ = this._mvRw(args);
            }
        },
        'b' => {
            if (g.emacs_like_lineedit != 0) {
                _ = this._mvLw(args);
            }
        },
        c.CTRL_H => {
            if (g.emacs_like_lineedit != 0) {
                _ = this._bsw(args);
            }
        },
        else => {
            if (c.wc_char_conv(c.WcOption, c.ESC_CODE).data == null and
                c.wc_char_conv(c.WcOption, @intCast(args.ch)).data == null)
            {
                this.i_quote = true;
            }
        },
    }
    return 0;
}

fn _editor(this: *@This(), args: *c.CmdArgs) c_int {
    if (this.is_passwd)
        return 0;

    var fi: c.FormItem = .{
        .readonly = 0,
        .value = c.Strdup(this.strBuf),
    };
    c.Strcat_char(fi.value, '\n');

    c.input_textarea(args, &fi);

    this.strBuf = c.Strnew();
    var p = fi.value.*.ptr;
    while (p[0] != 0) : (p += 1) {
        if (p[0] == '\r' or p[0] == '\n')
            continue;
        c.Strcat_char(this.strBuf, p[0]);
    }
    this.setStrType();
    // if (CurrentTab)
    //     displayBuffer(args, B_FORCE_REDRAW);
    return 0;
}

fn _prev(this: *@This(), _: *c.CmdArgs) c_int {
    const hist = this.CurrentHist;
    if (!this.use_hist)
        return 0;
    var p: ?[*]const u8 = undefined;
    if (this.strCurrentBuf != null) {
        p = history.prevHist(hist);
        if (p == null)
            return 0;
    } else {
        p = history.lastHist(hist);
        if (p == null)
            return 0;
        this.strCurrentBuf = this.strBuf;
    }
    if (g.DecodeURL != 0 and this.cm_mode.CPL_URL) {
        p = c.url_decode2(p, null);
    }
    this.strBuf = c.Strnew_charp(p);
    this.setStrType();
    this.offset = 0;
    return 0;
}

fn _next(this: *@This(), _: *c.CmdArgs) c_int {
    const hist = this.CurrentHist;

    if (!this.use_hist)
        return 0;

    if (this.strCurrentBuf == null)
        return 0;

    var p = history.nextHist(hist);
    if (p != null) {
        if (g.DecodeURL != 0 and this.cm_mode.CPL_URL) {
            p = c.url_decode2(p, null);
        }
        this.strBuf = c.Strnew_charp(p);
    } else {
        this.strBuf = this.strCurrentBuf;
        this.strCurrentBuf = null;
    }
    this.setStrType();
    this.offset = 0;
    return 0;
}

fn _compl(li: *@This(), _: *c.CmdArgs) c_int {
    li.next_compl(1);
    return 0;
}

fn _tcompl(this: *@This(), _: *c.CmdArgs) c_int {
    if (this.cm_mode.CPL_OFF) {
        this.cm_mode = .{ .CPL_ON = true };
    } else if (this.cm_mode.CPL_ON) {
        this.cm_mode = .{ .CPL_OFF = true };
    }
    return 0;
}

fn _dcompl(this: *@This(), args: *c.CmdArgs) c_int {
    this.next_dcompl(args, 1);
    return 0;
}

fn _rdcompl(this: *@This(), args: *c.CmdArgs) c_int {
    this.next_dcompl(args, -1);
    return 0;
}

fn _rcompl(this: *@This(), _: *c.CmdArgs) c_int {
    this.next_compl(-1);
    return 0;
}

pub fn process(
    this: *@This(),
    args: *c.CmdArgs,
    prompt: []const u8,
    flag: c.InputLineFlags,
    _incrfunc: c.IncrFunc,
) void {
    const opos = c.get_strwidth(c.WcOption, prompt.ptr);
    const epos = if (g.COLS >= 2 + opos)
        g.COLS - 2 - opos
    else
        0;
    const lpos = @divTrunc(epos, 3);
    const rpos = @divTrunc(epos * 2, 3);

    c.wc_char_conv_init(c.wc_guess_8bit_charset(c.DisplayCharset), c.InnerCharset);
    while (this.i_cont) {
        const x = c.calcPosition(this.strBuf.*.ptr, &this.strProp, @intCast(this.CLen), @intCast(this.CPos), 0, c.CP_FORCE);
        if (x - rpos > this.offset) {
            const y = c.calcPosition(this.strBuf.*.ptr, &this.strProp, @intCast(this.CLen), @intCast(this.CLen), 0, c.CP_AUTO);
            if (y - epos > x - rpos) {
                this.offset = @intCast(x - rpos);
            } else if (y - epos > 0) {
                this.offset = @intCast(y - epos);
            }
        } else if (x - lpos < this.offset) {
            if (x - lpos > 0) {
                this.offset = @intCast(x - lpos);
            } else {
                this.offset = 0;
            }
        }
        c.sc_move(@intCast(g.LINES - 1), 0);
        c.sc_addstr(prompt.ptr);
        if (this.is_passwd) {
            c.addPasswd(this.strBuf.*.ptr, &this.strProp, @intCast(this.CLen), @intCast(this.offset), g.COLS - opos);
        } else {
            c.addStr(this.strBuf.*.ptr, &this.strProp, @intCast(this.CLen), @intCast(this.offset), g.COLS - opos);
        }
        c.sc_clrtoeolx();
        c.sc_move(@intCast(g.LINES - 1), @intCast(opos + x - @as(c_int, @intCast(this.offset))));
        c.tty_write_sc();

        while (true) {
            // next_char:
            args.ch = c.getch(args);
            this.cm_clear = true;
            this.cm_disp_clear = true;
            if (!this.i_quote and (((this.cm_mode.CPL_ALWAYS) and (args.ch == c.CTRL_I or (g.space_autocomplete != 0 and args.ch == ' '))) or ((this.cm_mode.CPL_ON) and (args.ch == c.CTRL_I)))) {
                if (g.emacs_like_lineedit != 0 and this.cm_next) {
                    _ = this._dcompl(args);
                    this.need_redraw = true;
                } else {
                    _ = this._compl(args);
                    this.cm_disp_next = null;
                }
            } else if (!this.i_quote and this.CLen == this.CPos and (this.cm_mode.CPL_ALWAYS or this.cm_mode.CPL_ON) and args.ch == c.CTRL_D) {
                if (0 == g.emacs_like_lineedit) {
                    _ = this._dcompl(args);
                    this.need_redraw = true;
                }
            } else if (!this.i_quote and args.ch == c.DEL_CODE) {
                // _bs(args, &li);
                this.cm_next = false;
                this.cm_disp_next = null;
            } else if (!this.i_quote and args.ch < 0x20) { // Control code
                if (_incrfunc) |incrfunc| {
                    args.ch = incrfunc(args, this.strBuf.*.ptr, &this.strProp);
                    if (args.ch < 0x20) {
                        _ = (InputKeymap[@intCast(args.ch)])(this, args);
                    }
                } else {
                    _ = (InputKeymap[@intCast(args.ch)])(this, args);
                }
                if (_incrfunc) |incrfunc| {
                    if (args.ch != -1 and args.ch != c.CTRL_J) {
                        _ = incrfunc(args, this.strBuf.*.ptr, &this.strProp);
                    }
                }
                if (this.cm_clear)
                    this.cm_next = false;
                if (this.cm_disp_clear)
                    this.cm_disp_next = null;
            } else {
                const tmp = c.Strnew_wc_output(c.wc_char_conv(c.WcOption, @intCast(args.ch)));
                if (tmp == null) {
                    this.i_quote = true;
                    continue;
                }
                this.i_quote = false;
                this.cm_next = false;
                this.cm_disp_next = null;
                if (@as(c_int, @intCast(this.CLen)) + tmp.*.length > STR_LEN or 0 == tmp.*.length) {
                    continue;
                }
                this.ins_char(args, tmp);
                if (_incrfunc) |incrfunc| {
                    _ = incrfunc(args, this.strBuf.*.ptr, &this.strProp);
                }
            }
            break;
        }
        if (this.CLen != 0 and (flag & c.IN_CHAR != 0))
            break;
    }
}
