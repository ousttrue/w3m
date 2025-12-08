const std = @import("std");
const c = @import("w3m.zig").c;

const LocalCgiType = enum {
    NORMAL,
    LIBDIR,
    CGIBIN,
};

fn checkPath(file: []const u8, _path: []const u8) c.Str {
    var path = _path;
    while (path.len > 0) {
        const p = std.mem.indexOfScalar(u8, path, ':');
        const tmp2 = if (p) |pos| blk: {
            break :blk c.allocStr(&path[0], @intCast(pos));
        } else blk: {
            break :blk &path[0];
        };
        const tmp = c.Strnew_charp(c.expandPath(tmp2).*.ptr);
        if (c.Strlastchar(tmp) != '/') {
            _ = c.Strcat_char(tmp, '/');
        }
        c.Strcat_charp(tmp, &file[0]);
        var st: c.struct_stat = undefined;
        if (c.stat(tmp.*.ptr, &st) == 0)
            return tmp;
        if (p) |pos| {
            path = path[pos + 1 ..];
            while (path[0] == ':') {
                path = path[1..];
            }
        } else {
            break;
        }
    }
    return null;
}

const LocalCgi = struct {
    cgi_type: ?LocalCgiType = null,
    file: []const u8,
    name: []const u8,
    path_info: []const u8 = &.{},

    pub fn init(_uri: []const u8) @This() {
        var uri = _uri;
        var this = @This(){
            .file = uri,
            .name = uri,
        };

        if (c.w3m_config.cgi_bin != null and std.mem.startsWith(u8, uri, "/cgi-bin/")) {
            const offset = 9;
            // if ((*path_info = strchr(uri + offset, '/')))
            if (std.mem.indexOfScalar(u8, uri[offset..], '/')) |pos| {
                this.path_info = uri[offset + pos ..];
                this.name = std.mem.span(c.allocStr(&uri[0], @intCast(uri.len - offset - pos)));
            }
            const tmp = checkPath(this.name[offset..], std.mem.span(c.w3m_config.cgi_bin));
            if (tmp == null) {
                this.cgi_type = .NORMAL;
                return this;
            }
            this.file = std.mem.span(tmp.*.ptr);
            this.cgi_type = .CGIBIN;
            return this;
        }

        const tmp = c.Strnew_charp(c.w3m_lib_dir());
        if (c.Strlastchar(tmp) != '/') {
            _ = c.Strcat_char(tmp, '/');
        }
        const offset = if (std.mem.startsWith(u8, uri, "/$LIB/")) 6 else if (std.mem.startsWith(u8, uri, tmp.*.ptr[0..tmp.*.length])) tmp.*.length else if (uri[0] == '/' and c.w3m_config.document_root != null) blk: {
            const tmp2 = c.Strnew_charp(c.w3m_config.document_root);
            if (c.Strlastchar(tmp2) != '/') {
                _ = c.Strcat_char(tmp2, '/');
            }
            c.Strcat_charp(tmp2, &uri[1]);
            if (!std.mem.startsWith(u8, std.mem.span(tmp2.*.ptr), std.mem.span(tmp.*.ptr))) {
                this.cgi_type = .NORMAL;
                return this;
            }
            uri = std.mem.span(tmp2.*.ptr);
            this.name = uri;
            break :blk tmp.*.length;
        } else {
            this.cgi_type = .NORMAL;
            return this;
        };

        {
            const x = uri[offset..];
            if (std.mem.indexOfScalar(u8, x, '/')) |pos| {
                this.path_info = x[pos..];
                this.name = std.mem.span(c.allocStr(&uri[0], @intCast(offset + pos)));
            }
        }

        c.Strcat_charp(tmp, &this.name[offset]);
        this.file = std.mem.span(tmp.*.ptr);
        this.cgi_type = .LIBDIR;
        return this;
    }

    pub fn check_local_cgi(this: @This()) bool {
        if (this.cgi_type != .LIBDIR and this.cgi_type != .CGIBIN)
            return false;

        var st: c.struct_stat = undefined;
        if (c.stat(&this.file[0], &st) < 0)
            return false;
        if (c.S_ISDIR(st.st_mode))
            return false;
        if ((st.st_uid == c.geteuid() and (st.st_mode & c.S_IXUSR) != 0) or
            (st.st_gid == c.getegid() and (st.st_mode & c.S_IXGRP) != 0) or
            (st.st_mode & c.S_IXOTH) != 0)
        { // executable */
            return true;
        }
        return false;
    }

    fn mydirname(this: @This()) [*c]const u8 {
        var p: usize = this.file.len;
        if (p != 0)
            p -= 1;
        while (p > 0 and this.file[p] == '/')
            p -= 1;
        while (p > 0 and this.file[p] != '/')
            p -= 1;
        if (this.file[p] != '/')
            return ".";
        while (p > 0 and this.file[p] == '/')
            p -= 1;
        return c.allocStr(&this.file[0], @intCast(p + 1));
    }

    fn run_child(
        this: @This(),
        uri: []const u8,
        referer: [*c]const u8,
        qstr: [*c]const u8,
        data: [*c]c.FormList,
        tmpf: [*c]const u8,
        fw: ?*c.FILE,
    ) void {
        set_cgi_environ(this.name, this.file, uri);
        if (this.path_info.len > 0) {
            c.set_environ("PATH_INFO", &this.path_info[0]);
        }
        if (referer != null and referer != c.NO_REFERER) {
            c.set_environ("HTTP_REFERER", referer);
        }
        if (data != null) {
            c.set_environ("REQUEST_METHOD", "POST");
            if (qstr != null) {
                c.set_environ("QUERY_STRING", qstr);
            }
            c.set_environ("CONTENT_LENGTH", c.Sprintf("%d", data.*.length).*.ptr);
            if (data.*.enctype == c.FORM_ENCTYPE_MULTIPART) {
                c.set_environ("CONTENT_TYPE", c.Sprintf("multipart/form-data; boundary=%s", data.*.boundary).*.ptr);
                _ = c.freopen(data.*.body, "r", c.stdin);
            } else {
                c.set_environ("CONTENT_TYPE", "application/x-www-form-urlencoded");
                _ = c.fwrite(data.*.body, 1, data.*.length, fw);
                _ = c.fclose(fw);
                _ = c.freopen(tmpf, "r", c.stdin);
            }
        } else {
            c.set_environ("REQUEST_METHOD", "GET");
            c.set_environ("QUERY_STRING", if (qstr != null) qstr else "");
            _ = c.freopen("/dev/null", "r", c.stdin);
        }

        const cgi_dir = this.mydirname();
        if (c.chdir(cgi_dir) == -1) {
            std.debug.print("failed to chdir to {s}\n", .{cgi_dir});
            c.exit(1);
        }

        const cgi_basename = c.mybasename(&this.file[0]).*.ptr;
        _ = c.execl(&this.file[0], cgi_basename, @as([*c]const u8, @ptrFromInt(0)));
        std.log.debug(
            "execl(\"{s}\", \"{s}\", NULL)\n",
            .{ this.file, cgi_basename },
        );
        c.exit(1);
    }
};

test "LocalCgi" {
    c.w3m_initialize();
    {
        const cgi = LocalCgi.init("/path/to/file");
        try std.testing.expectEqual(.NORMAL, cgi.cgi_type);
    }

    if (c.w3m_config.cgi_bin != null) {
        const cgi = LocalCgi.init("/cgi-bin/to/file");
        try std.testing.expectEqual(.CGIBIN, cgi.cgi_type);
    }

    if (c.w3m_config.document_root != null) {
        const cgi = LocalCgi.init("/$LIB/to/file");
        try std.testing.expectEqual(.LIBDIR, cgi.cgi_type);
    }
}

var Local_cookie_file: ?[*:0]const u8 = null;
var Local_cookie: c.Str = null;

// setup cookie for local CGI
pub fn localCookie() c.Str {
    if (Local_cookie != null)
        return Local_cookie;
    // c.srand48((long)New(char) + (long)time(NULL));
    var rand = std.Random.DefaultPrng.init(@intCast(std.time.milliTimestamp()));
    Local_cookie = c.Sprintf(
        "%ld@%s",
        rand.random().int(u64),
        if (c.w3m.HostName != null) c.w3m.HostName else &("localhost")[0],
    );
    return Local_cookie;
}

fn writeLocalCookie() void {
    if (Local_cookie_file != null)
        return;
    Local_cookie_file = c.tmpfname(c.TMPF_COOKIE, null).*.ptr;
    c.set_environ("LOCAL_COOKIE_FILE", Local_cookie_file);
    const f = c.fopen(Local_cookie_file, "wb");
    if (f != null) {
        _ = c.localCookie();
        _ = c.fwrite(Local_cookie.*.ptr, 1, Local_cookie.*.length, f);
        _ = c.fclose(f);
        _ = c.chmod(Local_cookie_file, c.S_IRUSR | c.S_IWUSR);
    }
}

fn set_cgi_environ(name: []const u8, file: []const u8, req_uri: []const u8) void {
    c.set_environ("SERVER_SOFTWARE", c.w3m_version);
    c.set_environ("SERVER_PROTOCOL", "HTTP/1.0");
    c.set_environ("SERVER_NAME", "localhost");
    c.set_environ("SERVER_PORT", "80"); // dummy
    c.set_environ("REMOTE_HOST", "localhost");
    c.set_environ("REMOTE_ADDR", "127.0.0.1");
    c.set_environ("GATEWAY_INTERFACE", "CGI/1.1");

    c.set_environ("SCRIPT_NAME", &name[0]);
    c.set_environ("SCRIPT_FILENAME", &file[0]);
    c.set_environ("REQUEST_URI", &req_uri[0]);
}

pub fn post(
    _uri: [*c]const u8,
    qstr: [*c]const u8,
    data: [*c]c.FormList,
    referer: [*c]const u8,
) ?*c.FILE {
    var uri = _uri;

    const cgi = LocalCgi.init(std.mem.span(uri));

    if (!cgi.check_local_cgi()) {
        return null;
    }
    writeLocalCookie();
    var fw: ?*c.FILE = null;
    var tmpf: [*c]const u8 = null;
    if (data != null and data.*.enctype != c.FORM_ENCTYPE_MULTIPART) {
        tmpf = c.tmpfname(c.TMPF_DFL, null).*.ptr;
        fw = c.fopen(tmpf, "w");
        if (fw == null)
            return null;
    }
    if (qstr != null)
        uri = c.Strnew_m_charp(uri, "?", qstr, @as(c.Str, @ptrFromInt(0))).*.ptr;

    var fr: ?*c.FILE = undefined;
    const pid = c.open_pipe_rw(&fr, null); // open_pipe_rw() forks
    // Don't invoke gc after here, or the program might crash in some platforms */
    if (pid < 0) {
        if (fw) |f| {
            _ = c.fclose(f);
        }
        return null;
    } else if (pid != 0) {
        // parent
        if (fw) |f| {
            _ = c.fclose(f);
        }
        return fr;
    }

    //
    // child
    //
    c.tui_setup_child(1, 2, if (fw != null) c.fileno(fw) else -1);

    cgi.run_child(std.mem.span(uri), referer, qstr, data, tmpf, fw);

    // Suppress compiler warning: function might return no value
    // This code is never reached.
    unreachable;
}
