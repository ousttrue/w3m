#include "option.h"
// #include "alloc.h"
#include "indep.h"
#include "local_cgi.h"
#include "myctype.h"
#include "w3m_rc.h"
#include "w3m_types.h"
#include "symbol.h"
#include "cookie.h"
#include "image.h"
#include "siteconf.h"
#include <libwc/charset.h>
#include <libwc/conv.h>
#include <libwc/status.h>
#include <string.h>
#include "option_cmt.h"

struct rc_search_table {
    struct param_ptr* param;
    short uniq_pos;
};

static struct rc_search_table* RC_search_table;
static int RC_table_size;

static struct sel_c colorstr[] = {
    { 0, "black", N_("black") },
    { 1, "red", N_("red") },
    { 2, "green", N_("green") },
    { 3, "yellow", N_("yellow") },
    { 4, "blue", N_("blue") },
    { 5, "magenta", N_("magenta") },
    { 6, "cyan", N_("cyan") },
    { 7, "white", N_("white") },
    { 8, "terminal", N_("terminal") },
    { 0, NULL, NULL }
};

static struct sel_c defaulturls[] = {
    { N_S(DEFAULT_URL_EMPTY), N_("none") },
    { N_S(DEFAULT_URL_CURRENT), N_("current URL") },
    { N_S(DEFAULT_URL_LINK), N_("link URL") },
    { 0, NULL, NULL }
};

static struct sel_c displayinsdel[] = {
    { N_S(DISPLAY_INS_DEL_SIMPLE), N_("simple") },
    { N_S(DISPLAY_INS_DEL_NORMAL), N_("use tag") },
    { N_S(DISPLAY_INS_DEL_FONTIFY), N_("fontify") },
    { 0, NULL, NULL }
};

static struct sel_c dnsorders[] = {
    { N_S(DNS_ORDER_UNSPEC), N_("unspecified") },
    { N_S(DNS_ORDER_INET_INET6), N_("inet inet6") },
    { N_S(DNS_ORDER_INET6_INET), N_("inet6 inet") },
    { N_S(DNS_ORDER_INET_ONLY), N_("inet only") },
    { N_S(DNS_ORDER_INET6_ONLY), N_("inet6 only") },
    { 0, NULL, NULL }
};

static struct sel_c badcookiestr[] = {
    { N_S(ACCEPT_BAD_COOKIE_DISCARD), N_("discard") },
    { N_S(ACCEPT_BAD_COOKIE_ASK), N_("ask") },
    { 0, NULL, NULL }
};

static struct sel_c mailtooptionsstr[] = {
    { N_S(MAILTO_OPTIONS_IGNORE), N_("ignore options and use only the address") },
    { N_S(MAILTO_OPTIONS_USE_MAILTO_URL), N_("use full mailto URL") },
    { 0, NULL, NULL }
};

static wc_ces_list* display_charset_str = NULL;
static wc_ces_list* document_charset_str = NULL;
static wc_ces_list* system_charset_str = NULL;
static struct sel_c auto_detect_str[] = {
    { N_S(WC_OPT_DETECT_OFF), N_("OFF") },
    { N_S(WC_OPT_DETECT_ISO_2022), N_("Only ISO 2022") },
    { N_S(WC_OPT_DETECT_ON), N_("ON") },
    { 0, NULL, NULL }
};

static struct sel_c graphic_char_str[] = {
    { N_S(GRAPHIC_CHAR_ASCII), N_("ASCII") },
    { N_S(GRAPHIC_CHAR_CHARSET), N_("charset specific") },
    { N_S(GRAPHIC_CHAR_DEC), N_("DEC special graphics") },
    { 0, NULL, NULL }
};

static struct sel_c inlineimgstr[] = {
    { N_S(INLINE_IMG_NONE), N_("external command") },
    { N_S(INLINE_IMG_OSC5379), N_("OSC 5379 (mlterm)") },
    { N_S(INLINE_IMG_SIXEL), N_("sixel (img2sixel)") },
    { N_S(INLINE_IMG_ITERM2), N_("OSC 1337 (iTerm2)") },
    { N_S(INLINE_IMG_KITTY), N_("kitty (ImageMagick)") },
    { 0, NULL, NULL }
};

struct param_ptr params1[] = {
    { "tabstop", P_NZINT, PI_TEXT, (void*)&g_runtime.Tabstop, CMT_TABSTOP, NULL },
    { "indent_incr", P_NZINT, PI_TEXT, (void*)&g_runtime.IndentIncr, CMT_INDENT_INCR,
        NULL },
    { "pixel_per_char", P_PIXELS, PI_TEXT, (void*)&g_runtime.pixel_per_char,
        CMT_PIXEL_PER_CHAR, NULL },
    { "pixel_per_line", P_PIXELS, PI_TEXT, (void*)&g_runtime.pixel_per_line,
        CMT_PIXEL_PER_LINE, NULL },
    { "frame", P_CHARINT, PI_ONOFF, (void*)&g_runtime.RenderFrame, CMT_FRAME, NULL },
    { "target_self", P_CHARINT, PI_ONOFF, (void*)&g_runtime.TargetSelf, CMT_TSELF, NULL },
    { "open_tab_blank", P_INT, PI_ONOFF, (void*)&g_runtime.open_tab_blank,
        CMT_OPEN_TAB_BLANK, NULL },
    { "open_tab_dl_list", P_INT, PI_ONOFF, (void*)&g_runtime.open_tab_dl_list,
        CMT_OPEN_TAB_DL_LIST, NULL },
    { "display_link", P_INT, PI_ONOFF, (void*)&g_runtime.displayLink, CMT_DISPLINK,
        NULL },
    { "display_link_number", P_INT, PI_ONOFF, (void*)&g_runtime.displayLinkNumber,
        CMT_DISPLINKNUMBER, NULL },
    { "decode_url", P_INT, PI_ONOFF, (void*)&g_runtime.DecodeURL, CMT_DECODE_URL, NULL },
    { "display_lineinfo", P_INT, PI_ONOFF, (void*)&g_runtime.displayLineInfo,
        CMT_DISPLINEINFO, NULL },
    { "ext_dirlist", P_INT, PI_ONOFF, (void*)&g_runtime.UseExternalDirBuffer,
        CMT_EXT_DIRLIST, NULL },
    { "dirlist_cmd", P_STRING, PI_TEXT, (void*)&g_runtime.DirBufferCommand,
        CMT_DIRLIST_CMD, NULL },
    { "use_dictcommand", P_INT, PI_ONOFF, (void*)&g_runtime.UseDictCommand,
        CMT_USE_DICTCOMMAND, NULL },
    { "dictcommand", P_STRING, PI_TEXT, (void*)&g_runtime.DictCommand,
        CMT_DICTCOMMAND, NULL },
    { "multicol", P_INT, PI_ONOFF, (void*)&g_runtime.multicolList, CMT_MULTICOL, NULL },
    { "alt_entity", P_CHARINT, PI_ONOFF, (void*)&g_runtime.UseAltEntity, CMT_ALT_ENTITY,
        NULL },
    { "graphic_char", P_CHARINT, PI_SEL_C, &g_runtime.UseGraphicChar,
        CMT_GRAPHIC_CHAR, (void*)graphic_char_str },
    { "display_borders", P_CHARINT, PI_ONOFF, (void*)&g_runtime.DisplayBorders,
        CMT_DISP_BORDERS, NULL },
    { "disable_center", P_CHARINT, PI_ONOFF, (void*)&g_runtime.DisableCenter,
        CMT_DISABLE_CENTER, NULL },
    { "fold_textarea", P_CHARINT, PI_ONOFF, (void*)&g_runtime.FoldTextarea,
        CMT_FOLD_TEXTAREA, NULL },
    { "display_ins_del", P_INT, PI_SEL_C, (void*)&g_runtime.displayInsDel,
        CMT_DISP_INS_DEL, displayinsdel },
    { "ignore_null_img_alt", P_INT, PI_ONOFF, (void*)&g_runtime.ignore_null_img_alt,
        CMT_IGNORE_NULL_IMG_ALT, NULL },
    { "view_unseenobject", P_INT, PI_ONOFF, (void*)&g_runtime.view_unseenobject,
        CMT_VIEW_UNSEENOBJECTS, NULL },
    /* XXX: emacs-w3m force to off display_image even if image options off */
    { "display_image", P_INT, PI_ONOFF, (void*)&g_runtime.displayImage, CMT_DISP_IMAGE,
        NULL },
    { "pseudo_inlines", P_INT, PI_ONOFF, (void*)&g_runtime.pseudoInlines,
        CMT_PSEUDO_INLINES, NULL },
    { "auto_image", P_INT, PI_ONOFF, (void*)&g_runtime.autoImage, CMT_AUTO_IMAGE, NULL },
    { "max_load_image", P_INT, PI_TEXT, (void*)&g_runtime.maxLoadImage,
        CMT_MAX_LOAD_IMAGE, NULL },
    { "ext_image_viewer", P_INT, PI_ONOFF, (void*)&g_runtime.useExtImageViewer,
        CMT_EXT_IMAGE_VIEWER, NULL },
    { "image_scale", P_SCALE, PI_TEXT, (void*)&g_runtime.image_scale, CMT_IMAGE_SCALE,
        NULL },
    { "inline_img_protocol", P_INT, PI_SEL_C, (void*)&g_runtime.enable_inline_image,
        CMT_INLINE_IMG_PROTOCOL, (void*)inlineimgstr },
    { "imgdisplay", P_STRING, PI_TEXT, (void*)&g_runtime.Imgdisplay, CMT_IMGDISPLAY,
        NULL },
    { "image_map_list", P_INT, PI_ONOFF, (void*)&g_runtime.image_map_list,
        CMT_IMAGE_MAP_LIST, NULL },
    { "fold_line", P_INT, PI_ONOFF, (void*)&g_runtime.FoldLine, CMT_FOLD_LINE, NULL },
    { "show_lnum", P_INT, PI_ONOFF, (void*)&g_runtime.showLineNum, CMT_SHOW_NUM, NULL },
    { "show_srch_str", P_INT, PI_ONOFF, (void*)&g_runtime.show_srch_str,
        CMT_SHOW_SRCH_STR, NULL },
    { "label_topline", P_INT, PI_ONOFF, (void*)&g_runtime.label_topline,
        CMT_LABEL_TOPLINE, NULL },
    { "nextpage_topline", P_INT, PI_ONOFF, (void*)&g_runtime.nextpage_topline,
        CMT_NEXTPAGE_TOPLINE, NULL },
    { NULL, 0, 0, NULL, NULL, NULL },
};

struct param_ptr params2[] = {
    { "color", P_INT, PI_ONOFF, (void*)&g_runtime.useColor, CMT_COLOR, NULL },
    { "high-intensity", P_INT, PI_ONOFF, (void*)&g_runtime.highIntensityColors, CMT_HINTENSITY_COLOR, NULL },
    { "basic_color", P_COLOR, PI_SEL_C, (void*)&g_runtime.basic_color, CMT_B_COLOR,
        (void*)colorstr },
    { "anchor_color", P_COLOR, PI_SEL_C, (void*)&g_runtime.anchor_color, CMT_A_COLOR,
        (void*)colorstr },
    { "image_color", P_COLOR, PI_SEL_C, (void*)&g_runtime.image_color, CMT_I_COLOR,
        (void*)colorstr },
    { "form_color", P_COLOR, PI_SEL_C, (void*)&g_runtime.form_color, CMT_F_COLOR,
        (void*)colorstr },
    { "mark_color", P_COLOR, PI_SEL_C, (void*)&g_runtime.mark_color, CMT_MARK_COLOR,
        (void*)colorstr },
    { "bg_color", P_COLOR, PI_SEL_C, (void*)&g_runtime.bg_color, CMT_BG_COLOR,
        (void*)colorstr },
    { "active_style", P_INT, PI_ONOFF, (void*)&g_runtime.useActiveColor,
        CMT_ACTIVE_STYLE, NULL },
    { "active_color", P_COLOR, PI_SEL_C, (void*)&g_runtime.active_color, CMT_C_COLOR,
        (void*)colorstr },
    { "visited_anchor", P_INT, PI_ONOFF, (void*)&g_runtime.useVisitedColor,
        CMT_VISITED_ANCHOR, NULL },
    { "visited_color", P_COLOR, PI_SEL_C, (void*)&g_runtime.visited_color, CMT_V_COLOR,
        (void*)colorstr },
    { NULL, 0, 0, NULL, NULL, NULL },
};

struct param_ptr params3[] = {
    { "pagerline", P_NZINT, PI_TEXT, (void*)&g_runtime.PagerMax, CMT_PAGERLINE, NULL },
    { "use_history", P_INT, PI_ONOFF, (void*)&g_runtime.UseHistory, CMT_HISTORY, NULL },
    { "history", P_INT, PI_TEXT, (void*)&g_runtime.URLHistSize, CMT_HISTSIZE, NULL },
    { "save_hist", P_INT, PI_ONOFF, (void*)&g_runtime.SaveURLHist, CMT_SAVEHIST, NULL },
    { "confirm_qq", P_INT, PI_ONOFF, (void*)&g_runtime.confirm_on_quit, CMT_CONFIRM_QQ,
        NULL },
    { "close_tab_back", P_INT, PI_ONOFF, (void*)&g_runtime.close_tab_back,
        CMT_CLOSE_TAB_BACK, NULL },
    { "mark", P_INT, PI_ONOFF, (void*)&g_runtime.use_mark, CMT_USE_MARK, NULL },
    { "emacs_like_lineedit", P_INT, PI_ONOFF, (void*)&g_runtime.emacs_like_lineedit,
        CMT_EMACS_LIKE_LINEEDIT, NULL },
    { "space_autocomplete", P_INT, PI_ONOFF, (void*)&g_runtime.space_autocomplete,
        CMT_SPACE_AUTOCOMPLETE, NULL },
    { "vi_prec_num", P_INT, PI_ONOFF, (void*)&g_runtime.vi_prec_num, CMT_VI_PREC_NUM,
        NULL },
    { "mark_all_pages", P_INT, PI_ONOFF, (void*)&g_runtime.MarkAllPages,
        CMT_MARK_ALL_PAGES, NULL },
    { "wrap_search", P_INT, PI_ONOFF, (void*)&g_runtime.WrapDefault, CMT_WRAP, NULL },
    { "ignorecase_search", P_INT, PI_ONOFF, (void*)&g_runtime.IgnoreCase,
        CMT_IGNORE_CASE, NULL },
#ifdef USE_MIGEMO
    { "use_migemo", P_INT, PI_ONOFF, (void*)&use_migemo, CMT_USE_MIGEMO,
        NULL },
    { "migemo_command", P_STRING, PI_TEXT, (void*)&migemo_command,
        CMT_MIGEMO_COMMAND, NULL },
#endif /* USE_MIGEMO */
    { "clear_buffer", P_INT, PI_ONOFF, (void*)&g_runtime.clear_buffer, CMT_CLEAR_BUF,
        NULL },
    { "auto_uncompress", P_CHARINT, PI_ONOFF, (void*)&g_runtime.AutoUncompress,
        CMT_AUTO_UNCOMPRESS, NULL },
    { "preserve_timestamp", P_CHARINT, PI_ONOFF, (void*)&g_runtime.PreserveTimestamp,
        CMT_PRESERVE_TIMESTAMP, NULL },
    { "keymap_file", P_STRING, PI_TEXT, (void*)&g_runtime.keymap_file, CMT_KEYMAP_FILE,
        NULL },
    { NULL, 0, 0, NULL, NULL, NULL },
};

struct param_ptr params4[] = {
    { "use_proxy", P_CHARINT, PI_ONOFF, (void*)&g_runtime.use_proxy, CMT_USE_PROXY,
        NULL },
    { "http_proxy", P_STRING, PI_TEXT, (void*)&g_runtime.HTTP_proxy, CMT_HTTP_PROXY,
        NULL },
    { "https_proxy", P_STRING, PI_TEXT, (void*)&g_runtime.HTTPS_proxy, CMT_HTTPS_PROXY,
        NULL },
    { "ftp_proxy", P_STRING, PI_TEXT, (void*)&g_runtime.FTP_proxy, CMT_FTP_PROXY, NULL },
    { "no_proxy", P_STRING, PI_TEXT, (void*)&g_runtime.NO_proxy, CMT_NO_PROXY, NULL },
    { "noproxy_netaddr", P_INT, PI_ONOFF, (void*)&g_runtime.NOproxy_netaddr,
        CMT_NOPROXY_NETADDR, NULL },
    { "no_cache", P_CHARINT, PI_ONOFF, (void*)&g_runtime.NoCache, CMT_NO_CACHE, NULL },

    { NULL, 0, 0, NULL, NULL, NULL },
};

struct param_ptr params5[] = {
    { "document_root", P_STRING, PI_TEXT, (void*)&g_runtime.document_root, CMT_DROOT,
        NULL },
    { "personal_document_root", P_STRING, PI_TEXT,
        (void*)&g_runtime.personal_document_root, CMT_PDROOT, NULL },
    { "cgi_bin", P_STRING, PI_TEXT, (void*)&g_runtime.cgi_bin, CMT_CGIBIN, NULL },
    { "index_file", P_STRING, PI_TEXT, (void*)&g_runtime.index_file, CMT_IFILE, NULL },
    { "tmp_dir", P_STRING, PI_TEXT, (void*)&g_runtime.param_tmp_dir, CMT_TMP, NULL },
    { NULL, 0, 0, NULL, NULL, NULL },
};

struct param_ptr params6[] = {
    { "mime_types", P_STRING, PI_TEXT, (void*)&g_runtime.mimetypes_files, CMT_MIMETYPES,
        NULL },
    { "mailcap", P_STRING, PI_TEXT, (void*)&g_runtime.mailcap_files, CMT_MAILCAP, NULL },
    { "urimethodmap", P_STRING, PI_TEXT, (void*)&g_runtime.urimethodmap_files,
        CMT_URIMETHODMAP, NULL },
    { "editor", P_STRING, PI_TEXT, (void*)&g_runtime.Editor, CMT_EDITOR, NULL },
    { "mailto_options", P_INT, PI_SEL_C, (void*)&g_runtime.MailtoOptions,
        CMT_MAILTO_OPTIONS, (void*)mailtooptionsstr },
    { "mailer", P_STRING, PI_TEXT, (void*)&g_runtime.Mailer, CMT_MAILER, NULL },
    { "extbrowser", P_STRING, PI_TEXT, (void*)&g_runtime.ExtBrowser, CMT_EXTBRZ, NULL },
    { "extbrowser2", P_STRING, PI_TEXT, (void*)&g_runtime.ExtBrowser2, CMT_EXTBRZ2,
        NULL },
    { "extbrowser3", P_STRING, PI_TEXT, (void*)&g_runtime.ExtBrowser3, CMT_EXTBRZ3,
        NULL },
    { "extbrowser4", P_STRING, PI_TEXT, (void*)&g_runtime.ExtBrowser4, CMT_EXTBRZ4,
        NULL },
    { "extbrowser5", P_STRING, PI_TEXT, (void*)&g_runtime.ExtBrowser5, CMT_EXTBRZ5,
        NULL },
    { "extbrowser6", P_STRING, PI_TEXT, (void*)&g_runtime.ExtBrowser6, CMT_EXTBRZ6,
        NULL },
    { "extbrowser7", P_STRING, PI_TEXT, (void*)&g_runtime.ExtBrowser7, CMT_EXTBRZ7,
        NULL },
    { "extbrowser8", P_STRING, PI_TEXT, (void*)&g_runtime.ExtBrowser8, CMT_EXTBRZ8,
        NULL },
    { "extbrowser9", P_STRING, PI_TEXT, (void*)&g_runtime.ExtBrowser9, CMT_EXTBRZ9,
        NULL },
    { "bgextviewer", P_INT, PI_ONOFF, (void*)&g_runtime.BackgroundExtViewer,
        CMT_BGEXTVIEW, NULL },
    { NULL, 0, 0, NULL, NULL, NULL },
};

struct param_ptr params7[] = {
    { "ssl_forbid_method", P_STRING, PI_TEXT, (void*)&g_runtime.ssl_forbid_method,
        CMT_SSL_FORBID_METHOD, NULL },
    { "ssl_min_version", P_STRING, PI_TEXT, (void*)&g_runtime.ssl_min_version,
        CMT_SSL_MIN_VERSION, NULL },
    { "ssl_cipher", P_STRING, PI_TEXT, (void*)&g_runtime.ssl_cipher, CMT_SSL_CIPHER,
        NULL },
    { "ssl_verify_server", P_INT, PI_ONOFF, (void*)&g_runtime.ssl_verify_server,
        CMT_SSL_VERIFY_SERVER, NULL },
    { "ssl_cert_file", P_SSLPATH, PI_TEXT, (void*)&g_runtime.ssl_cert_file,
        CMT_SSL_CERT_FILE, NULL },
    { "ssl_key_file", P_SSLPATH, PI_TEXT, (void*)&g_runtime.ssl_key_file,
        CMT_SSL_KEY_FILE, NULL },
    { "ssl_ca_path", P_SSLPATH, PI_TEXT, (void*)&g_runtime.ssl_ca_path, CMT_SSL_CA_PATH,
        NULL },
    { "ssl_ca_file", P_SSLPATH, PI_TEXT, (void*)&g_runtime.ssl_ca_file, CMT_SSL_CA_FILE,
        NULL },
    { "ssl_ca_default", P_INT, PI_ONOFF, (void*)&g_runtime.ssl_ca_default,
        CMT_SSL_CA_DEFAULT, NULL },
    { NULL, 0, 0, NULL, NULL, NULL },
};

struct param_ptr params8[] = {
    { "use_cookie", P_INT, PI_ONOFF, (void*)&g_runtime.use_cookie, CMT_USECOOKIE, NULL },
    { "show_cookie", P_INT, PI_ONOFF, (void*)&g_runtime.show_cookie,
        CMT_SHOWCOOKIE, NULL },
    { "accept_cookie", P_INT, PI_ONOFF, (void*)&g_runtime.accept_cookie,
        CMT_ACCEPTCOOKIE, NULL },
    { "accept_bad_cookie", P_INT, PI_SEL_C, (void*)&g_runtime.accept_bad_cookie,
        CMT_ACCEPTBADCOOKIE, (void*)badcookiestr },
    { "cookie_reject_domains", P_STRING, PI_TEXT,
        (void*)&g_runtime.cookie_reject_domains, CMT_COOKIE_REJECT_DOMAINS, NULL },
    { "cookie_accept_domains", P_STRING, PI_TEXT,
        (void*)&g_runtime.cookie_accept_domains, CMT_COOKIE_ACCEPT_DOMAINS, NULL },
    { "cookie_avoid_wrong_number_of_dots", P_STRING, PI_TEXT,
        (void*)&g_runtime.cookie_avoid_wrong_number_of_dots,
        CMT_COOKIE_AVOID_WONG_NUMBER_OF_DOTS, NULL },
    { NULL, 0, 0, NULL, NULL, NULL },
};

struct param_ptr params9[] = {
    { "passwd_file", P_STRING, PI_TEXT, (void*)&g_runtime.passwd_file, CMT_PASSWDFILE,
        NULL },
    { "disable_secret_security_check", P_INT, PI_ONOFF,
        (void*)&g_runtime.disable_secret_security_check, CMT_DISABLE_SECRET_SECURITY_CHECK,
        NULL },
    { "ftppasswd", P_STRING, PI_TEXT, (void*)&g_runtime.ftppasswd, CMT_FTPPASS, NULL },
    { "ftppass_hostnamegen", P_INT, PI_ONOFF, (void*)&g_runtime.ftppass_hostnamegen,
        CMT_FTPPASS_HOSTNAMEGEN, NULL },
    { "pre_form_file", P_STRING, PI_TEXT, (void*)&g_runtime.pre_form_file,
        CMT_PRE_FORM_FILE, NULL },
    { "siteconf_file", P_STRING, PI_TEXT, (void*)&siteconf_file,
        CMT_SITECONF_FILE, NULL },
    { "user_agent", P_STRING, PI_TEXT, (void*)&g_runtime.UserAgent, CMT_USERAGENT, NULL },
    { "no_referer", P_INT, PI_ONOFF, (void*)&g_runtime.NoSendReferer, CMT_NOSENDREFERER,
        NULL },
    { "cross_origin_referer", P_INT, PI_ONOFF, (void*)&g_runtime.CrossOriginReferer,
        CMT_CROSSORIGINREFERER, NULL },
    { "accept_language", P_STRING, PI_TEXT, (void*)&g_runtime.AcceptLang, CMT_ACCEPTLANG,
        NULL },
    { "accept_encoding", P_STRING, PI_TEXT, (void*)&g_runtime.AcceptEncoding,
        CMT_ACCEPTENCODING,
        NULL },
    { "accept_media", P_STRING, PI_TEXT, (void*)&g_runtime.AcceptMedia, CMT_ACCEPTMEDIA,
        NULL },
    { "argv_is_url", P_CHARINT, PI_ONOFF, (void*)&g_runtime.ArgvIsURL, CMT_ARGV_IS_URL,
        NULL },
    { "retry_http", P_INT, PI_ONOFF, (void*)&g_runtime.retryAsHttp, CMT_RETRY_HTTP,
        NULL },
    { "default_url", P_INT, PI_SEL_C, (void*)&g_runtime.DefaultURLString,
        CMT_DEFAULT_URL, (void*)defaulturls },
    { "follow_redirection", P_INT, PI_TEXT, &g_runtime.FollowRedirection,
        CMT_FOLLOW_REDIRECTION, NULL },
    { "meta_refresh", P_CHARINT, PI_ONOFF, (void*)&g_runtime.MetaRefresh,
        CMT_META_REFRESH, NULL },
    { "localhost_only", P_CHARINT, PI_ONOFF, (void*)&g_runtime.LocalhostOnly,
        CMT_LOCALHOST_ONLY, NULL },
    { "dns_order", P_INT, PI_SEL_C, (void*)&g_runtime.DNS_order, CMT_DNS_ORDER,
        (void*)dnsorders },
    { NULL, 0, 0, NULL, NULL, NULL },
};

struct param_ptr params10[] = {
    {
        .name = "display_charset",
        .type = P_CODE,
        .inputtype = PI_CODE,
        .varptr = (void*)&g_runtime.DisplayCharset,
        .comment = CMT_DISPLAY_CHARSET,
        .select = (void*)&display_charset_str,
    },
    { "document_charset", P_CODE, PI_CODE, (void*)&g_runtime.DocumentCharset,
        CMT_DOCUMENT_CHARSET, (void*)&document_charset_str },
    { "auto_detect", P_CHARINT, PI_SEL_C, (void*)&WcOption.auto_detect,
        CMT_AUTO_DETECT, (void*)auto_detect_str },
    { "system_charset", P_CODE, PI_CODE, (void*)&g_runtime.SystemCharset,
        CMT_SYSTEM_CHARSET, (void*)&system_charset_str },
    { "follow_locale", P_CHARINT, PI_ONOFF, (void*)&g_runtime.FollowLocale,
        CMT_FOLLOW_LOCALE, NULL },
    { "use_wide", P_CHARINT, PI_ONOFF, (void*)&WcOption.use_wide, CMT_USE_WIDE,
        NULL },
    { "use_combining", P_CHARINT, PI_ONOFF, (void*)&WcOption.use_combining,
        CMT_USE_COMBINING, NULL },
    { "east_asian_width", P_CHARINT, PI_ONOFF,
        (void*)&WcOption.east_asian_width, CMT_EAST_ASIAN_WIDTH, NULL },
    { "use_language_tag", P_CHARINT, PI_ONOFF,
        (void*)&WcOption.use_language_tag, CMT_USE_LANGUAGE_TAG, NULL },
    { "ucs_conv", P_CHARINT, PI_ONOFF, (void*)&WcOption.ucs_conv, CMT_UCS_CONV,
        NULL },
    { "pre_conv", P_CHARINT, PI_ONOFF, (void*)&WcOption.pre_conv, CMT_PRE_CONV,
        NULL },
    { "search_conv", P_CHARINT, PI_ONOFF, (void*)&g_runtime.SearchConv, CMT_SEARCH_CONV,
        NULL },
    { "fix_width_conv", P_CHARINT, PI_ONOFF, (void*)&WcOption.fix_width_conv,
        CMT_FIX_WIDTH_CONV, NULL },
    { "use_gb12345_map", P_CHARINT, PI_ONOFF, (void*)&WcOption.use_gb12345_map,
        CMT_USE_GB12345_MAP, NULL },
    { "use_jisx0201", P_CHARINT, PI_ONOFF, (void*)&WcOption.use_jisx0201,
        CMT_USE_JISX0201, NULL },
    { "use_jisc6226", P_CHARINT, PI_ONOFF, (void*)&WcOption.use_jisc6226,
        CMT_USE_JISC6226, NULL },
    { "use_jisx0201k", P_CHARINT, PI_ONOFF, (void*)&WcOption.use_jisx0201k,
        CMT_USE_JISX0201K, NULL },
    { "use_jisx0212", P_CHARINT, PI_ONOFF, (void*)&WcOption.use_jisx0212,
        CMT_USE_JISX0212, NULL },
    { "use_jisx0213", P_CHARINT, PI_ONOFF, (void*)&WcOption.use_jisx0213,
        CMT_USE_JISX0213, NULL },
    { "strict_iso2022", P_CHARINT, PI_ONOFF, (void*)&WcOption.strict_iso2022,
        CMT_STRICT_ISO2022, NULL },
    { "gb18030_as_ucs", P_CHARINT, PI_ONOFF, (void*)&WcOption.gb18030_as_ucs,
        CMT_GB18030_AS_UCS, NULL },
    { "simple_preserve_space", P_CHARINT, PI_ONOFF, (void*)&g_runtime.SimplePreserveSpace,
        CMT_SIMPLE_PRESERVE_SPACE, NULL },
    { NULL, 0, 0, NULL, NULL, NULL },
};

struct param_section sections[] = {
    { .name = "Display Settings", .params = params1 },
    { .name = "Color Settings", .params = params2 },
    { .name = "Miscellaneous Settings", .params = params3 },
    { .name = "Directory Settings", .params = params5 },
    { .name = "External Program Settings", .params = params6 },
    { .name = "Network Settings", .params = params9 },
    { .name = "Proxy Settings", .params = params4 },
    { .name = "SSL Settings", .params = params7 },
    { .name = "Cookie Settings", .params = params8 },
    { .name = "Charset Settings", .params = params10 },
    { .name = NULL, NULL }
};

static int
compare_table(struct rc_search_table* a, struct rc_search_table* b)
{
    return strcmp(a->param->name, b->param->name);
}

void opt_init()
{
    display_charset_str = wc_get_ces_list();
    document_charset_str = display_charset_str;
    system_charset_str = display_charset_str;

    // /* count table size */
    // RC_table_size = 0;
    // for (int j = 0; sections[j].name != NULL; j++) {
    //     int i = 0;
    //     while (sections[j].params[i].name) {
    //         i++;
    //         RC_table_size++;
    //     }
    // }
    //
    // RC_search_table = New_N(struct rc_search_table, RC_table_size);
    // int k = 0;
    // for (int j = 0; sections[j].name != NULL; j++) {
    //     int i = 0;
    //     while (sections[j].params[i].name) {
    //         RC_search_table[k].param = &sections[j].params[i];
    //         k++;
    //         i++;
    //     }
    // }
    //
    // qsort(RC_search_table, RC_table_size, sizeof(struct rc_search_table),
    //     (int (*)(const void*, const void*))compare_table);
    //
    // int diff2 = 0;
    // for (int i = 0; i < RC_table_size - 1; i++) {
    //     const char* p = RC_search_table[i].param->name;
    //     const char* q = RC_search_table[i + 1].param->name;
    //     int j = 0;
    //     for (; p[j] != '\0' && q[j] != '\0' && p[j] == q[j]; j++)
    //         ;
    //     int diff1 = j;
    //     if (diff1 > diff2)
    //         RC_search_table[i].uniq_pos = diff1 + 1;
    //     else
    //         RC_search_table[i].uniq_pos = diff2 + 1;
    //     diff2 = diff1;
    // }

    for (int j = 0; sections[j].name; j++) {
        for (int i = 0; sections[j].params[i].name; ++i) {
            opt_register(j, &sections[j].params[i]);
        }
    }
}

/// show parameter with bad options invokation
void show_params(FILE* fp)
{
    int i, j, l;
    const char* t = "";
    const char* cmt;

    g_runtime.OptionCharset = g_runtime.SystemCharset; /* FIXME */

    fputs("\nconfiguration parameters\n", fp);
    for (j = 0; sections[j].name != NULL; j++) {
        if (!g_runtime.OptionEncode)
            cmt = wc_conv(_(sections[j].name), g_runtime.OptionCharset,
                g_runtime.InnerCharset)
                      ->ptr;
        else
            cmt = sections[j].name;
        fprintf(fp, "  section[%d]: %s\n", j, conv_to_system(cmt));
        i = 0;
        while (sections[j].params[i].name) {
            switch (sections[j].params[i].type) {
            case P_INT:
            case P_SHORT:
            case P_CHARINT:
            case P_NZINT:
                t = (sections[j].params[i].inputtype == PI_ONOFF) ? "bool" : "number";
                break;
            case P_CHAR:
                t = "char";
                break;
            case P_STRING:
                t = "string";
                break;

            case P_SSLPATH:
                t = "path";
                break;

            case P_COLOR:
                t = "color";
                break;

            case P_CODE:
                t = "charset";
                break;

            case P_PIXELS:
                t = "number";
                break;
            case P_SCALE:
                t = "percent";
                break;
            }

            if (!g_runtime.OptionEncode)
                cmt = wc_conv(_(sections[j].params[i].comment),
                    g_runtime.OptionCharset, g_runtime.InnerCharset)
                          ->ptr;
            else

                cmt = sections[j].params[i].comment;
            l = 30 - (strlen(sections[j].params[i].name) + strlen(t));
            if (l < 0)
                l = 1;
            fprintf(fp, "    -o %s=<%s>%*s%s\n",
                sections[j].params[i].name, t, l, " ",
                conv_to_system(cmt));
            i++;
        }
    }
}

static int
str_to_color(const char* value)
{
    if (value == NULL)
        return 8; /* terminal */
    switch (TOLOWER(*value)) {
    case '0':
        return 0; /* black */
    case '1':
    case 'r':
        return 1; /* red */
    case '2':
    case 'g':
        return 2; /* green */
    case '3':
    case 'y':
        return 3; /* yellow */
    case '4':
        return 4; /* blue */
    case '5':
    case 'm':
        return 5; /* magenta */
    case '6':
    case 'c':
        return 6; /* cyan */
    case '7':
    case 'w':
        return 7; /* white */
    case '8':
    case 't':
        return 8; /* terminal */
    case 'b':
        if (!strncasecmp(value, "blu", 3))
            return 4; /* blue */
        else
            return 0; /* black */
    }
    return 8; /* terminal */
}

bool opt_set_param(const char* name, const char* value)
{
    if (!value)
        return 0;

    struct param_ptr* p = opt_get_param(name);
    if (p == NULL)
        return 0;
    switch (p->type) {
    case P_INT:
        if (atoi(value) >= 0)
            *(int*)p->varptr = (p->inputtype == PI_ONOFF)
                ? str_to_bool(value, *(int*)p->varptr)
                : atoi(value);
        break;
    case P_NZINT:
        if (atoi(value) > 0)
            *(int*)p->varptr = atoi(value);
        break;
    case P_SHORT:
        *(short*)p->varptr = (p->inputtype == PI_ONOFF)
            ? str_to_bool(value, *(short*)p->varptr)
            : atoi(value);
        break;
    case P_CHARINT:
        *(char*)p->varptr = (p->inputtype == PI_ONOFF)
            ? str_to_bool(value, *(char*)p->varptr)
            : atoi(value);
        break;
    case P_CHAR:
        *(char*)p->varptr = value[0];
        break;
    case P_STRING:
        *(const char**)p->varptr = value;
        break;

    case P_SSLPATH:
        if (value != NULL && value[0] != '\0')
            *(char**)p->varptr = rcFile(value);
        else
            *(char**)p->varptr = NULL;
        g_runtime.ssl_path_modified = 1;
        break;

    case P_COLOR:
        *(int*)p->varptr = str_to_color(value);
        break;

    case P_CODE:
        *(enum wc_ces*)p->varptr = wc_guess_charset_short(value, *(enum wc_ces*)p->varptr);
        break;

    case P_PIXELS: {
        double ppc = atof(value);
        if (ppc >= MINIMUM_PIXEL_PER_CHAR && ppc <= MAXIMUM_PIXEL_PER_CHAR * 2)
            *(double*)p->varptr = ppc;
        break;
    }
    case P_SCALE: {
        double ppc = atof(value);
        if (ppc >= 10 && ppc <= 1000)
            *(double*)p->varptr = ppc;
        break;
    }
    }
    return 1;
}

bool opt_set_param_option(const char* option)
{
    Str tmp = Strnew();
    const char *p = option, *q;

    while (*p && !IS_SPACE(*p) && *p != '=')
        Strcat_char(tmp, *p++);
    while (*p && IS_SPACE(*p))
        p++;
    if (*p == '=') {
        p++;
        while (*p && IS_SPACE(*p))
            p++;
    }
    Strlower(tmp);
    if (opt_set_param(tmp->ptr, p))
        goto option_assigned;
    q = tmp->ptr;
    if (!strncmp(q, "no", 2)) { /* -o noxxx, -o no-xxx, -o no_xxx */
        q += 2;
        if (*q == '-' || *q == '_')
            q++;
    } else if (tmp->ptr[0] == '-') /* -o -xxx */
        q++;
    else
        return 0;
    if (opt_set_param(q, "0"))
        goto option_assigned;
    return 0;
option_assigned:
    return 1;
}

static Str to_str(struct param_ptr* p)
{
    switch (p->type) {
    case P_INT:
    case P_COLOR:
    case P_CODE:
        return Sprintf("%d", (int)(*(enum wc_ces*)p->varptr));
    case P_NZINT:
        return Sprintf("%d", *(int*)p->varptr);
    case P_SHORT:
        return Sprintf("%d", *(short*)p->varptr);
    case P_CHARINT:
        return Sprintf("%d", *(char*)p->varptr);
    case P_CHAR:
        return Sprintf("%c", *(char*)p->varptr);
    case P_STRING:
    case P_SSLPATH:
        /*  SystemCharset -> InnerCharset */
        return Strnew_charp(conv_from_system(*(char**)p->varptr));
    case P_PIXELS:
    case P_SCALE:
        return Sprintf("%g", *(double*)p->varptr);
    }
    /* not reached */
    return NULL;
}

char* opt_get_param_option(const char* name)
{
    struct param_ptr* p = opt_get_param(name);
    return p ? to_str(p)->ptr : NULL;
}

#define CMT_HELPER "External Viewer Setup"

Str opt_load_panel(void)
{
    static Str optionpanel_str = NULL;
    static char optionpanel_src1[] = "<html><head><title>Option Setting Panel</title></head><body>\
<h1 align=center>Option Setting Panel<br>(w3m version %s)</b></h1>\
<form method=post action=\"file:///$LIB/" W3MHELPERPANEL_CMDNAME "\">\
<input type=hidden name=mode value=panel>\
<input type=hidden name=cookie value=\"%s\">\
<input type=submit value=\"%s\">\
</form><br>\
<form method=internal action=option>";

    if (optionpanel_str == NULL)
        optionpanel_str = Sprintf(optionpanel_src1, w3m_version,
            html_quote(localCookie()->ptr), CMT_HELPER);

    g_runtime.OptionCharset = g_runtime.SystemCharset; /* FIXME */
    if (!g_runtime.OptionEncode) {
        optionpanel_str = wc_Str_conv(optionpanel_str, g_runtime.OptionCharset, g_runtime.InnerCharset);
        for (int i = 0; sections[i].name != NULL; i++) {
            sections[i].name = wc_conv(_(sections[i].name), g_runtime.OptionCharset, g_runtime.InnerCharset)->ptr;
            for (struct param_ptr* p = sections[i].params; p->name; p++) {
                p->comment = wc_conv(_(p->comment), g_runtime.OptionCharset,
                    g_runtime.InnerCharset)
                                 ->ptr;
                if (p->inputtype == PI_SEL_C
                    && p->select != colorstr) {
                    for (struct sel_c* s = (struct sel_c*)p->select; s->text != NULL; s++) {
                        s->text = wc_conv(_(s->text), g_runtime.OptionCharset,
                            g_runtime.InnerCharset)
                                      ->ptr;
                    }
                }
            }
        }

        for (struct sel_c* s = colorstr; s->text; s++)
            s->text = wc_conv(_(s->text), g_runtime.OptionCharset,
                g_runtime.InnerCharset)
                          ->ptr;

        g_runtime.OptionEncode = TRUE;
    }

    Str src = Strdup(optionpanel_str);

    Strcat_charp(src, "<table><tr><td>");
    for (int i = 0; sections[i].name != NULL; i++) {
        Strcat_m_charp(src, "<h1>", sections[i].name, "</h1>", NULL);
        struct param_ptr* p = sections[i].params;
        Strcat_charp(src, "<table width=100% cellpadding=0>");
        while (p->name) {
            Strcat_m_charp(src, "<tr><td>", p->comment, NULL);
            Strcat(src, Sprintf("</td><td width=%d>", (int)(28 * g_runtime.pixel_per_char)));
            switch (p->inputtype) {
            case PI_TEXT:
                Strcat_m_charp(src, "<input type=text name=",
                    p->name,
                    " value=\"",
                    html_quote(to_str(p)->ptr), "\">", NULL);
                break;
            case PI_ONOFF: {
                int x = atoi(to_str(p)->ptr);
                Strcat_m_charp(src, "<input type=radio name=",
                    p->name,
                    " value=1",
                    (x ? " checked" : ""),
                    ">YES&nbsp;&nbsp;<input type=radio name=",
                    p->name,
                    " value=0", (x ? "" : " checked"), ">NO", NULL);
                break;
            }
            case PI_SEL_C: {
                Str tmp = to_str(p);
                Strcat_m_charp(src, "<select name=", p->name, ">", NULL);
                for (struct sel_c* s = (struct sel_c*)p->select; s->text != NULL; s++) {
                    Strcat_charp(src, "<option value=");
                    Strcat(src, Sprintf("%s\n", s->cvalue));
                    if ((p->type != P_CHAR && s->value == atoi(tmp->ptr)) || (p->type == P_CHAR && (char)s->value == *(tmp->ptr)))
                        Strcat_charp(src, " selected");
                    Strcat_char(src, '>');
                    Strcat_charp(src, s->text);
                }
                Strcat_charp(src, "</select>");
                break;
            }
            case PI_CODE: {
                Str tmp = to_str(p);
                Strcat_m_charp(src, "<select name=", p->name, ">", NULL);
                for (wc_ces_list* c = *(wc_ces_list**)p->select; c->desc != NULL; c++) {
                    Strcat_charp(src, "<option value=");
                    Strcat(src, Sprintf("%s\n", c->name));
                    if (c->id == atoi(tmp->ptr))
                        Strcat_charp(src, " selected");
                    Strcat_char(src, '>');
                    Strcat_charp(src, c->desc);
                }
                Strcat_charp(src, "</select>");
                break;
            }
            }
            Strcat_charp(src, "</td></tr>\n");
            p++;
        }
        Strcat_charp(src,
            "<tr><td></td><td><p><input type=submit value=\"OK\"></td></tr>");
        Strcat_charp(src, "</table><hr width=50%>");
    }
    Strcat_charp(src, "</table></form></body></html>");
    return src;
}
