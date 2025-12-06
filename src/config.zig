const std = @import("std");
const gcstr = @import("gcstr");
const c = @import("w3m.zig").c;

var display_charset_str: [*c]c.wc_ces_list = null;
var document_charset_str: [*c]c.wc_ces_list = null;
var system_charset_str: [*c]c.wc_ces_list = null;

const ParamTypes = enum {
    P_INT,
    P_SHORT,
    P_CHARINT,
    P_CHAR,
    P_STRING,
    P_SSLPATH,
    P_COLOR,
    P_CODE,
    P_PIXELS,
    P_NZINT,
    P_SCALE,
};

const ParamInputTypes = enum {
    PI_TEXT,
    PI_ONOFF,
    PI_SEL_C,
    PI_CODE,
};

const sel_c = struct {
    value: c_int,
    cvalue: []const u8,
    text: []const u8,
};

const param_ptr = struct {
    name: []const u8,
    type: ParamTypes,
    inputtype: ParamInputTypes,
    /// value
    varptr: *anyopaque,
    /// comment
    comment: []const u8,
    /// enum values
    select: []const sel_c = &.{},
    ces_list: ?*[*c]c.wc_ces_list = null,
};

const param_section = struct {
    name: []const u8,
    /// param list
    params: []const param_ptr,
};

const colorstr = [_]sel_c{
    .{ .value = 0, .cvalue = "black", .text = "black" },
    .{ .value = 1, .cvalue = "red", .text = "red" },
    .{ .value = 2, .cvalue = "green", .text = "green" },
    .{ .value = 3, .cvalue = "yellow", .text = "yellow" },
    .{ .value = 4, .cvalue = "blue", .text = "blue" },
    .{ .value = 5, .cvalue = "magenta", .text = "magenta" },
    .{ .value = 6, .cvalue = "cyan", .text = "cyan" },
    .{ .value = 7, .cvalue = "white", .text = "white" },
    .{ .value = 8, .cvalue = "terminal", .text = "terminal" },
};

const defaulturls = [_]sel_c{
    .{ .value = c.DEFAULT_URL_EMPTY, .cvalue = &.{c.DEFAULT_URL_EMPTY + '0'}, .text = "none" },
    .{ .value = c.DEFAULT_URL_CURRENT, .cvalue = &.{c.DEFAULT_URL_CURRENT + '0'}, .text = "current URL" },
    .{ .value = c.DEFAULT_URL_LINK, .cvalue = &.{c.DEFAULT_URL_LINK + '0'}, .text = "link URL" },
};

const displayinsdel = [_]sel_c{
    .{ .value = c.DISPLAY_INS_DEL_SIMPLE, .cvalue = &.{c.DISPLAY_INS_DEL_SIMPLE + '0'}, .text = "simple" },
    .{ .value = c.DISPLAY_INS_DEL_NORMAL, .cvalue = &.{c.DISPLAY_INS_DEL_NORMAL + '0'}, .text = "use tag" },
    .{ .value = c.DISPLAY_INS_DEL_FONTIFY, .cvalue = &.{c.DISPLAY_INS_DEL_FONTIFY + '0'}, .text = "fontify" },
};

const wheelmode = [_]sel_c{
    .{ .value = 1, .cvalue = "1", .text = "A:relative to screen height" },
    .{ .value = 0, .cvalue = "0", .text = "B:fixed speed" },
};

const dnsorders = [_]sel_c{
    .{ .value = c.DNS_ORDER_UNSPEC, .cvalue = &.{c.DNS_ORDER_UNSPEC + '0'}, .text = "unspecified" },
    .{ .value = c.DNS_ORDER_INET_INET6, .cvalue = &.{c.DNS_ORDER_INET_INET6 + '0'}, .text = "inet inet6" },
    .{ .value = c.DNS_ORDER_INET6_INET, .cvalue = &.{c.DNS_ORDER_INET6_INET + '0'}, .text = "inet6 inet" },
    .{ .value = c.DNS_ORDER_INET_ONLY, .cvalue = &.{c.DNS_ORDER_INET_ONLY + '0'}, .text = "inet only" },
    .{ .value = c.DNS_ORDER_INET6_ONLY, .cvalue = &.{c.DNS_ORDER_INET6_ONLY + '0'}, .text = "inet6 only" },
};

const badcookiestr = [_]sel_c{
    .{ .value = c.ACCEPT_BAD_COOKIE_DISCARD, .cvalue = &.{c.ACCEPT_BAD_COOKIE_DISCARD + '0'}, .text = "discard" },
    .{ .value = c.ACCEPT_BAD_COOKIE_ASK, .cvalue = &.{c.ACCEPT_BAD_COOKIE_ASK + '0'}, .text = "ask" },
};

const auto_detect_str = [_]sel_c{
    .{ .value = c.WC_OPT_DETECT_OFF, .cvalue = &.{c.WC_OPT_DETECT_OFF + '0'}, .text = "OFF" },
    .{ .value = c.WC_OPT_DETECT_ISO_2022, .cvalue = &.{c.WC_OPT_DETECT_ISO_2022 + '0'}, .text = "Only ISO 2022" },
    .{ .value = c.WC_OPT_DETECT_ON, .cvalue = &.{c.WC_OPT_DETECT_ON + '0'}, .text = "ON" },
};

const graphic_char_str = [_]sel_c{
    .{ .value = c.GRAPHIC_CHAR_ASCII, .cvalue = &.{c.GRAPHIC_CHAR_ASCII + '0'}, .text = "ASCII" },
    .{ .value = c.GRAPHIC_CHAR_CHARSET, .cvalue = &.{c.GRAPHIC_CHAR_CHARSET + '0'}, .text = "charset specific" },
    .{ .value = c.GRAPHIC_CHAR_DEC, .cvalue = &.{c.GRAPHIC_CHAR_DEC + '0'}, .text = "DEC special graphics" },
};

const inlineimgstr = [_]sel_c{
    .{ .value = c.INLINE_IMG_NONE, .cvalue = &.{c.INLINE_IMG_NONE + '0'}, .text = "external command" },
    .{ .value = c.INLINE_IMG_OSC5379, .cvalue = &.{c.INLINE_IMG_OSC5379 + '0'}, .text = "OSC 5379 (mlterm)" },
    .{ .value = c.INLINE_IMG_SIXEL, .cvalue = &.{c.INLINE_IMG_SIXEL + '0'}, .text = "sixel (img2sixel)" },
    .{ .value = c.INLINE_IMG_ITERM2, .cvalue = &.{c.INLINE_IMG_ITERM2 + '0'}, .text = "OSC 1337 (iTerm2)" },
    .{ .value = c.INLINE_IMG_KITTY, .cvalue = &.{c.INLINE_IMG_KITTY + '0'}, .text = "kitty (ImageMagick)" },
};

// #define CMT_HELPER N_("External Viewer Setup")
const CMT_TABSTOP = "Tab width in characters";
const CMT_INDENT_INCR = "Indent for HTML rendering";
const CMT_PIXEL_PER_CHAR = "Number of pixels per character (4.0...32.0)";
const CMT_PIXEL_PER_LINE = "Number of pixels per line (4.0...64.0)";
const CMT_PAGERLINE = "Number of remembered lines when used as a pager";
const CMT_HISTORY = "Use URL history";
const CMT_HISTSIZE = "Number of remembered URL";
const CMT_SAVEHIST = "Save URL history";
const CMT_FRAME = "Render frames automatically";
const CMT_ARGV_IS_URL = "Treat argument without scheme as URL";
const CMT_TSELF = "Use _self as default target";
const CMT_OPEN_TAB_BLANK = "Open link on new tab if target is _blank or _new";
const CMT_OPEN_TAB_DL_LIST = "Open download list panel on new tab";
const CMT_DISPLINK = "Display link URL automatically";
const CMT_DISPLINKNUMBER = "Display link numbers";
const CMT_DECODE_URL = "Display decoded URL";
const CMT_DISPLINEINFO = "Display current line number";
const CMT_DISP_IMAGE = "Display inline images";
const CMT_PSEUDO_INLINES = "Display pseudo-ALTs for inline images with no ALT or TITLE string";
const CMT_AUTO_IMAGE = "Load inline images automatically";
const CMT_MAX_LOAD_IMAGE = "Maximum processes for parallel image loading";
const CMT_EXT_IMAGE_VIEWER = "Use external image viewer";
const CMT_IMAGE_SCALE = "Scale of image (%)";
const CMT_IMGDISPLAY = "External command to display image";
const CMT_IMAGE_MAP_LIST = "Use link list of image map";
const CMT_INLINE_IMG_PROTOCOL = "Inline image display method";
const CMT_MULTICOL = "Display file names in multi-column format";
const CMT_ALT_ENTITY = "Use ASCII equivalents to display entities";
const CMT_GRAPHIC_CHAR = "Character type for border of table and menu";
const CMT_DISP_BORDERS = "Display table borders, ignore value of BORDER";
const CMT_DISABLE_CENTER = "Disable center alignment";
const CMT_FOLD_TEXTAREA = "Fold lines in TEXTAREA";
const CMT_DISP_INS_DEL = "Display INS, DEL, S and STRIKE element";
const CMT_COLOR = "Display with color";
const CMT_HINTENSITY_COLOR = "Use high-intensity colors";
const CMT_B_COLOR = "Color of normal character";
const CMT_A_COLOR = "Color of anchor";
const CMT_I_COLOR = "Color of image link";
const CMT_F_COLOR = "Color of form";
const CMT_ACTIVE_STYLE = "Enable coloring of active link";
const CMT_C_COLOR = "Color of currently active link";
const CMT_VISITED_ANCHOR = "Use visited link color";
const CMT_V_COLOR = "Color of visited link";
const CMT_BG_COLOR = "Color of background";
const CMT_MARK_COLOR = "Color of mark";
const CMT_USE_PROXY = "Use proxy";
const CMT_HTTP_PROXY = "URL of HTTP proxy host";
const CMT_HTTPS_PROXY = "URL of HTTPS proxy host";
const CMT_GOPHER_PROXY = "URL of GOPHER proxy host";
const CMT_FTP_PROXY = "URL of FTP proxy host";
const CMT_NO_PROXY = "Domains to be accessed directly (no proxy)";
const CMT_NOPROXY_NETADDR = "Check noproxy by network address";
const CMT_NO_CACHE = "Disable cache";
const CMT_NNTP_SERVER = "News server";
const CMT_NNTP_MODE = "Mode of news server";
const CMT_MAX_NEWS = "Number of news messages";
const CMT_DNS_ORDER = "Order of name resolution";
const CMT_DROOT = "Directory corresponding to / (document root)";
const CMT_PDROOT = "Directory corresponding to /~user";
const CMT_CGIBIN = "Directory corresponding to /cgi-bin";
const CMT_TMP = "Directory for temporary files";
const CMT_CONFIRM_QQ = "Confirm when quitting with q";
const CMT_CLOSE_TAB_BACK = "Close tab if buffer is last when back";
const CMT_USE_MARK = "Enable mark operations";
const CMT_EMACS_LIKE_LINEEDIT = "Enable Emacs-style line editing";
const CMT_SPACE_AUTOCOMPLETE = "Space key triggers file completion while editing URLs";
const CMT_VI_PREC_NUM = "Enable vi-like numeric prefix";
const CMT_LABEL_TOPLINE = "Move cursor to top line when going to label";
const CMT_NEXTPAGE_TOPLINE = "Move cursor to top line when moving to next page";
const CMT_FOLD_LINE = "Fold lines of plain text file";
const CMT_SHOW_NUM = "Show line numbers";
const CMT_SHOW_SRCH_STR = "Show search string";
const CMT_MIMETYPES = "List of mime.types files";
const CMT_MAILCAP = "List of mailcap files";
const CMT_URIMETHODMAP = "List of urimethodmap files";
const CMT_EDITOR = "Editor";
const CMT_MAILER = "Mailer";
const CMT_MAILTO_OPTIONS = "How to call Mailer for mailto URLs with options";
const CMT_EXTBRZ = "External browser";
const CMT_EXTBRZ2 = "2nd external browser";
const CMT_EXTBRZ3 = "3rd external browser";
const CMT_EXTBRZ4 = "4th external browser";
const CMT_EXTBRZ5 = "5th external browser";
const CMT_EXTBRZ6 = "6th external browser";
const CMT_EXTBRZ7 = "7th external browser";
const CMT_EXTBRZ8 = "8th external browser";
const CMT_EXTBRZ9 = "9th external browser";
const CMT_DISABLE_SECRET_SECURITY_CHECK = "Disable secret file security check";
const CMT_PASSWDFILE = "Password file";
const CMT_PRE_FORM_FILE = "File for setting form on loading";
const CMT_SITECONF_FILE = "File for preferences for each site";
const CMT_FTPPASS = "Password for anonymous FTP (your mail address)";
const CMT_FTPPASS_HOSTNAMEGEN = "Generate domain part of password for FTP";
const CMT_USERAGENT = "User-Agent identification string";
const CMT_ACCEPTENCODING = "Accept-Encoding header";
const CMT_ACCEPTMEDIA = "Accept header";
const CMT_ACCEPTLANG = "Accept-Language header";
const CMT_MARK_ALL_PAGES = "Treat URL-like strings as links in all pages";
const CMT_WRAP = "Wrap search";
const CMT_VIEW_UNSEENOBJECTS = "Display unseen objects (e.g. bgimage tag)";
const CMT_AUTO_UNCOMPRESS = "Uncompress compressed data automatically when downloading";
const CMT_BGEXTVIEW = "Run external viewer in the background";
const CMT_EXT_DIRLIST = "Use external program for directory listing";
const CMT_DIRLIST_CMD = "URL of directory listing command";
const CMT_USE_DICTCOMMAND = "Enable dictionary lookup through CGI";
const CMT_DICTCOMMAND = "URL of dictionary lookup command";
const CMT_IGNORE_NULL_IMG_ALT = "Display link name for images lacking ALT";
const CMT_IFILE = "Index file for directories";
const CMT_RETRY_HTTP = "Prepend http:// to URL automatically";
const CMT_DEFAULT_URL = "Default value for open-URL command";
const CMT_DECODE_CTE = "Decode Content-Transfer-Encoding when saving";
const CMT_PRESERVE_TIMESTAMP = "Preserve timestamp when saving";
const CMT_MOUSE = "Enable mouse";
const CMT_REVERSE_MOUSE = "Scroll in reverse direction of mouse drag";
const CMT_RELATIVE_WHEEL_SCROLL = "Behavior of wheel scroll speed";
const CMT_RELATIVE_WHEEL_SCROLL_RATIO = "(A only)Scroll by # (%) of screen";
const CMT_FIXED_WHEEL_SCROLL_COUNT = "(B only)Scroll by # lines";
const CMT_CLEAR_BUF = "Free memory of undisplayed buffers";
const CMT_NOSENDREFERER = "Suppress `Referer:' header";
const CMT_CROSSORIGINREFERER = "Exclude pathname and query string from `Referer:' header when cross domain communication";
const CMT_IGNORE_CASE = "Search case-insensitively";
const CMT_USE_LESSOPEN = "Use LESSOPEN";
const CMT_SSL_VERIFY_SERVER = "Perform SSL server verification";
const CMT_SSL_CERT_FILE = "PEM encoded certificate file of client";
const CMT_SSL_KEY_FILE = "PEM encoded private key file of client";
const CMT_SSL_CA_PATH = "Path to directory for PEM encoded certificates of CAs";
const CMT_SSL_CA_FILE = "File consisting of PEM encoded certificates of CAs";
const CMT_SSL_CA_DEFAULT = "Use default locations for PEM encoded certificates of CAs";
const CMT_SSL_FORBID_METHOD = "List of forbidden SSL methods (2: SSLv2, 3: SSLv3, t: TLSv1.0, 5: TLSv1.1, 6: TLSv1.2, 7: TLSv1.3)";
// #ifdef SSL_CTX_set_min_proto_version
const CMT_SSL_MIN_VERSION = "Minimum SSL version (all, TLSv1.0, TLSv1.1, TLSv1.2, or TLSv1.3)";
// #endif
const CMT_SSL_CIPHER = "SSL ciphers for TLSv1.2 and below (e.g. DEFAULT:@SECLEVEL=2)";
const CMT_USECOOKIE = "Enable cookie processing";
const CMT_SHOWCOOKIE = "Print a message when receiving a cookie";
const CMT_ACCEPTCOOKIE = "Accept cookies";
const CMT_ACCEPTBADCOOKIE = "Action to be taken on invalid cookie";
const CMT_COOKIE_REJECT_DOMAINS = "Domains to reject cookies from";
const CMT_COOKIE_ACCEPT_DOMAINS = "Domains to accept cookies from";
const CMT_COOKIE_AVOID_WONG_NUMBER_OF_DOTS = "Domains to avoid [wrong number of dots]";
const CMT_FOLLOW_REDIRECTION = "Number of redirections to follow";
const CMT_META_REFRESH = "Enable processing of meta-refresh tag";
const CMT_LOCALHOST_ONLY = "Restrict connections only to localhost";
//
const CMT_DISPLAY_CHARSET = "Display charset";
const CMT_DOCUMENT_CHARSET = "Default document charset";
const CMT_AUTO_DETECT = "Automatic charset detection when loading";
const CMT_SYSTEM_CHARSET = "System charset";
const CMT_FOLLOW_LOCALE = "System charset follows locale(LC_CTYPE)";
const CMT_EXT_HALFDUMP = "Output halfdump with display charset";
const CMT_USE_WIDE = "Use multi-column characters";
const CMT_USE_COMBINING = "Use combining characters";
const CMT_EAST_ASIAN_WIDTH = "Use double width for some Unicode characters";
const CMT_USE_LANGUAGE_TAG = "Use Unicode language tags";
const CMT_UCS_CONV = "Charset conversion using Unicode map";
const CMT_PRE_CONV = "Charset conversion when loading";
const CMT_SEARCH_CONV = "Adjust search string for document charset";
const CMT_FIX_WIDTH_CONV = "Fix character width when converting";
const CMT_USE_GB12345_MAP = "Use GB 12345 Unicode map instead of GB 2312's";
const CMT_USE_JISX0201 = "Use JIS X 0201 Roman for ISO-2022-JP";
const CMT_USE_JISC6226 = "Use JIS C 6226:1978 for ISO-2022-JP";
const CMT_USE_JISX0201K = "Use JIS X 0201 Katakana";
const CMT_USE_JISX0212 = "Use JIS X 0212:1990 (Supplemental Kanji)";
const CMT_USE_JISX0213 = "Use JIS X 0213:2000 (2000JIS)";
const CMT_STRICT_ISO2022 = "Strict ISO-2022-JP/KR/CN";
const CMT_GB18030_AS_UCS = "Treat 4 bytes char. of GB18030 as Unicode";
const CMT_SIMPLE_PRESERVE_SPACE = "Simple Preserve space";

const CMT_KEYMAP_FILE = "keymap file";

const params1 = [_]param_ptr{
    .{ .name = "tabstop", .type = .P_NZINT, .inputtype = .PI_TEXT, .varptr = &c.Tabstop, .comment = CMT_TABSTOP },
    .{ .name = "indent_incr", .type = .P_NZINT, .inputtype = .PI_TEXT, .varptr = &c.IndentIncr, .comment = CMT_INDENT_INCR },
    .{ .name = "pixel_per_char", .type = .P_PIXELS, .inputtype = .PI_TEXT, .varptr = &c.pixel_per_char, .comment = CMT_PIXEL_PER_CHAR },
    .{ .name = "pixel_per_line", .type = .P_PIXELS, .inputtype = .PI_TEXT, .varptr = &c.pixel_per_line, .comment = CMT_PIXEL_PER_LINE },
    .{ .name = "frame", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.RenderFrame, .comment = CMT_FRAME },
    .{ .name = "target_self", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.TargetSelf, .comment = CMT_TSELF },
    .{ .name = "open_tab_blank", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.open_tab_blank, .comment = CMT_OPEN_TAB_BLANK },
    .{ .name = "open_tab_dl_list", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.open_tab_dl_list, .comment = CMT_OPEN_TAB_DL_LIST },
    .{ .name = "display_link", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.displayLink, .comment = CMT_DISPLINK },
    .{ .name = "display_link_number", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.displayLinkNumber, .comment = CMT_DISPLINKNUMBER },
    .{ .name = "decode_url", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.DecodeURL, .comment = CMT_DECODE_URL },

    .{ .name = "display_lineinfo", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.displayLineInfo, .comment = CMT_DISPLINEINFO },
    .{ .name = "ext_dirlist", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.UseExternalDirBuffer, .comment = CMT_EXT_DIRLIST },
    .{ .name = "dirlist_cmd", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.DirBufferCommand), .comment = CMT_DIRLIST_CMD },
    .{ .name = "use_dictcommand", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.UseDictCommand, .comment = CMT_USE_DICTCOMMAND },
    .{ .name = "dictcommand", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.DictCommand), .comment = CMT_DICTCOMMAND },
    .{ .name = "multicol", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.multicolList, .comment = CMT_MULTICOL },
    .{ .name = "alt_entity", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.UseAltEntity, .comment = CMT_ALT_ENTITY },
    .{ .name = "graphic_char", .type = .P_CHARINT, .inputtype = .PI_SEL_C, .varptr = &c.UseGraphicChar, .comment = CMT_GRAPHIC_CHAR, .select = &graphic_char_str },
    .{ .name = "display_borders", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.DisplayBorders, .comment = CMT_DISP_BORDERS },
    .{ .name = "disable_center", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.DisableCenter, .comment = CMT_DISABLE_CENTER },

    .{ .name = "fold_textarea", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.FoldTextarea, .comment = CMT_FOLD_TEXTAREA },
    .{ .name = "display_ins_del", .type = .P_INT, .inputtype = .PI_SEL_C, .varptr = &c.displayInsDel, .comment = CMT_DISP_INS_DEL, .select = &displayinsdel },
    .{ .name = "ignore_null_img_alt", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.ignore_null_img_alt, .comment = CMT_IGNORE_NULL_IMG_ALT },
    .{ .name = "view_unseenobject", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.view_unseenobject, .comment = CMT_VIEW_UNSEENOBJECTS },
    .{ .name = "display_image", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.displayImage, .comment = CMT_DISP_IMAGE },
    .{ .name = "pseudo_inlines", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.pseudoInlines, .comment = CMT_PSEUDO_INLINES },
    .{ .name = "auto_image", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.autoImage, .comment = CMT_AUTO_IMAGE },
    .{ .name = "max_load_image", .type = .P_INT, .inputtype = .PI_TEXT, .varptr = &c.maxLoadImage, .comment = CMT_MAX_LOAD_IMAGE },
    .{ .name = "ext_image_viewer", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.useExtImageViewer, .comment = CMT_EXT_IMAGE_VIEWER },
    .{ .name = "image_scale", .type = .P_SCALE, .inputtype = .PI_TEXT, .varptr = &c.image_scale, .comment = CMT_IMAGE_SCALE },

    .{ .name = "inline_img_protocol", .type = .P_INT, .inputtype = .PI_SEL_C, .varptr = &c.enable_inline_image, .comment = CMT_INLINE_IMG_PROTOCOL, .select = &inlineimgstr },
    .{ .name = "imgdisplay", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.Imgdisplay), .comment = CMT_IMGDISPLAY },
    .{ .name = "image_map_list", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.image_map_list, .comment = CMT_IMAGE_MAP_LIST },
    .{ .name = "fold_line", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.FoldLine, .comment = CMT_FOLD_LINE },
    .{ .name = "show_lnum", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.showLineNum, .comment = CMT_SHOW_NUM },
    .{ .name = "show_srch_str", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.show_srch_str, .comment = CMT_SHOW_SRCH_STR },
    .{ .name = "label_topline", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.label_topline, .comment = CMT_LABEL_TOPLINE },
    .{ .name = "nextpage_topline", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.nextpage_topline, .comment = CMT_NEXTPAGE_TOPLINE },
};

const params2 = [_]param_ptr{
    .{ .name = "color", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.useColor, .comment = CMT_COLOR },
    .{ .name = "high-intensity", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.highIntensityColors, .comment = CMT_HINTENSITY_COLOR },
    .{ .name = "basic_color", .type = .P_COLOR, .inputtype = .PI_SEL_C, .varptr = &c.basic_color, .comment = CMT_B_COLOR, .select = &colorstr },
    .{ .name = "anchor_color", .type = .P_COLOR, .inputtype = .PI_SEL_C, .varptr = &c.anchor_color, .comment = CMT_A_COLOR, .select = &colorstr },
    .{ .name = "image_color", .type = .P_COLOR, .inputtype = .PI_SEL_C, .varptr = &c.image_color, .comment = CMT_I_COLOR, .select = &colorstr },
    .{ .name = "form_color", .type = .P_COLOR, .inputtype = .PI_SEL_C, .varptr = &c.form_color, .comment = CMT_F_COLOR, .select = &colorstr },
    .{ .name = "mark_color", .type = .P_COLOR, .inputtype = .PI_SEL_C, .varptr = &c.mark_color, .comment = CMT_MARK_COLOR, .select = &colorstr },
    .{ .name = "bg_color", .type = .P_COLOR, .inputtype = .PI_SEL_C, .varptr = &c.bg_color, .comment = CMT_BG_COLOR, .select = &colorstr },
    .{ .name = "active_style", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.useActiveColor, .comment = CMT_ACTIVE_STYLE },
    .{ .name = "active_color", .type = .P_COLOR, .inputtype = .PI_SEL_C, .varptr = &c.active_color, .comment = CMT_C_COLOR, .select = &colorstr },
    .{ .name = "visited_anchor", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.useVisitedColor, .comment = CMT_VISITED_ANCHOR },
    .{ .name = "visited_color", .type = .P_COLOR, .inputtype = .PI_SEL_C, .varptr = &c.visited_color, .comment = CMT_V_COLOR, .select = &colorstr },
};

const params3 = [_]param_ptr{
    .{ .name = "pagerline", .type = .P_NZINT, .inputtype = .PI_TEXT, .varptr = &c.PagerMax, .comment = CMT_PAGERLINE },
    .{ .name = "use_history", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.UseHistory, .comment = CMT_HISTORY },
    .{ .name = "history", .type = .P_INT, .inputtype = .PI_TEXT, .varptr = &c.URLHistSize, .comment = CMT_HISTSIZE },
    .{ .name = "save_hist", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.SaveURLHist, .comment = CMT_SAVEHIST },
    .{ .name = "confirm_qq", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.confirm_on_quit, .comment = CMT_CONFIRM_QQ },
    .{ .name = "close_tab_back", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.close_tab_back, .comment = CMT_CLOSE_TAB_BACK },
    .{ .name = "mark", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.use_mark, .comment = CMT_USE_MARK },
    .{ .name = "emacs_like_lineedit", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.emacs_like_lineedit, .comment = CMT_EMACS_LIKE_LINEEDIT },
    .{ .name = "space_autocomplete", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.space_autocomplete, .comment = CMT_SPACE_AUTOCOMPLETE },
    .{ .name = "vi_prec_num", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.vi_prec_num, .comment = CMT_VI_PREC_NUM },
    .{ .name = "mark_all_pages", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.MarkAllPages, .comment = CMT_MARK_ALL_PAGES },
    .{ .name = "wrap_search", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.WrapDefault, .comment = CMT_WRAP },
    .{ .name = "ignorecase_search", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.IgnoreCase, .comment = CMT_IGNORE_CASE },
    .{ .name = "relative_wheel_scroll", .type = .P_INT, .inputtype = .PI_SEL_C, .varptr = &c.relative_wheel_scroll, .comment = CMT_RELATIVE_WHEEL_SCROLL, .select = &wheelmode },
    .{ .name = "relative_wheel_scroll_ratio", .type = .P_INT, .inputtype = .PI_TEXT, .varptr = &c.relative_wheel_scroll_ratio, .comment = CMT_RELATIVE_WHEEL_SCROLL_RATIO },
    .{ .name = "fixed_wheel_scroll_count", .type = .P_INT, .inputtype = .PI_TEXT, .varptr = &c.fixed_wheel_scroll_count, .comment = CMT_FIXED_WHEEL_SCROLL_COUNT },
    .{ .name = "clear_buffer", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.clear_buffer, .comment = CMT_CLEAR_BUF },
    .{ .name = "decode_cte", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.DecodeCTE, .comment = CMT_DECODE_CTE },
    .{ .name = "auto_uncompress", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.AutoUncompress, .comment = CMT_AUTO_UNCOMPRESS },
    .{ .name = "preserve_timestamp", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.PreserveTimestamp, .comment = CMT_PRESERVE_TIMESTAMP },
    .{ .name = "keymap_file", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.keymap_file), .comment = CMT_KEYMAP_FILE },
};

const params4 = [_]param_ptr{
    .{ .name = "use_proxy", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.use_proxy, .comment = CMT_USE_PROXY },
    .{ .name = "http_proxy", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.HTTP_proxy), .comment = CMT_HTTP_PROXY },
    .{ .name = "https_proxy", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.HTTPS_proxy), .comment = CMT_HTTPS_PROXY },
    .{ .name = "gopher_proxy", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.GOPHER_proxy), .comment = CMT_GOPHER_PROXY },
    .{ .name = "ftp_proxy", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.FTP_proxy), .comment = CMT_FTP_PROXY },
    .{ .name = "no_proxy", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.NO_proxy), .comment = CMT_NO_PROXY },
    .{ .name = "noproxy_netaddr", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.NOproxy_netaddr, .comment = CMT_NOPROXY_NETADDR },
    .{ .name = "no_cache", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.NoCache, .comment = CMT_NO_CACHE },
};

const params5 = [_]param_ptr{
    .{ .name = "document_root", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.w3m_config.document_root), .comment = CMT_DROOT },
    .{ .name = "personal_document_root", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.w3m_config.personal_document_root), .comment = CMT_PDROOT },
    .{ .name = "cgi_bin", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.w3m_config.cgi_bin), .comment = CMT_CGIBIN },
    .{ .name = "index_file", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.w3m_config.index_file), .comment = CMT_IFILE },
    .{ .name = "tmp_dir", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.w3m_config.param_tmp_dir), .comment = CMT_TMP },
};

const params6 = [_]param_ptr{
    .{ .name = "mime_types", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.mimetypes_files), .comment = CMT_MIMETYPES },
    .{ .name = "mailcap", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.mailcap_files), .comment = CMT_MAILCAP },
    .{ .name = "editor", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.Editor), .comment = CMT_EDITOR },
    .{ .name = "mailer", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.Mailer), .comment = CMT_MAILER },
    .{ .name = "extbrowser", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.ExtBrowser), .comment = CMT_EXTBRZ },
    .{ .name = "extbrowser2", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.ExtBrowser2), .comment = CMT_EXTBRZ2 },
    .{ .name = "extbrowser3", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.ExtBrowser3), .comment = CMT_EXTBRZ3 },
    .{ .name = "extbrowser4", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.ExtBrowser4), .comment = CMT_EXTBRZ4 },
    .{ .name = "extbrowser5", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.ExtBrowser5), .comment = CMT_EXTBRZ5 },
    .{ .name = "extbrowser6", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.ExtBrowser6), .comment = CMT_EXTBRZ6 },
    .{ .name = "extbrowser7", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.ExtBrowser7), .comment = CMT_EXTBRZ7 },
    .{ .name = "extbrowser8", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.ExtBrowser8), .comment = CMT_EXTBRZ8 },
    .{ .name = "extbrowser9", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.ExtBrowser9), .comment = CMT_EXTBRZ9 },
    .{ .name = "bgextviewer", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.BackgroundExtViewer, .comment = CMT_BGEXTVIEW },
    .{ .name = "use_lessopen", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.use_lessopen, .comment = CMT_USE_LESSOPEN },
};

const params7 = [_]param_ptr{
    .{ .name = "ssl_forbid_method", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.ssl_forbid_method), .comment = CMT_SSL_FORBID_METHOD },
    // #ifdef SSL_CTX_set_min_proto_version
    .{ .name = "ssl_min_version", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.ssl_min_version), .comment = CMT_SSL_MIN_VERSION },
    // #endif
    .{ .name = "ssl_cipher", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.ssl_cipher), .comment = CMT_SSL_CIPHER },
    .{ .name = "ssl_verify_server", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.ssl_verify_server, .comment = CMT_SSL_VERIFY_SERVER },
    .{ .name = "ssl_cert_file", .type = .P_SSLPATH, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.ssl_cert_file), .comment = CMT_SSL_CERT_FILE },
    .{ .name = "ssl_key_file", .type = .P_SSLPATH, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.ssl_key_file), .comment = CMT_SSL_KEY_FILE },
    .{ .name = "ssl_ca_path", .type = .P_SSLPATH, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.ssl_ca_path), .comment = CMT_SSL_CA_PATH },
    .{ .name = "ssl_ca_file", .type = .P_SSLPATH, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.ssl_ca_file), .comment = CMT_SSL_CA_FILE },
    .{ .name = "ssl_ca_default", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.ssl_ca_default, .comment = CMT_SSL_CA_DEFAULT },
};

const params8 = [_]param_ptr{
    .{ .name = "use_cookie", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.use_cookie, .comment = CMT_USECOOKIE },
    .{ .name = "show_cookie", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.show_cookie, .comment = CMT_SHOWCOOKIE },
    .{ .name = "accept_cookie", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.accept_cookie, .comment = CMT_ACCEPTCOOKIE },
    .{ .name = "accept_bad_cookie", .type = .P_INT, .inputtype = .PI_SEL_C, .varptr = &c.accept_bad_cookie, .comment = CMT_ACCEPTBADCOOKIE, .select = &badcookiestr },
    .{ .name = "cookie_reject_domains", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.cookie_reject_domains), .comment = CMT_COOKIE_REJECT_DOMAINS },
    .{ .name = "cookie_accept_domains", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.cookie_accept_domains), .comment = CMT_COOKIE_ACCEPT_DOMAINS },
    .{ .name = "cookie_avoid_wrong_number_of_dots", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.cookie_avoid_wrong_number_of_dots), .comment = CMT_COOKIE_AVOID_WONG_NUMBER_OF_DOTS },
};

const params9 = [_]param_ptr{
    .{ .name = "passwd_file", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.passwd_file), .comment = CMT_PASSWDFILE },
    .{ .name = "disable_secret_security_check", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.disable_secret_security_check, .comment = CMT_DISABLE_SECRET_SECURITY_CHECK },
    .{ .name = "ftppasswd", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.ftppasswd), .comment = CMT_FTPPASS },
    .{ .name = "ftppass_hostnamegen", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.ftppass_hostnamegen, .comment = CMT_FTPPASS_HOSTNAMEGEN },
    .{ .name = "pre_form_file", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.pre_form_file), .comment = CMT_PRE_FORM_FILE },
    .{ .name = "user_agent", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.UserAgent), .comment = CMT_USERAGENT },
    .{ .name = "no_referer", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.NoSendReferer, .comment = CMT_NOSENDREFERER },
    .{ .name = "cross_origin_referer", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.CrossOriginReferer, .comment = CMT_CROSSORIGINREFERER },
    .{ .name = "accept_language", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.AcceptLang), .comment = CMT_ACCEPTLANG },
    .{ .name = "accept_encoding", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.AcceptEncoding), .comment = CMT_ACCEPTENCODING },
    .{ .name = "accept_media", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.AcceptMedia), .comment = CMT_ACCEPTMEDIA },
    .{ .name = "argv_is_url", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.ArgvIsURL, .comment = CMT_ARGV_IS_URL },
    .{ .name = "retry_http", .type = .P_INT, .inputtype = .PI_ONOFF, .varptr = &c.retryAsHttp, .comment = CMT_RETRY_HTTP },
    .{ .name = "default_url", .type = .P_INT, .inputtype = .PI_SEL_C, .varptr = &c.DefaultURLString, .comment = CMT_DEFAULT_URL, .select = &defaulturls },
    .{ .name = "follow_redirection", .type = .P_INT, .inputtype = .PI_TEXT, .varptr = &c.FollowRedirection, .comment = CMT_FOLLOW_REDIRECTION },
    .{ .name = "meta_refresh", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.MetaRefresh, .comment = CMT_META_REFRESH },
    .{ .name = "localhost_only", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.LocalhostOnly, .comment = CMT_LOCALHOST_ONLY },
    .{ .name = "dns_order", .type = .P_INT, .inputtype = .PI_SEL_C, .varptr = &c.DNS_order, .comment = CMT_DNS_ORDER, .select = &dnsorders },
    .{ .name = "nntpserver", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.NNTP_server), .comment = CMT_NNTP_SERVER },
    .{ .name = "nntpmode", .type = .P_STRING, .inputtype = .PI_TEXT, .varptr = @ptrCast(&c.NNTP_mode), .comment = CMT_NNTP_MODE },
    .{ .name = "max_news", .type = .P_INT, .inputtype = .PI_TEXT, .varptr = &c.MaxNewsMessage, .comment = CMT_MAX_NEWS },
};

const params10 = [_]param_ptr{
    .{ .name = "display_charset", .type = .P_CODE, .inputtype = .PI_CODE, .varptr = &c.DisplayCharset, .comment = CMT_DISPLAY_CHARSET, .ces_list = &display_charset_str },
    .{ .name = "document_charset", .type = .P_CODE, .inputtype = .PI_CODE, .varptr = &c.DocumentCharset, .comment = CMT_DOCUMENT_CHARSET, .ces_list = &document_charset_str },
    .{ .name = "auto_detect", .type = .P_CHARINT, .inputtype = .PI_SEL_C, .varptr = &c.WcOption.auto_detect, .comment = CMT_AUTO_DETECT, .select = &auto_detect_str },
    .{ .name = "system_charset", .type = .P_CODE, .inputtype = .PI_CODE, .varptr = &c.SystemCharset, .comment = CMT_SYSTEM_CHARSET, .ces_list = &system_charset_str },
    .{ .name = "follow_locale", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.FollowLocale, .comment = CMT_FOLLOW_LOCALE },
    .{ .name = "use_wide", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.WcOption.use_wide, .comment = CMT_USE_WIDE },
    .{ .name = "use_combining", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.WcOption.use_combining, .comment = CMT_USE_COMBINING },
    .{ .name = "east_asian_width", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.WcOption.east_asian_width, .comment = CMT_EAST_ASIAN_WIDTH },
    .{ .name = "use_language_tag", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.WcOption.use_language_tag, .comment = CMT_USE_LANGUAGE_TAG },
    .{ .name = "ucs_conv", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.WcOption.ucs_conv, .comment = CMT_UCS_CONV },
    .{ .name = "pre_conv", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.WcOption.pre_conv, .comment = CMT_PRE_CONV },
    .{ .name = "search_conv", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.SearchConv, .comment = CMT_SEARCH_CONV },
    .{ .name = "fix_width_conv", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.WcOption.fix_width_conv, .comment = CMT_FIX_WIDTH_CONV },
    .{ .name = "use_gb12345_map", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.WcOption.use_gb12345_map, .comment = CMT_USE_GB12345_MAP },
    .{ .name = "use_jisx0201", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.WcOption.use_jisx0201, .comment = CMT_USE_JISX0201 },
    .{ .name = "use_jisc6226", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.WcOption.use_jisc6226, .comment = CMT_USE_JISC6226 },
    .{ .name = "use_jisx0201k", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.WcOption.use_jisx0201k, .comment = CMT_USE_JISX0201K },
    .{ .name = "use_jisx0212", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.WcOption.use_jisx0212, .comment = CMT_USE_JISX0212 },
    .{ .name = "use_jisx0213", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.WcOption.use_jisx0213, .comment = CMT_USE_JISX0213 },
    .{ .name = "strict_iso2022", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.WcOption.strict_iso2022, .comment = CMT_STRICT_ISO2022 },
    .{ .name = "gb18030_as_ucs", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.WcOption.gb18030_as_ucs, .comment = CMT_GB18030_AS_UCS },
    .{ .name = "simple_preserve_space", .type = .P_CHARINT, .inputtype = .PI_ONOFF, .varptr = &c.SimplePreserveSpace, .comment = CMT_SIMPLE_PRESERVE_SPACE },
};

const sections = [_]param_section{
    .{ .name = "Display Settings", .params = &params1 },
    .{ .name = "Color Settings", .params = &params2 },
    .{ .name = "Miscellaneous Settings", .params = &params3 },
    .{ .name = "Directory Settings", .params = &params5 },
    .{ .name = "External Program Settings", .params = &params6 },
    .{ .name = "Network Settings", .params = &params9 },
    .{ .name = "Proxy Settings", .params = &params4 },
    .{ .name = "SSL Settings", .params = &params7 },
    .{ .name = "Cookie Settings", .params = &params8 },
    .{ .name = "Charset Settings", .params = &params10 },
};

fn setVal(T: type, p: *const param_ptr, val: T) void {
    const ptr: *T = @ptrCast(@alignCast(p.*.varptr));
    ptr.* = val;
}

fn getVal(T: type, p: *const param_ptr) T {
    const ptr: *T = @ptrCast(@alignCast(p.*.varptr));
    return ptr.*;
}

fn getBool(T: type, p: *const param_ptr) bool {
    const val = getVal(T, p);
    return val != 0;
}

const ParamIterator = struct {
    params: []const param_ptr,
    pos: usize = 0,

    fn next(this: *@This()) ?*const param_ptr {
        if (this.pos >= this.params.len) {
            return null;
        }
        defer this.pos += 1;
        return &this.params[this.pos];
    }
};

const SectionIterator = struct {
    sections: []const param_section,
    pos: usize = 0,

    fn next(this: *@This()) ?*const param_section {
        if (this.pos >= this.sections.len) {
            return null;
        }
        defer this.pos += 1;
        return &this.sections[this.pos];
    }
};

const SelectIterator = struct {
    select: []const sel_c,
    pos: usize = 0,

    fn next(this: *@This()) ?*const sel_c {
        if (this.pos >= this.select.len) {
            return null;
        }
        defer this.pos += 1;
        return &this.select[this.pos];
    }
};

const CesListIterator = struct {
    list: [*]c.wc_ces_list,
    pos: usize = 0,

    fn next(this: *@This()) ?*c.wc_ces_list {
        if (this.list[this.pos].desc == null) {
            return null;
        }
        defer this.pos += 1;
        return &this.list[this.pos];
    }
};

var RC_search_table: ?std.StringHashMap(*const param_ptr) = null;

fn make_rc_table() !void {
    if (display_charset_str == null) {
        display_charset_str = c.wc_get_ces_list();
        document_charset_str = display_charset_str;
        system_charset_str = display_charset_str;
    }

    var map = std.StringHashMap(*const param_ptr).init(gcstr.GcAllocator.allocator());
    defer RC_search_table = map;
    var sit = SectionIterator{
        .sections = &sections,
    };
    while (sit.next()) |section| {
        var pit = ParamIterator{
            .params = section.*.params,
        };
        while (pit.next()) |p| {
            try map.put(p.name, p);
        }
    }
}

fn config_search_param(_name: [*c]const u8) ?*const param_ptr {
    if (_name) |name| {
        if (RC_search_table) |table| {
            const span = std.mem.span(name);
            return table.get(span);
        } else {
            @panic("RC_search_table not initialized");
        }
    } else {
        return null;
    }
}

export fn config_initialize() void {
    make_rc_table() catch @panic("config_make_rc_table");
}

const W3MHELPERPANEL_CMDNAME = "w3mhelperpanel";

const optionpanel_src1 = ("<html><head><title>Option Setting Panel</title></head><body>" //
    ++ "<h1 align=center>Option Setting Panel<br>(w3m version {s})</b></h1>" //
    ++ "<form method=post action=\"file:///$LIB/" ++ W3MHELPERPANEL_CMDNAME ++ "\">" //
    ++ "<input type=hidden name=mode value=panel>" //
    ++ "<input type=hidden name=cookie value=\"{s}\">" //
    ++ "<input type=submit value=\"{s}\">" //
    ++ "</form><br>" //
    ++ "<form method=internal action=option>" //
);

fn write_config_panel_html(writer: *std.Io.Writer) !void {
    try writer.print(optionpanel_src1, .{
        c.w3m_version,
        gcstr.c.html_quote(c.localCookie().*.ptr),
        "External Viewer Setup",
    });

    try writer.writeAll("<table><tr><td>");

    var sit = SectionIterator{
        .sections = &sections,
    };
    while (sit.next()) |section| {
        try writer.print("<h1>{s}</h1>", .{section.name});
        try writer.writeAll("<table width=100% cellpadding=0>");

        var pit = ParamIterator{
            .params = section.params,
        };
        while (pit.next()) |p| {
            try writer.print("<tr><td>{s}</td><td width={}>", .{
                p.comment,
                @floor(28 * c.pixel_per_char),
            });
            const str = std.mem.span(to_str(p).*.ptr);
            switch (p.inputtype) {
                .PI_TEXT => {
                    try writer.print("<input type=text name={s} value=\"{s}\">", .{
                        p.name,
                        c.html_quote(str),
                    });
                },
                .PI_ONOFF => {
                    const x = try std.fmt.parseInt(c_int, str, 10);
                    try writer.print("<input type=radio name={s} value=1{s}>YES&nbsp;&nbsp;<input type=radio name={s} value=0{s}>NO", .{
                        p.name,
                        if (x != 0) " checked" else "",
                        p.name,
                        if (x != 0) "" else " checked",
                    });
                },
                .PI_SEL_C => {
                    try writer.print("<select name={s}>", .{p.name});
                    var selIt = SelectIterator{
                        .select = p.select,
                    };
                    const value = try std.fmt.parseInt(c_int, str, 10);
                    while (selIt.next()) |s| {
                        try writer.print("<option value={s}\n", .{s.cvalue});
                        if (p.type != .P_CHAR and s.value == value) {
                            try writer.writeAll(" selected");
                        }
                        try writer.writeByte('>');
                        try writer.writeAll(s.text);
                    }
                    try writer.writeAll("</select>");
                },
                .PI_CODE => {
                    try writer.print("<select name={s}>", .{p.name});
                    var cesIt = CesListIterator{
                        .list = p.ces_list.?.*,
                    };
                    const value = try std.fmt.parseInt(c_int, str, 10);
                    while (cesIt.next()) |ces| {
                        try writer.print("<option value={d}\n", .{ces.id});
                        if (ces.id == value) {
                            try writer.writeAll(" selected");
                        }
                        try writer.writeByte('>');
                        try writer.writeAll(std.mem.span(ces.name));
                    }
                    try writer.writeAll("</select>");
                },
            }
            try writer.writeAll("</td></tr>\n");
        }
        try writer.writeAll("<tr><td></td><td><p><input type=submit value=\"OK\"></td></tr>");
        try writer.writeAll("</table><hr width=50%>");
    }
    try writer.writeAll("</table></form></body></html>");
}

export fn config_panel_html() gcstr.c.Str {
    // if (optionpanel_str == NULL)
    const allocator = gcstr.GcAllocator.allocator();
    var out = std.Io.Writer.Allocating.init(allocator);
    defer out.deinit();
    const writer: *std.io.Writer = &out.writer;

    write_config_panel_html(writer) catch @panic("_config_panel_html");

    const src = out.toOwnedSlice() catch @panic("OOM");
    return gcstr.Strnew_charp_n(&src[0], @intCast(src.len));
}

extern fn _config_panel_html() gcstr.c.Str;
extern fn w3m_initialize() void;

// test "config_panel" {
//     // var argv = [1][*]const u8{"test"};
//     // _ = w3m_parse_arg(@intCast(argv.len), @ptrCast(&argv[0]));
//     w3m_initialize();
//
//     const c_ver = _config_panel_html();
//     const z_ver = config_panel_html();
//     const c_span: []const u8 = c_ver.*.ptr[0..c_ver.*.length];
//     const z_span: []const u8 = z_ver.*.ptr[0..z_ver.*.length];
//     try std.testing.expectEqualSlices(u8, c_span, z_span);
// }

/// return color code
///
/// 0 black
/// 1 red
/// 2 green
/// 3 yellow
/// 4 blue
/// 5 magenta
/// 6 cyan
/// 7 white
///
/// 8 terminal
fn strToColorCode(value: [*c]const u8) u8 {
    if (value == null) {
        // terminal
        return 8;
    }
    return switch (std.ascii.toLower(value[0])) {
        '0' => 0, // black
        '1', 'r' => 1, // red
        '2', 'g' => 2, // green
        '3', 'y' => 3, // yellow
        '4' => 4, // blue
        '5', 'm' => 5, // magenta
        '6', 'c' => 6, // cyan
        '7', 'w' => 7, // white
        '8', 't' => 8, // terminal
        'b' => if (std.mem.startsWith(u8, std.mem.span(value), "blu"))
            4 // blue
        else
            0, // black
        else => 8, // terminal
    };
}

export fn str_to_bool(_value: [*c]const u8, old: bool) bool {
    if (_value == null)
        return true;

    const value = std.mem.span(_value);
    return switch (std.ascii.toLower(value[0])) {
        '0',
        'f', // false
        'n', // no
        'u', // undef
        => false,
        'o' => if (std.ascii.toLower(value[1]) == 'f') // off
            false
        else // on
            true,
        't' => if (std.ascii.toLower(value[1]) == 'o') // toggle */
            !old
        else
            true, // true
        '!',
        'r', // reverse
        'x', // exchange
        => return !old,
        else => true,
    };
}

test "str_to_bool" {
    {
        try std.testing.expectEqual(false, str_to_bool("0", true));
        try std.testing.expectEqual(false, str_to_bool("false", true));
        try std.testing.expectEqual(false, str_to_bool("no", true));
        try std.testing.expectEqual(false, str_to_bool("undef", true));
        try std.testing.expectEqual(false, str_to_bool("off", true));
        try std.testing.expectEqual(true, str_to_bool("on", true));
        try std.testing.expectEqual(false, str_to_bool("toggle", true));
        try std.testing.expectEqual(true, str_to_bool("true", true));
        try std.testing.expectEqual(false, str_to_bool("!", true));
        try std.testing.expectEqual(false, str_to_bool("reverse", true));
        try std.testing.expectEqual(false, str_to_bool("x", true));
    }
    {
        try std.testing.expectEqual(false, str_to_bool("0", false));
        try std.testing.expectEqual(false, str_to_bool("false", false));
        try std.testing.expectEqual(false, str_to_bool("no", false));
        try std.testing.expectEqual(false, str_to_bool("undef", false));
        try std.testing.expectEqual(false, str_to_bool("off", false));
        try std.testing.expectEqual(true, str_to_bool("on", false));
        try std.testing.expectEqual(true, str_to_bool("true", false));
        try std.testing.expectEqual(true, str_to_bool("toggle", false));
        try std.testing.expectEqual(true, str_to_bool("!", false));
        try std.testing.expectEqual(true, str_to_bool("reverse", false));
        try std.testing.expectEqual(true, str_to_bool("x", false));
    }
    {
        try std.testing.expectEqual(false, str_to_bool("f", false));
        try std.testing.expectEqual(false, str_to_bool("n", false));
        try std.testing.expectEqual(false, str_to_bool("u", false));
        try std.testing.expectEqual(true, str_to_bool("o", false));
        try std.testing.expectEqual(true, str_to_bool("o", false));
        try std.testing.expectEqual(true, str_to_bool("t", false));
        try std.testing.expectEqual(true, str_to_bool("r", false));
    }
}

fn to_str(p: *const param_ptr) c.Str {
    return switch (p.*.type) {
        .P_INT, .P_COLOR, .P_CODE => c.Sprintf("%d", getVal(c.wc_ces, p)),
        .P_NZINT => c.Sprintf("%d", getVal(c_int, p)),
        .P_SHORT => c.Sprintf("%d", getVal(c_short, p)),
        .P_CHARINT => c.Sprintf("%d", getVal(c_char, p)),
        .P_CHAR => c.Sprintf("%c", getVal(c_char, p)),
        //  SystemCharset -> InnerCharset
        .P_STRING, .P_SSLPATH => c.Strnew_charp(c.conv_from_system(getVal([*c]const u8, p))),
        .P_PIXELS, .P_SCALE => c.Sprintf("%g", getVal(f64, p)),
    };
}

// show parameter with bad options invokation
export fn show_params(handle: std.fs.File.Handle) void {
    const file = std.fs.File{
        .handle = handle,
    };
    var buf: [1024]u8 = undefined;
    var writer = file.writer(&buf);
    write_show_params(&writer.interface) catch @panic("show_params");
}

fn write_show_params(writer: *std.io.Writer) !void {
    defer writer.flush() catch @panic("OOM");
    try writer.writeAll("\nconfiguration parameters\n");
    var sit = SectionIterator{
        .sections = &sections,
    };
    var j: usize = 0;
    const padding = " " ** 64;
    while (sit.next()) |section| : (j += 1) {
        try writer.print("  section[{}]: {s}\n", .{ j, section.name });
        var pit = ParamIterator{
            .params = section.params,
        };
        while (pit.next()) |p| {
            const t = switch (p.type) {
                .P_INT, .P_SHORT, .P_CHARINT, .P_NZINT => if (p.inputtype == .PI_ONOFF) "bool" else "number",
                .P_CHAR => "char",
                .P_STRING => "string",
                .P_SSLPATH => "path",
                .P_COLOR => "color",
                .P_CODE => "charset",
                .P_PIXELS => "number",
                .P_SCALE => "percent",
            };
            var l: c_int = 30 - @as(c_int, @intCast(p.name.len + t.len));
            if (l < 0)
                l = 1;
            try writer.print("    -o {s}=<{s}>{s}{s}\n", .{
                p.name,
                t,
                padding[0..@intCast(l)],
                p.comment,
            });
        }
    }
}

fn get_lower_key(line: []const u8, buf: []u8) ?[]const u8 {
    for (line, 0..) |ch, i| {
        if (std.ascii.isWhitespace(ch)) {
            buf[i] = 0;
            return buf[0..i];
        } else {
            buf[i] = std.ascii.toLower(ch);
        }
    }
    return null;
}

fn atoi(T: type, _value: [*c]const u8) T {
    const value: []const u8 = std.mem.span(_value);
    return std.fmt.parseInt(T, value, 10) catch 0;
}

fn atof(T: type, _value: [*c]const u8) T {
    const value: []const u8 = std.mem.span(_value);
    return std.fmt.parseFloat(T, value) catch 0;
}

export fn config_get_param_option(name: [*c]const u8) [*c]const u8 {
    if (config_search_param(name)) |p| {
        return to_str(p).*.ptr;
    } else {
        return null;
    }
}

export fn config_set_param_option(option: [*c]const u8) bool {
    const tmp = c.Strnew();
    var p: [*]const u8 = option;
    while (p[0] != 0 and !std.ascii.isWhitespace(p[0]) and p[0] != '=') {
        _ = c.Strcat_char(tmp, p[0]);
        p += 1;
    }
    while (p[0] != 0 and !std.ascii.isWhitespace(p[0]) and p[0] != '=') {
        while (p[0] != 0 and std.ascii.isWhitespace(p[0])) {
            p += 1;
        }
    }
    if (p[0] == '=') {
        p += 1;
        while (p[0] != 0 and std.ascii.isWhitespace(p[0]))
            p += 1;
    }
    c.Strlower(tmp);
    if (config_set_param(tmp.*.ptr, p)) {
        return true;
    }
    var q = tmp.*.ptr;
    if (std.mem.startsWith(u8, std.mem.span(q), "no")) {
        // -o noxxx, -o no-xxx, -o no_xxx
        q += 2;
        if (q[0] == '-' or q[0] == '_')
            q += 1;
    } else if (tmp.*.ptr[0] == '-') {
        // -o -xxx
        q += 1;
    } else {
        return false;
    }
    if (config_set_param(q, "0")) {
        return true;
    }
    return false;
}

export fn config_set_param(name: [*c]const u8, value: [*c]const u8) bool {
    const p = config_search_param(name) orelse {
        return false;
    };

    switch (p.type) {
        .P_INT => {
            if (p.inputtype == .PI_ONOFF) {
                const bool_val = str_to_bool(value, getBool(c_int, p));
                setVal(c_int, p, if (bool_val) 1 else 0);
            } else {
                const int_val = atoi(c_int, value);
                setVal(c_int, p, int_val);
            }
        },
        .P_NZINT => {
            const int_val = atoi(c_int, value);
            if (int_val > 0) {
                setVal(c_int, p, int_val);
            }
        },
        .P_SHORT => {
            if (p.inputtype == .PI_ONOFF) {
                const bool_val = str_to_bool(&value[0], getBool(c_short, p));
                setVal(c_short, p, if (bool_val) 1 else 0);
            } else {
                const short_val = atoi(c_short, value);
                setVal(c_short, p, short_val);
            }
        },
        .P_CHARINT => {
            if (p.inputtype == .PI_ONOFF) {
                const bool_val = str_to_bool(&value[0], getBool(c_char, p));
                setVal(c_char, p, if (bool_val) 1 else 0);
            } else {
                const char_val = atoi(c_char, value);
                setVal(c_char, p, char_val);
            }
        },
        .P_CHAR => {
            setVal(c_char, p, @intCast(value[0]));
        },
        .P_STRING => {
            setVal([*c]const u8, p, @ptrCast(&value[0]));
        },
        .P_SSLPATH => {
            if (value != null and value[0] != 0) {
                setVal([*c]const u8, p, c.rcFile(&value[0]).*.ptr);
            } else {
                setVal([*c]const u8, p, null);
                c.ssl_path_modified = 1;
            }
        },
        .P_COLOR => {
            const int_val = strToColorCode(value);
            setVal(c_int, p, int_val);
        },
        .P_CODE => {
            const wc_val = c.wc_guess_charset_short(value, getVal(c.wc_ces, p));
            setVal(c.wc_ces, p, wc_val);
        },
        .P_PIXELS => {
            const ppc = atof(f64, value);
            if (ppc >= c.MINIMUM_PIXEL_PER_CHAR and ppc <= c.MAXIMUM_PIXEL_PER_CHAR * 2) {
                setVal(f64, p, ppc);
            }
        },
        .P_SCALE => {
            const ppc = atof(f64, value);
            if (ppc >= 10 and ppc <= 1000) {
                setVal(f64, p, ppc);
            }
        },
    }
    return true;
}

pub fn read_config(reader: *std.io.Reader) !void {
    var i: usize = 0;
    while (try reader.takeDelimiter('\n')) |_line| : (i += 1) {
        const line = std.mem.trimLeft(u8, _line, &std.ascii.whitespace);
        if (line.len == 0) {
            continue;
        }
        if (line[0] == '#') {
            // comment
            continue;
        }

        var _key: [64]u8 = undefined;
        const key = get_lower_key(line, &_key) orelse {
            std.log.err("{}: [parse error]{s}", .{ i, line });
            @panic("config: line has no space");
        };

        var sp_end = key.len + 1;
        while (sp_end < line.len) : (sp_end += 1) {
            if (!std.ascii.isWhitespace(line[sp_end])) {
                break;
            }
        }

        if (sp_end < line.len) {
            std.log.debug("{s} => {s}", .{ key, line[sp_end..] });
            _ = c.config_set_param(&key[0], &line[sp_end]);
        } else {
            std.log.debug("{s} -- empty ", .{key});
            _ = c.config_set_param(&key[0], "");
        }
    }
    // @panic("X");
}
