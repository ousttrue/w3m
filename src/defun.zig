const c = @cImport({
    // @cInclude("keymap.h");
    @cInclude("w3m.h");
    @cInclude("ui.h");
});

pub const CommandFunc = fn () callconv(.c) void;

// DEFUN(goLineF, BEGIN, "Go to the first line")
// {
//     _goLine("^");
// }
pub export fn goLineF() void {
    c._goLine(c.getUI(), "^");
}

// /* Go to the beginning of the line */
// DEFUN(linbeg, LINE_BEGIN, "Go to the beginning of the line")
// {
//     if (Currentbuf->firstLine == NULL)
//         return;
//     while (Currentbuf->currentLine->prev && Currentbuf->currentLine->bpos)
//         cursorUp0(Currentbuf, 1);
//     Currentbuf->pos = 0;
//     arrangeCursor(Currentbuf);
// }
pub export fn linbeg() void {
    c.ui_cursor_set_x(0);
}

pub fn init(addFunc: fn (cmd: *const CommandFunc, name: []const u8, desc: []const u8) void) void {
    addFunc(&goLineF, "BEGIN", "Go to the first line");
    addFunc(&linbeg, "LINE_BEGIN", "Go to the beginning of the line");
}
