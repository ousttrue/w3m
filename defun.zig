const std = @import("std");
const c = @cImport({
    @cInclude("defun_impl.h");
});

const CmdFunc = struct {
    func: *const fn (args: c.CmdArgs) callconv(.c) void,
    desc: []const u8,
};

pub const NOTHING = CmdFunc{ .func = c.nulcmd, .desc = "Do nothing" };
pub const ESCMAP = CmdFunc{ .func = c.escmap, .desc = "ESC map" };
pub const ESCBMAP = CmdFunc{ .func = c.escbmap, .desc = "ESC [ map" };
pub const MULTIMAP = CmdFunc{ .func = c.multimap, .desc = "multimap" };
pub const NEXT_PAGE = CmdFunc{ .func = c.pgFore, .desc = "Scroll down one page" };
pub const PREV_PAGE = CmdFunc{ .func = c.pgBack, .desc = "Scroll up one page" };
pub const NEXT_HALF_PAGE = CmdFunc{ .func = c.hpgFore, .desc = "Scroll down half a page" };
pub const PREV_HALF_PAGE = CmdFunc{ .func = c.hpgBack, .desc = "Scroll up half a page" };
pub const UP = CmdFunc{ .func = c.lup1, .desc = "Scroll the screen up one line" };
pub const DOWN = CmdFunc{ .func = c.ldown1, .desc = "Scroll the screen down one line" };
pub const CENTER_V = CmdFunc{ .func = c.ctrCsrV, .desc = "Center on cursor line" };
pub const CENTER_H = CmdFunc{ .func = c.ctrCsrH, .desc = "Center on cursor column" };
pub const REDRAW = CmdFunc{ .func = c.rdrwSc, .desc = "Draw the screen anew" };
pub const SEARCH = CmdFunc{ .func = c.srchfor, .desc = "Search forward" };
pub const ISEARCH = CmdFunc{ .func = c.isrchfor, .desc = "Incremental search forward" };
pub const SEARCH_BACK = CmdFunc{ .func = c.srchbak, .desc = "Search backward" };
pub const ISEARCH_BACK = CmdFunc{ .func = c.isrchbak, .desc = "Incremental search backward" };
pub const SEARCH_NEXT = CmdFunc{ .func = c.srchnxt, .desc = "Continue search forward" };
pub const SEARCH_PREV = CmdFunc{ .func = c.srchprv, .desc = "Continue search backward" };
pub const SHIFT_LEFT = CmdFunc{ .func = c.shiftl, .desc = "Shift screen left" };
pub const SHIFT_RIGHT = CmdFunc{ .func = c.shiftr, .desc = "Shift screen right" };
pub const RIGHT = CmdFunc{ .func = c.col1R, .desc = "Shift screen one column right" };
pub const LEFT = CmdFunc{ .func = c.col1L, .desc = "Shift screen one column left" };
pub const SETENV = CmdFunc{ .func = c.setEnv, .desc = "Set environment variable" };
pub const PIPE_BUF = CmdFunc{ .func = c.pipeBuf, .desc = "Pipe current buffer through a shell command and display output" };
pub const PIPE_SHELL = CmdFunc{ .func = c.pipesh, .desc = "Execute shell command and display output" };
pub const READ_SHELL = CmdFunc{ .func = c.readsh, .desc = "Execute shell command and display output" };
pub const SHELL = CmdFunc{ .func = c.execsh, .desc = "Execute shell command and display output" };
pub const LOAD = CmdFunc{ .func = c.ldfile, .desc = "Open local file in a new buffer" };
pub const HELP = CmdFunc{ .func = c.ldhelp, .desc = "Show help panel" };
pub const MOVE_LEFT = CmdFunc{ .func = c.movL, .desc = "Cursor left" };
pub const MOVE_LEFT1 = CmdFunc{ .func = c.movL1, .desc = "Cursor left. With edge touched, slide" };
pub const MOVE_DOWN = CmdFunc{ .func = c.movD, .desc = "Cursor down" };
pub const MOVE_DOWN1 = CmdFunc{ .func = c.movD1, .desc = "Cursor down. With edge touched, slide" };
pub const MOVE_UP = CmdFunc{ .func = c.movU, .desc = "Cursor up" };
pub const MOVE_UP1 = CmdFunc{ .func = c.movU1, .desc = "Cursor up. With edge touched, slide" };
pub const MOVE_RIGHT = CmdFunc{ .func = c.movR, .desc = "Cursor right" };
pub const MOVE_RIGHT1 = CmdFunc{ .func = c.movR1, .desc = "Cursor right. With edge touched, slide" };
pub const PREV_WORD = CmdFunc{ .func = c.movLW, .desc = "Move to the previous word" };
pub const NEXT_WORD = CmdFunc{ .func = c.movRW, .desc = "Move to the next word" };
pub const ABORT = CmdFunc{ .func = c.quitfm, .desc = "Quit without confirmation" };
pub const QUIT = CmdFunc{ .func = c.qquitfm, .desc = "Quit with confirmation request" };
pub const SELECT = CmdFunc{ .func = c.selBuf, .desc = "Display buffer-stack panel" };
pub const SUSPEND = CmdFunc{ .func = c.susp, .desc = "Suspend w3m to background" };
pub const GOTO_LINE = CmdFunc{ .func = c.goLine, .desc = "Go to the specified line" };
pub const BEGIN = CmdFunc{ .func = c.goLineF, .desc = "Go to the first line" };
pub const END = CmdFunc{ .func = c.goLineL, .desc = "Go to the last line" };
pub const LINE_BEGIN = CmdFunc{ .func = c.linbeg, .desc = "Go to the beginning of the line" };
pub const LINE_END = CmdFunc{ .func = c.linend, .desc = "Go to the end of the line" };
pub const EDIT = CmdFunc{ .func = c.editBf, .desc = "Edit local source" };
pub const EDIT_SCREEN = CmdFunc{ .func = c.editScr, .desc = "Edit rendered copy of document" };
pub const MARK = CmdFunc{ .func = c._mark, .desc = "Set/unset mark" };
pub const NEXT_MARK = CmdFunc{ .func = c.nextMk, .desc = "Go to the next mark" };
pub const PREV_MARK = CmdFunc{ .func = c.prevMk, .desc = "Go to the previous mark" };
pub const REG_MARK = CmdFunc{ .func = c.reMark, .desc = "Mark all occurences of a pattern" };
pub const GOTO_LINK = CmdFunc{ .func = c.followA, .desc = "Follow current hyperlink in a new buffer" };
pub const VIEW_IMAGE = CmdFunc{ .func = c.followI, .desc = "Display image in viewer" };
pub const SUBMIT = CmdFunc{ .func = c.submitForm, .desc = "Submit form" };
pub const LINK_BEGIN = CmdFunc{ .func = c.topA, .desc = "Move to the first hyperlink" };
pub const LINK_END = CmdFunc{ .func = c.lastA, .desc = "Move to the last hyperlink" };
pub const LINK_N = CmdFunc{ .func = c.nthA, .desc = "Go to the nth link" };
pub const NEXT_LINK = CmdFunc{ .func = c.nextA, .desc = "Move to the next hyperlink" };
pub const PREV_LINK = CmdFunc{ .func = c.prevA, .desc = "Move to the previous hyperlink" };
pub const NEXT_VISITED = CmdFunc{ .func = c.nextVA, .desc = "Move to the next visited hyperlink" };
pub const PREV_VISITED = CmdFunc{ .func = c.prevVA, .desc = "Move to the previous visited hyperlink" };
pub const NEXT_LEFT = CmdFunc{ .func = c.nextL, .desc = "Move left to the next hyperlink" };
pub const NEXT_LEFT_UP = CmdFunc{ .func = c.nextLU, .desc = "Move left or upward to the next hyperlink" };
pub const NEXT_RIGHT = CmdFunc{ .func = c.nextR, .desc = "Move right to the next hyperlink" };
pub const NEXT_RIGHT_DOWN = CmdFunc{ .func = c.nextRD, .desc = "Move right or downward to the next hyperlink" };
pub const NEXT_DOWN = CmdFunc{ .func = c.nextD, .desc = "Move downward to the next hyperlink" };
pub const NEXT_UP = CmdFunc{ .func = c.nextU, .desc = "Move upward to the next hyperlink" };
pub const NEXT = CmdFunc{ .func = c.nextBf, .desc = "Switch to the next buffer" };
pub const PREV = CmdFunc{ .func = c.prevBf, .desc = "Switch to the previous buffer" };
pub const BACK = CmdFunc{ .func = c.backBf, .desc = "Close current buffer and return to the one below in stack" };
pub const DELETE_PREVBUF = CmdFunc{ .func = c.deletePrevBuf, .desc = "Delete previous buffer (mainly for local CGI-scripts)" };
pub const GOTO = CmdFunc{ .func = c.goURL, .desc = "Open specified document in a new buffer" };
pub const GOTO_HOME = CmdFunc{ .func = c.goHome, .desc = "Open home page in a new buffer" };
pub const GOTO_RELATIVE = CmdFunc{ .func = c.gorURL, .desc = "Go to relative address" };
pub const BOOKMARK = CmdFunc{ .func = c.ldBmark, .desc = "View bookmarks" };
pub const ADD_BOOKMARK = CmdFunc{ .func = c.adBmark, .desc = "Add current page to bookmarks" };
pub const OPTIONS = CmdFunc{ .func = c.ldOpt, .desc = "Display options setting panel" };
pub const SET_OPTION = CmdFunc{ .func = c.setOpt, .desc = "Set option" };
pub const MSGS = CmdFunc{ .func = c.msgs, .desc = "Display error messages" };
pub const INFO = CmdFunc{ .func = c.pginfo, .desc = "Display information about the current document" };
pub const LINK_MENU = CmdFunc{ .func = c.linkMn, .desc = "Pop up link element menu" };
pub const ACCESSKEY = CmdFunc{ .func = c.accessKey, .desc = "Pop up accesskey menu" };
pub const LIST_MENU = CmdFunc{ .func = c.listMn, .desc = "Pop up menu for hyperlinks to browse to" };
pub const MOVE_LIST_MENU = CmdFunc{ .func = c.movlistMn, .desc = "Pop up menu to navigate between hyperlinks" };
pub const LIST = CmdFunc{ .func = c.linkLst, .desc = "Show all URLs referenced" };
pub const COOKIE = CmdFunc{ .func = c.cooLst, .desc = "View cookie list" };
pub const HISTORY = CmdFunc{ .func = c.ldHist, .desc = "Show browsing history" };
pub const SAVE_LINK = CmdFunc{ .func = c.svA, .desc = "Save hyperlink target" };
pub const SAVE_IMAGE = CmdFunc{ .func = c.svI, .desc = "Save inline image" };
pub const PRINT = CmdFunc{ .func = c.svBuf, .desc = "Save rendered document" };
pub const SAVE = CmdFunc{ .func = c.svSrc, .desc = "Save document source" };
pub const PEEK_LINK = CmdFunc{ .func = c.peekURL, .desc = "Show target address" };
pub const PEEK_IMG = CmdFunc{ .func = c.peekIMG, .desc = "Show image address" };
pub const PEEK = CmdFunc{ .func = c.curURL, .desc = "Show current address" };
pub const SOURCE = CmdFunc{ .func = c.vwSrc, .desc = "Toggle between HTML shown or processed" };
pub const RELOAD = CmdFunc{ .func = c.reload, .desc = "Load current document anew" };
pub const RESHAPE = CmdFunc{ .func = c.reshape, .desc = "Re-render document" };
pub const CHARSET = CmdFunc{ .func = c.docCSet, .desc = "Change the character encoding for the current document" };
pub const DEFAULT_CHARSET = CmdFunc{ .func = c.defCSet, .desc = "Change the default character encoding" };
pub const MARK_URL = CmdFunc{ .func = c.chkURL, .desc = "Turn URL-like strings into hyperlinks" };
pub const MARK_WORD = CmdFunc{ .func = c.chkWORD, .desc = "Turn current word into hyperlink" };
pub const MARK_MID = CmdFunc{ .func = c.chkNMID, .desc = "Turn Message-ID-like strings into hyperlinks" };
pub const FRAME = CmdFunc{ .func = c.rFrame, .desc = "Toggle rendering HTML frames" };
pub const EXTERN = CmdFunc{ .func = c.extbrz, .desc = "Display using an external browser" };
pub const EXTERN_LINK = CmdFunc{ .func = c.linkbrz, .desc = "Display target using an external browser" };
pub const LINE_INFO = CmdFunc{ .func = c.curlno, .desc = "Display current position in document" };
pub const DISPLAY_IMAGE = CmdFunc{ .func = c.dispI, .desc = "Restart loading and drawing of images" };
pub const STOP_IMAGE = CmdFunc{ .func = c.stopI, .desc = "Stop loading and drawing of images" };
pub const MOUSE_TOGGLE = CmdFunc{ .func = c.nulcmd, .desc = "Toggle mouse support" };
pub const MOUSE = CmdFunc{ .func = c.nulcmd, .desc = "mouse operation" };
pub const SGRMOUSE = CmdFunc{ .func = c.nulcmd, .desc = "SGR 1006 mouse operation" };
pub const MOVE_MOUSE = CmdFunc{ .func = c.nulcmd, .desc = "Move cursor to mouse pointer" };
pub const MENU_MOUSE = CmdFunc{ .func = c.nulcmd, .desc = "Pop up menu at mouse pointer" };
pub const TAB_MOUSE = CmdFunc{ .func = c.nulcmd, .desc = "Select tab by mouse action" };
pub const CLOSE_TAB_MOUSE = CmdFunc{ .func = c.nulcmd, .desc = "Close tab at mouse pointer" };
pub const VERSION = CmdFunc{ .func = c.dispVer, .desc = "Display the version of w3m" };
pub const WRAP_TOGGLE = CmdFunc{ .func = c.wrapToggle, .desc = "Toggle wrapping mode in searches" };
pub const DICT_WORD = CmdFunc{ .func = c.dictword, .desc = "Execute dictionary command (see README.dict)" };
pub const DICT_WORD_AT = CmdFunc{ .func = c.dictwordat, .desc = "Execute dictionary command for word at cursor" };
pub const COMMAND = CmdFunc{ .func = c.execCmd, .desc = "Invoke w3m function(s)" };
pub const ALARM = CmdFunc{ .func = c.setAlarm, .desc = "Set alarm" };
pub const REINIT = CmdFunc{ .func = c.reinit, .desc = "Reload configuration file" };
pub const DEFINE_KEY = CmdFunc{ .func = c.defKey, .desc = "Define a binding between a key stroke combination and a command" };
pub const NEW_TAB = CmdFunc{ .func = c.newT, .desc = "Open a new tab (with current document)" };
pub const CLOSE_TAB = CmdFunc{ .func = c.closeT, .desc = "Close tab" };
pub const NEXT_TAB = CmdFunc{ .func = c.nextT, .desc = "Switch to the next tab" };
pub const PREV_TAB = CmdFunc{ .func = c.prevT, .desc = "Switch to the previous tab" };
pub const TAB_LINK = CmdFunc{ .func = c.tabA, .desc = "Follow current hyperlink in a new tab" };
pub const TAB_GOTO = CmdFunc{ .func = c.tabURL, .desc = "Open specified document in a new tab" };
pub const TAB_GOTO_RELATIVE = CmdFunc{ .func = c.tabrURL, .desc = "Open relative address in a new tab" };
pub const TAB_RIGHT = CmdFunc{ .func = c.tabR, .desc = "Move right along the tab bar" };
pub const TAB_LEFT = CmdFunc{ .func = c.tabL, .desc = "Move left along the tab bar" };
pub const DOWNLOAD_LIST = CmdFunc{ .func = c.ldDL, .desc = "Display downloads panel" };
pub const UNDO = CmdFunc{ .func = c.undoPos, .desc = "Cancel the last cursor movement" };
pub const REDO = CmdFunc{ .func = c.redoPos, .desc = "Cancel the last undo" };
pub const CURSOR_TOP = CmdFunc{ .func = c.cursorTop, .desc = "Move cursor to the top of the screen" };
pub const CURSOR_MIDDLE = CmdFunc{ .func = c.cursorMiddle, .desc = "Move cursor to the middle of the screen" };
pub const CURSOR_BOTTOM = CmdFunc{ .func = c.cursorBottom, .desc = "Move cursor to the bottom of the screen" };
pub const MENU = CmdFunc{ .func = c.mainMn, .desc = "Pop up menu" };
pub const SELECT_MENU = CmdFunc{ .func = c.selMn, .desc = "Pop up buffer-stack menu" };
pub const TAB_MENU = CmdFunc{ .func = c.tabMn, .desc = "Pop up tab selection menu" };

const funcMap: std.StaticStringMap(CmdFunc) = .initComptime(blk: {
    const KV = struct { []const u8, CmdFunc };
    var map_kvs: []const KV = &.{};
    // only pub decl
    for (@typeInfo(@This()).@"struct".decls) |d| {
        const f = @field(@This(), d.name);
        if (@TypeOf(f) == CmdFunc) {
            map_kvs = map_kvs ++ [1]KV{.{ d.name, f }};
        }
    }
    break :blk map_kvs;
});

export fn w3mFunc(_cmd: [*c]const u8) void {
    if (_cmd) |cmd| {
        if (funcMap.get(std.mem.span(cmd))) |found| {
            found.func(.{});
        } else {
            std.log.warn("{s} not found", .{cmd});
        }
    } else {
        std.log.warn("null cmd", .{});
    }
}
