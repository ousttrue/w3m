pub const SRCS = [_][]const u8{
    "keybind.c",

    "main.c",
    "defun.c",
    "file.c",
    "digest_auth.c",
    "buffer.c",
    "display.c",
    "etc.c",
    "search.c",
    "linein.c",
    "table.c",
    "form.c",
    "map.c",
    "frame.c",
    "rc.c",
    "menu.c",
    "mailcap.c",
    "symbol.c",
    "entity.c",

    "scr.c",
    "terms.c",

    "mimehead.c",
    "regex.c",
    "news.c",
    "func.c",
    "cookie.c",
    "history.c",

    "anchor.c",
    "parsetagx.c",
    "tagtable.c",
    "istream.c",

    "Str.c",
    "indep.c",
    "textlist.c",
    "parsetag.c",
    "myctype.c",
    "hash.c",

    "version.c",
    "url_stream.c",
    "downloadlist.c",
};

pub const SRCS_POSIX = [_][]const u8{
    "tty_posix.c",
    "termsize_posix.c",
    "localcgi_posix.c",
    "ftp_posix.c",
    "etc_posix.c",
    "file_posix.c",
    "indep_posix.c",
    "form_posix.c",
};

pub const SRCS_WIN32 = [_][]const u8{
    "tty_win32.c",
    "termsize_win32.c",
    "localcgi_win32.c",
    "rand48_win32.c",
};
