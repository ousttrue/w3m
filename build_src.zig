pub const SRCS = [_][]const u8{
    "keybind.c",
    "loader.c",
    "datetime.c",
    "line.c",
    "filepath.c",
    "trap_jmp.c",

    "compression.c",
    "growbuf.c",
    "alloc.c",
    "app.c",
    "main.c",
    "defun.c",
    "file.c",
    "buffer.c",
    "display.c",
    "search.c",
    "linein.c",
    "table.c",
    "form.c",
    "map.c",
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
    "localcgi.c",
    "ftp.c",
    "readbuffer.c",
    "html_parser.c",
    "http_response.c",
    "http_auth.c",
    "http_auth_digest.c",
    "rand48.c",
    "strcase.c",
    "romannum.c",
    "os.c",
    "url.c",
};

pub const SRCS_POSIX = [_][]const u8{
    "tty_posix.c",
    "termsize_posix.c",
    "localcgi_posix.c",
    "os_posix.c",
    "etc_posix.c",
    "isocket_posix.c",
};

pub const SRCS_WIN32 = [_][]const u8{
    "tty_win32.c",
    "termsize_win32.c",
    "localcgi_win32.c",
    "os_win32.c",
    "etc_win32.c",
    "isocket_win32.c",
};
