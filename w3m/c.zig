pub const c = @cImport({
    // @cInclude("w3m.h");
    @cInclude("defun_impl.h");
    @cInclude("constants.h");
    @cInclude("termios.h");

    @cInclude("sys/ioctl.h");
    @cInclude("unistd.h");
});
