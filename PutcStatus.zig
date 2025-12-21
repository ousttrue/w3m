const std = @import("std");
const c = @cImport({
    @cInclude("libwc/status.h");
    @cInclude("libwc/ces.h");
    @cInclude("libwc/conv.h");
    @cInclude("libwc/wtf.h");
});

putc_st: c.wc_status = undefined,
putc_f_ces: c.wc_ces,
putc_t_ces: c.wc_ces,
putc_str: c.Str,

pub fn init(f_ces: c.wc_ces, t_ces: c.wc_ces) @This() {
    var this = @This(){
        .putc_str = c.Strnew_size(8),
        .putc_f_ces = f_ces,
        .putc_t_ces = t_ces,
    };
    c.wc_output_init(t_ces, &this.putc_st);
    return this;
}

pub fn putc(this: *@This(), ch: [*c]u8, fd: c_int) void {
    var p: [*c]u8 = undefined;
    if (this.putc_f_ces != c.WC_CES_WTF) {
        p = c.wc_conv(ch, this.putc_f_ces, c.WC_CES_WTF).*.ptr;
    } else {
        p = ch;
    }

    c.Strclear(this.putc_str);
    while (p[0] != 0) {
        this.putc_st.ces_info.*.push_to.?(this.putc_str, c.wtf_parse(&p), &this.putc_st);
    }
    _ = std.c.write(fd, this.putc_str.*.ptr, this.putc_str.*.length);
}

pub fn end(this: *@This(), fd: c_int) void {
    c.Strclear(this.putc_str);
    c.wc_push_end(this.putc_str, &this.putc_st);
    if (this.putc_str.*.length > 0) {
        _ = std.c.write(fd, this.putc_str.*.ptr, this.putc_str.*.length);
    }
}

pub fn clear(this: *@This()) void {
    if (this.putc_st.ces_info.*.id & c.WC_CES_T_ISO_2022 != 0) {
        this.putc_st.gl = 0;
        this.putc_st.gr = 0;
        this.putc_st.ss = 0;
        this.putc_st.design[0] = 0;
        this.putc_st.design[1] = 0;
        this.putc_st.design[2] = 0;
        this.putc_st.design[3] = 0;
    }
}
