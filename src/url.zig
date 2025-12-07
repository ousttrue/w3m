const std = @import("std");
const c = @import("w3m.zig").c;
const util = @import("util.zig");

const WhitSpaceMode = enum {
    Allow,
    Ignore,
    Replace,
};

const CopyPathOpts = struct {
    WHITESPACE: WhitSpaceMode = .Allow,
    LOWERCASE: bool = false,
};

fn copyPath(_orgpath: [*c]const u8, _length: c_int, option: CopyPathOpts) [*c]const u8 {
    const tmp = c.Strnew();
    var orgpath = _orgpath;
    var length = _length;
    while (true) {
        var ch = orgpath[0];
        if (ch == 0) {
            break;
        }
        if (length == 0) {
            break;
        }
        if (option.LOWERCASE)
            ch = std.ascii.toLower(ch);
        if (std.ascii.isWhitespace(ch)) {
            switch (option.WHITESPACE) {
                .Allow => {
                    _ = c.Strcat_char(tmp, ch);
                },
                .Ignore => {
                    // do nothing
                },
                .Replace => {
                    c.Strcat_charp(tmp, "%20");
                },
            }
        } else {
            _ = c.Strcat_char(tmp, ch);
        }
        orgpath += 1;
        length -= 1;
    }
    return tmp.*.ptr;
}

fn do_label(p_url: *c.Url, p: [*c]const u8) void {
    if (p_url.scheme == c.SCM_MISSING) {
        p_url.scheme = c.SCM_LOCAL;
        p_url.file = c.allocStr(p, -1);
        p_url.label = null;
    } else if (p[0] == '#') {
        p_url.label = c.allocStr(p + 1, -1);
    } else {
        p_url.label = null;
    }
}

fn do_query(p_url: *c.Url, _p: [*c]const u8) void {
    var p = _p;
    if (p[0] == '?') {
        p += 1;
        const q = p;
        while (p[0] != 0 and p[0] != '#') {
            p += 1;
        }
        p_url.query = copyPath(q, @intCast(p - q), .{});
    }
    do_label(p_url, p);
}

// XXX: note html.h SCM_
const DefaultPort = [_]u16{
    80, // http
    70, // gopher
    21, // ftp
    21, // ftpdir
    0, // local - not defined
    0, // local-CGI - not defined?
    0, // exec - not defined?
    119, // nntp
    119, // nntp group
    119, // news
    119, // news group
    0, // data - not defined
    0, // mailto - not defined
    443, // https
};

const HTTP_DEFAULT_FILE = "/";

fn DefaultFile(scheme: c.UrlScheme) [*c]u8 {
    return switch (scheme) {
        c.SCM_HTTP, c.SCM_HTTPS => c.allocStr(HTTP_DEFAULT_FILE, -1),
        c.SCM_GOPHER => c.allocStr("1", -1),
        c.SCM_LOCAL, c.SCM_LOCAL_CGI, c.SCM_FTP, c.SCM_FTPDIR => c.allocStr("/", -1),
        else => null,
    };
}

fn analyze_file(p_url: *c.Url, _p: [*c]const u8) void {
    var p = _p;
    if (p_url.scheme == c.SCM_LOCAL and
        p_url.user == null and
        p_url.host != null and
        p_url.host[0] != 0 and c.is_localhost(p_url.host) == 0)
    {
        // In the environments other than CYGWIN, a URL like
        // file://host/file is regarded as ftp://host/file.
        // On the other hand, file://host/file on CYGWIN is
        // regarded as local access to the file //host/file.
        // `host' is a netbios-hostname, drive, or any other
        // name; It is CYGWIN system call who interprets that.
        p_url.scheme = c.SCM_FTP; // ftp://host/... */
        if (p_url.port == 0) {
            p_url.port = DefaultPort[c.SCM_FTP];
        }
    }

    if ((p[0] == 0 or p[0] == '#' or p[0] == '?') and p_url.host == null) {
        p_url.file = "";
        do_query(p_url, p);
        return;
    }

    if (p_url.scheme == c.SCM_LOCAL) {
        var q = p;
        if (q[0] == '/')
            q += 1;
        if (std.ascii.isAlphabetic(q[0]) and (q[1] == ':' or q[1] == '|')) {
            if (q[1] == '|') {
                const tmp = c.allocStr(q, -1);
                tmp[1] = ':';
                p = tmp;
            } else {
                p = q;
            }
        }
    }

    var q = p;
    if (p_url.scheme == c.SCM_GOPHER) {
        if (q[0] == '/') {
            q += 1;
        }
        if (q[0] != 0 and q[0] != '/' and q[1] != '/' and q[2] == '/') {
            q += 1;
        }
    }
    if (p[0] == '/')
        p += 1;
    if (p[0] == 0 or p[0] == '#' or p[0] == '?') {
        // scheme://host[:port]/
        p_url.file = DefaultFile(p_url.scheme);
        do_query(p_url, p);
        return;
    }
    if (p_url.scheme == c.SCM_GOPHER and p[0] == 'R') {
        p += 1;
        if (p[0] == 0) {
            p_url.file = "";
            do_query(p_url, p);
            return;
        }
        const tmp = c.Strnew();
        _ = c.Strcat_char(tmp, p[0]);
        p += 1;
        while (p[0] != 0 and p[0] != '/')
            p += 1;
        c.Strcat_charp(tmp, p);
        while (p[0] != 0)
            p += 1;
        p_url.file = copyPath(tmp.*.ptr, -1, .{ .WHITESPACE = .Ignore });
    } else {
        const cgi = c.strchr(p, '?');

        while (true) {
            while (p[0] != 0 and p[0] != '#' and p != cgi)
                p += 1;
            if (p[0] == '#' and p_url.scheme == c.SCM_LOCAL) {
                // According to RFC2396, # means the beginning of
                // URI-reference, and # should be escaped.  But,
                // if the scheme is SCM_LOCAL, the special
                // treatment will apply to # for convinience.
                const prev = p - 1;
                if (p > q and prev == '/' and (cgi == null or p < cgi)) {
                    // # comes as the first character of the file name
                    // that means, # is not a label but a part of the file
                    // name.
                    p += 1;
                    continue;
                } else if (p[1] == 0) {
                    // # comes as the last character of the file name that
                    // means, # is not a label but a part of the file
                    // name.
                    p += 1;
                }
            }
            break;
        }

        if (p_url.scheme == c.SCM_LOCAL or p_url.scheme == c.SCM_MISSING) {
            p_url.file = copyPath(q, @intCast(p - q), .{});
        } else {
            p_url.file = copyPath(q, @intCast(p - q), .{ .WHITESPACE = .Ignore });
        }
    }

    do_query(p_url, p);
}

fn analyze_url(p_url: *c.Url, _p: [*c]const u8) void {
    var p = _p;
    var q = p;
    if (q[0] == '[') { // rfc2732,rfc2373 compliance
        p += 1;
        while (c.IS_XDIGIT(p[0]) or p[0] == ':' or p[0] == '.')
            p += 1;
        if (p[0] != ']' or (p[1] != 0 and std.mem.containsAtLeastScalar(u8, ":/?#", 1, p[1])))
            p = q;
    }
    while (p[0] != 0 and !std.mem.containsAtLeastScalar(u8, ":/@?#", 1, p[0]))
        p += 1;
    switch (p[0]) {
        ':' => {
            // scheme://user:pass@host or
            // scheme://host:port
            const qq = q;
            p += 1;
            q = p;
            while (p[0] != 0 and std.mem.containsAtLeastScalar(u8, "@/?#", 1, p[0]))
                p += 1;
            if (p[0] == '@') {
                // scheme://user:pass@...
                p_url.user = copyPath(qq, @intCast(q - 1 - qq), .{ .WHITESPACE = .Ignore });
                p_url.pass = copyPath(q, @intCast(p - q), .{});
                p += 1;
                analyze_url(p_url, p);
                return;
            }
            // scheme://host:port/
            p_url.host = copyPath(qq, @intCast(q - 1 - qq), .{ .WHITESPACE = .Ignore, .LOWERCASE = true });
            const tmp = c.Strnew_charp_n(q, @intCast(p - q));
            p_url.port = util.atoi(c_int, tmp.*.ptr);
            // *p is one of ['\0', '/', '?', '#']
        },
        '@' => {
            // scheme://user@...
            p_url.user = copyPath(q, @intCast(p - q), .{ .WHITESPACE = .Ignore });
            p += 1;
            analyze_url(p_url, p);
            return;
        },
        0, '/', '?', '#' => {
            p_url.host = copyPath(q, @intCast(p - q), .{ .WHITESPACE = .Ignore, .LOWERCASE = true });
            if (p_url.scheme != c.SCM_UNKNOWN) {
                p_url.port = DefaultPort[p_url.scheme];
            } else {
                p_url.port = 0;
            }
        },
        else => {},
    }

    analyze_file(p_url, p);
}

pub fn parseURL(_url: [*c]const u8, p_url: *c.Url, _current: [*c]c.Url) void {
    // quote 0x01-0x20, 0x7F-0xFF
    const url = c.url_quote(_url).*.ptr;

    var p = url;
    c.copyParsedURL(p_url, null);
    p_url.scheme = c.SCM_MISSING;

    // RFC1808: Relative Uniform Resource Locators
    // 4.  Resolving Relative URLs
    if (url[0] == 0 or url[0] == '#') {
        if (_current) |current| {
            c.copyParsedURL(p_url, current);
        }
        do_label(p_url, p);
        return;
    }
    if (std.ascii.isAlphabetic(p[0]) and (p[1] == ':' or p[1] == '|')) {
        // 'C:/' etc ?
        // 'C:|' ?
        p_url.scheme = c.SCM_LOCAL;
        analyze_file(p_url, p);
        return;
    }

    // search for scheme
    p_url.scheme = c.getURLScheme(@ptrCast(@constCast(&p)));
    if (p_url.scheme == c.SCM_MISSING) {
        // scheme part is not found in the url. This means either
        // (a) the url is relative to the current or (b) the url
        // denotes a filename (therefore the scheme is SCM_LOCAL).
        if (_current) |current| {
            switch (current.*.scheme) {
                c.SCM_LOCAL, c.SCM_LOCAL_CGI => {
                    p_url.scheme = c.SCM_LOCAL;
                },
                c.SCM_FTP, c.SCM_FTPDIR => {
                    p_url.scheme = c.SCM_FTP;
                },
                c.SCM_NNTP, c.SCM_NNTP_GROUP => {
                    p_url.scheme = c.SCM_NNTP;
                },
                c.SCM_NEWS, c.SCM_NEWS_GROUP => {
                    p_url.scheme = c.SCM_NEWS;
                },
                else => {
                    p_url.scheme = current.*.scheme;
                },
            }
        } else {
            p_url.scheme = c.SCM_LOCAL;
        }
        p = url;
        if (std.mem.startsWith(u8, std.mem.span(p), "//")) {
            // URL begins with //
            // it means that 'scheme:' is abbreviated
            p += 2;
            analyze_url(p_url, p);
            return;
        }
        // the url doesn't begin with '//'
        //         analyze_file(p_url, p);
        //         return;
    }
    // scheme part has been found
    if (p_url.scheme == c.SCM_UNKNOWN) {
        p_url.file = c.allocStr(url, -1);
        return;
    }
    // get host and port
    if (p[0] != '/' or p[1] != '/') { // scheme:foo or scheme:/foo
        p_url.host = null;
        if (p_url.scheme != c.SCM_UNKNOWN) {
            p_url.port = DefaultPort[p_url.scheme];
        } else {
            p_url.port = 0;
        }
        analyze_file(p_url, p);
        return;
    }
    // after here, p begins with //
    if (p_url.scheme == c.SCM_LOCAL) { // file://foo
        if (p[2] == '/' or p[2] == '~'
            // <A HREF="file:///foo">file:///foo</A>  or <A HREF="file://~user">file://~user</A>
        or (std.ascii.isAlphabetic(p[2]) and (p[3] == ':' or p[3] == '|'))
            // <A HREF="file://DRIVE/foo">file://DRIVE/foo</A>
        ) {
            p += 2;
            analyze_file(p_url, p);
            return;
        }
    }
    p += 2; // scheme://foo
    //          ^p is here

    analyze_url(p_url, p);
}
