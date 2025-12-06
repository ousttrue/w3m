pub const c = @cImport({
    @cInclude("w3m_runtime.h");
    @cInclude("local_cgi.h");
    @cInclude("Line.h");
    @cInclude("html.h");
    @cInclude("dns_order.h");
    @cInclude("cookie.h");
    @cInclude("term_entry.h");
    @cInclude("image.h");
    @cInclude("fm.h");
    @cInclude("buffer.h");
    @cInclude("history.h");
    @cInclude("gcstr/entity.h"); // UseAltEntity... etc
    @cInclude("screen.h");
    @cInclude("tui.h");
    @cInclude("func.h");
    @cInclude("HttpRequest.h");
    @cInclude("mimetype.h");
    @cInclude("mailcap.h");
    @cInclude("http_auth.h");

    @cInclude("etc.h");
    @cInclude("frame.h");
});
