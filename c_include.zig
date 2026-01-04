pub const c = @cImport({
    @cInclude("w3m_rc.h");
    @cInclude("termcap.h");
    @cInclude("image.h");
    @cInclude("screen.h");
    @cInclude("myctype.h");
    @cInclude("stdlib.h");
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
    // wc
    @cInclude("char_conv.h");
    @cInclude("charset.h");
    @cInclude("wtf_width.h");
});

