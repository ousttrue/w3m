pub const c = @cImport({
    @cInclude("w3m_rc.h");
    @cInclude("w3m_types.h");
    @cInclude("cookie.h");
    @cInclude("termcap.h");
    @cInclude("image.h");
    @cInclude("screen.h");
    @cInclude("myctype.h");
    @cInclude("linein.h");
    @cInclude("history.h");
    @cInclude("indep.h");
    @cInclude("LineWriter.h");
    @cInclude("ctrlcode.h");
    @cInclude("buffer.h");
    @cInclude("html_form.h");
    @cInclude("tab.h");
    @cInclude("display.h");
    @cInclude("message.h");
    @cInclude("tab_list.h");
    @cInclude("option.h");
    @cInclude("local_cgi.h");
    @cInclude("symbol.h");
    @cInclude("siteconf.h");
    @cInclude("parsetag.h");
    @cInclude("mailcap.h");
    @cInclude("func.h");
    @cInclude("etc.h");
    // wc
    @cInclude("libwc/char_conv.h");
    @cInclude("libwc/charset.h");
    @cInclude("libwc/wtf_width.h");
    @cInclude("libwc/wtf_len.h");
    @cInclude("libwc/status.h");
    //
    @cInclude("unistd.h");
    @cInclude("stdlib.h");
    // generated
    @cInclude("funcheader.h");
});

