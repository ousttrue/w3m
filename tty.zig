const uv = @import("uv");

export fn tty_open() void {}

export fn tty_close() void {}

export fn tty_cbreak() void {}
export fn tty_crmode() void {}
export fn tty_nocrmode() void {}
export fn tty_echo() void {}
export fn tty_noecho() void {}
export fn tty_raw() void {}
export fn tty_cooked() void {}

export fn tty_flush() void {}
export fn tty_putc(c: c_int) c_int {
    _ = c;
    return 0;
}
export fn tty_printf(fmt: [*:0]const u8, ...) void {
    _ = fmt;
}

export fn tty_getch() c_char {
    return 0;
}
export fn tty_sleep_till_anykey(sec: c_int, purge: c_int) c_int {
    _ = sec;
    _ = purge;
    return 0;
}
