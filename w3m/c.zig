pub const c = @cImport({
    // @cInclude("w3m.h");
    @cInclude("defun_impl.h");
    @cInclude("constants.h");
    @cInclude("termios.h");
    @cInclude("image_cache.h");
    @cInclude("LineInput.h");
    @cInclude("indep.h");
    @cInclude("alloc.h");
    @cInclude("growbuf.h");
    @cInclude("line_input.h");
    @cInclude("content_type.h");
    @cInclude("filepath.h");

    // #include "LineInput.h"
    // #include "Str.h"
    @cInclude("line.h");
    // #include "global.h"
    @cInclude("ctrlcode.h");
    @cInclude("form.h");
    // #include "local_cgi.h"
    // #include "tab.h"
    // #include "display.h"
    // #include "history.h"
    @cInclude("url.h");
    // #include "indep.h"
    // #include "etc.h"
    // #include "alloc.h"
    // #include "qsort_util.h"
    // #include "term_tty.h"
    @cInclude("screen.h");
    // #include <stdbool.h>
    // #include <dirent.h>
    // #include <w3m.h>

    @cInclude("wc_util.h");
    @cInclude("libwc/conv.h");
    @cInclude("libwc/charset.h");
    @cInclude("libwc/putc.h");

    @cInclude("sys/ioctl.h");
    @cInclude("unistd.h");

    @cInclude("libwc/wtf.h");
});
