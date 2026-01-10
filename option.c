#include "option.h"
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

void opt_init()
{
    opt_register(SETTINGS_DISPLAY, "tabstop", CMT_TABSTOP, (void*)&g_runtime.Tabstop, P_NZINT, PI_TEXT, NULL);
    opt_register(SETTINGS_DISPLAY, "indent_incr", CMT_INDENT_INCR, (void*)&g_runtime.IndentIncr, P_NZINT, PI_TEXT, NULL);
    opt_register(SETTINGS_DISPLAY, "pixel_per_char", CMT_PIXEL_PER_CHAR, (void*)&g_runtime.pixel_per_char, P_PIXELS, PI_TEXT, NULL);
    opt_register(SETTINGS_DISPLAY, "pixel_per_line", CMT_PIXEL_PER_LINE, (void*)&g_runtime.pixel_per_line, P_PIXELS, PI_TEXT, NULL);
    opt_register(SETTINGS_DISPLAY, "frame", CMT_FRAME, (void*)&g_runtime.RenderFrame, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "target_self", CMT_TSELF, (void*)&g_runtime.TargetSelf, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "open_tab_blank", CMT_OPEN_TAB_BLANK, (void*)&g_runtime.open_tab_blank, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "open_tab_dl_list", CMT_OPEN_TAB_DL_LIST, (void*)&g_runtime.open_tab_dl_list, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "display_link", CMT_DISPLINK, (void*)&g_runtime.displayLink, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "display_link_number", CMT_DISPLINKNUMBER, (void*)&g_runtime.displayLinkNumber, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "decode_url", CMT_DECODE_URL, (void*)&g_runtime.DecodeURL, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "display_lineinfo", CMT_DISPLINEINFO, (void*)&g_runtime.displayLineInfo, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "ext_dirlist", CMT_EXT_DIRLIST, (void*)&g_runtime.UseExternalDirBuffer, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "dirlist_cmd", CMT_DIRLIST_CMD, (void*)&g_runtime.DirBufferCommand, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_DISPLAY, "use_dictcommand", CMT_USE_DICTCOMMAND, (void*)&g_runtime.UseDictCommand, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "dictcommand", CMT_DICTCOMMAND, (void*)&g_runtime.DictCommand, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_DISPLAY, "multicol", CMT_MULTICOL, (void*)&g_runtime.multicolList, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "alt_entity", CMT_ALT_ENTITY, (void*)&g_runtime.UseAltEntity, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "graphic_char", CMT_GRAPHIC_CHAR, &g_runtime.UseGraphicChar, P_CHARINT, PI_SEL_C, (void*)graphic_char_str);
    opt_register(SETTINGS_DISPLAY, "display_borders", CMT_DISP_BORDERS, (void*)&g_runtime.DisplayBorders, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "disable_center", CMT_DISABLE_CENTER, (void*)&g_runtime.DisableCenter, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "fold_textarea", CMT_FOLD_TEXTAREA, (void*)&g_runtime.FoldTextarea, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "display_ins_del", CMT_DISP_INS_DEL, (void*)&g_runtime.displayInsDel, P_INT, PI_SEL_C, displayinsdel);
    opt_register(SETTINGS_DISPLAY, "ignore_null_img_alt", CMT_IGNORE_NULL_IMG_ALT, (void*)&g_runtime.ignore_null_img_alt, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "view_unseenobject", CMT_VIEW_UNSEENOBJECTS, (void*)&g_runtime.view_unseenobject, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "display_image", CMT_DISP_IMAGE, (void*)&g_runtime.displayImage, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "pseudo_inlines", CMT_PSEUDO_INLINES, (void*)&g_runtime.pseudoInlines, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "auto_image", CMT_AUTO_IMAGE, (void*)&g_runtime.autoImage, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "max_load_image", CMT_MAX_LOAD_IMAGE, (void*)&g_runtime.maxLoadImage, P_INT, PI_TEXT, NULL);
    opt_register(SETTINGS_DISPLAY, "ext_image_viewer", CMT_EXT_IMAGE_VIEWER, (void*)&g_runtime.useExtImageViewer, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "image_scale", CMT_IMAGE_SCALE, (void*)&g_runtime.image_scale, P_SCALE, PI_TEXT, NULL);
    opt_register(SETTINGS_DISPLAY, "inline_img_protocol", CMT_INLINE_IMG_PROTOCOL, (void*)&g_runtime.enable_inline_image, P_INT, PI_SEL_C, (void*)inlineimgstr);
    opt_register(SETTINGS_DISPLAY, "imgdisplay", CMT_IMGDISPLAY, (void*)&g_runtime.Imgdisplay, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_DISPLAY, "image_map_list", CMT_IMAGE_MAP_LIST, (void*)&g_runtime.image_map_list, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "fold_line", CMT_FOLD_LINE, (void*)&g_runtime.FoldLine, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "show_lnum", CMT_SHOW_NUM, (void*)&g_runtime.showLineNum, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "show_srch_str", CMT_SHOW_SRCH_STR, (void*)&g_runtime.show_srch_str, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "label_topline", CMT_LABEL_TOPLINE, (void*)&g_runtime.label_topline, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_DISPLAY, "nextpage_topline", CMT_NEXTPAGE_TOPLINE, (void*)&g_runtime.nextpage_topline, P_INT, PI_ONOFF, NULL);

    opt_register(SETTINGS_COLOR, "color", CMT_COLOR, (void*)&g_runtime.useColor, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_COLOR, "high-intensity", CMT_HINTENSITY_COLOR, (void*)&g_runtime.highIntensityColors, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_COLOR, "basic_color", CMT_B_COLOR, (void*)&g_runtime.basic_color, P_COLOR, PI_SEL_C, (void*)colorstr);
    opt_register(SETTINGS_COLOR, "anchor_color", CMT_A_COLOR, (void*)&g_runtime.anchor_color, P_COLOR, PI_SEL_C, (void*)colorstr);
    opt_register(SETTINGS_COLOR, "image_color", CMT_I_COLOR, (void*)&g_runtime.image_color, P_COLOR, PI_SEL_C, (void*)colorstr);
    opt_register(SETTINGS_COLOR, "form_color", CMT_F_COLOR, (void*)&g_runtime.form_color, P_COLOR, PI_SEL_C, (void*)colorstr);
    opt_register(SETTINGS_COLOR, "mark_color", CMT_MARK_COLOR, (void*)&g_runtime.mark_color, P_COLOR, PI_SEL_C, (void*)colorstr);
    opt_register(SETTINGS_COLOR, "bg_color", CMT_BG_COLOR, (void*)&g_runtime.bg_color, P_COLOR, PI_SEL_C, (void*)colorstr);
    opt_register(SETTINGS_COLOR, "active_style", CMT_ACTIVE_STYLE, (void*)&g_runtime.useActiveColor, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_COLOR, "active_color", CMT_C_COLOR, (void*)&g_runtime.active_color, P_COLOR, PI_SEL_C, (void*)colorstr);
    opt_register(SETTINGS_COLOR, "visited_anchor", CMT_VISITED_ANCHOR, (void*)&g_runtime.useVisitedColor, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_COLOR, "visited_color", CMT_V_COLOR, (void*)&g_runtime.visited_color, P_COLOR, PI_SEL_C, (void*)colorstr);

    opt_register(SETTINGS_DIRECTORY, "document_root", CMT_DROOT, (void*)&g_runtime.document_root, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_DIRECTORY, "personal_document_root", CMT_PDROOT, (void*)&g_runtime.personal_document_root, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_DIRECTORY, "cgi_bin", CMT_CGIBIN, (void*)&g_runtime.cgi_bin, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_DIRECTORY, "index_file", CMT_IFILE, (void*)&g_runtime.index_file, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_DIRECTORY, "tmp_dir", CMT_TMP, (void*)&g_runtime.param_tmp_dir, P_STRING, PI_TEXT, NULL);

    opt_register(SETTINGS_MISCELLANEOUS, "pagerline", CMT_PAGERLINE, (void*)&g_runtime.PagerMax, P_NZINT, PI_TEXT, NULL);
    opt_register(SETTINGS_MISCELLANEOUS, "use_history", CMT_HISTORY, (void*)&g_runtime.UseHistory, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_MISCELLANEOUS, "history", CMT_HISTSIZE, (void*)&g_runtime.URLHistSize, P_INT, PI_TEXT, NULL);
    opt_register(SETTINGS_MISCELLANEOUS, "save_hist", CMT_SAVEHIST, (void*)&g_runtime.SaveURLHist, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_MISCELLANEOUS, "confirm_qq", CMT_CONFIRM_QQ, (void*)&g_runtime.confirm_on_quit, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_MISCELLANEOUS, "close_tab_back", CMT_CLOSE_TAB_BACK, (void*)&g_runtime.close_tab_back, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_MISCELLANEOUS, "mark", CMT_USE_MARK, (void*)&g_runtime.use_mark, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_MISCELLANEOUS, "emacs_like_lineedit", CMT_EMACS_LIKE_LINEEDIT, (void*)&g_runtime.emacs_like_lineedit, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_MISCELLANEOUS, "space_autocomplete", CMT_SPACE_AUTOCOMPLETE, (void*)&g_runtime.space_autocomplete, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_MISCELLANEOUS, "vi_prec_num", CMT_VI_PREC_NUM, (void*)&g_runtime.vi_prec_num, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_MISCELLANEOUS, "mark_all_pages", CMT_MARK_ALL_PAGES, (void*)&g_runtime.MarkAllPages, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_MISCELLANEOUS, "wrap_search", CMT_WRAP, (void*)&g_runtime.WrapDefault, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_MISCELLANEOUS, "ignorecase_search", CMT_IGNORE_CASE, (void*)&g_runtime.IgnoreCase, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_MISCELLANEOUS, "clear_buffer", CMT_CLEAR_BUF, (void*)&g_runtime.clear_buffer, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_MISCELLANEOUS, "auto_uncompress", CMT_AUTO_UNCOMPRESS, (void*)&g_runtime.AutoUncompress, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_MISCELLANEOUS, "preserve_timestamp", CMT_PRESERVE_TIMESTAMP, (void*)&g_runtime.PreserveTimestamp, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_MISCELLANEOUS, "keymap_file", CMT_KEYMAP_FILE, (void*)&g_runtime.keymap_file, P_STRING, PI_TEXT, NULL);

    opt_register(SETTINGS_EXTERNALPROGRAM, "mime_types", CMT_MIMETYPES, (void*)&g_runtime.mimetypes_files, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_EXTERNALPROGRAM, "mailcap", CMT_MAILCAP, (void*)&g_runtime.mailcap_files, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_EXTERNALPROGRAM, "urimethodmap", CMT_URIMETHODMAP, (void*)&g_runtime.urimethodmap_files, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_EXTERNALPROGRAM, "editor", CMT_EDITOR, (void*)&g_runtime.Editor, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_EXTERNALPROGRAM, "mailto_options", CMT_MAILTO_OPTIONS, (void*)&g_runtime.MailtoOptions, P_INT, PI_SEL_C, (void*)mailtooptionsstr);
    opt_register(SETTINGS_EXTERNALPROGRAM, "mailer", CMT_MAILER, (void*)&g_runtime.Mailer, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_EXTERNALPROGRAM, "extbrowser", CMT_EXTBRZ, (void*)&g_runtime.ExtBrowser, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_EXTERNALPROGRAM, "extbrowser2", CMT_EXTBRZ2, (void*)&g_runtime.ExtBrowser2, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_EXTERNALPROGRAM, "extbrowser3", CMT_EXTBRZ3, (void*)&g_runtime.ExtBrowser3, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_EXTERNALPROGRAM, "extbrowser4", CMT_EXTBRZ4, (void*)&g_runtime.ExtBrowser4, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_EXTERNALPROGRAM, "extbrowser5", CMT_EXTBRZ5, (void*)&g_runtime.ExtBrowser5, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_EXTERNALPROGRAM, "extbrowser6", CMT_EXTBRZ6, (void*)&g_runtime.ExtBrowser6, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_EXTERNALPROGRAM, "extbrowser7", CMT_EXTBRZ7, (void*)&g_runtime.ExtBrowser7, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_EXTERNALPROGRAM, "extbrowser8", CMT_EXTBRZ8, (void*)&g_runtime.ExtBrowser8, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_EXTERNALPROGRAM, "extbrowser9", CMT_EXTBRZ9, (void*)&g_runtime.ExtBrowser9, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_EXTERNALPROGRAM, "bgextviewer", CMT_BGEXTVIEW, (void*)&g_runtime.BackgroundExtViewer, P_INT, PI_ONOFF, NULL);

    opt_register(SETTINGS_PROXY, "use_proxy", CMT_USE_PROXY, (void*)&g_runtime.use_proxy, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_PROXY, "http_proxy", CMT_HTTP_PROXY, (void*)&g_runtime.HTTP_proxy, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_PROXY, "https_proxy", CMT_HTTPS_PROXY, (void*)&g_runtime.HTTPS_proxy, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_PROXY, "ftp_proxy", CMT_FTP_PROXY, (void*)&g_runtime.FTP_proxy, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_PROXY, "no_proxy", CMT_NO_PROXY, (void*)&g_runtime.NO_proxy, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_PROXY, "noproxy_netaddr", CMT_NOPROXY_NETADDR, (void*)&g_runtime.NOproxy_netaddr, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_PROXY, "no_cache", CMT_NO_CACHE, (void*)&g_runtime.NoCache, P_CHARINT, PI_ONOFF, NULL);

    opt_register(SETTINGS_NETWORK, "passwd_file", CMT_PASSWDFILE, (void*)&g_runtime.passwd_file, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_NETWORK, "disable_secret_security_check", CMT_DISABLE_SECRET_SECURITY_CHECK, (void*)&g_runtime.disable_secret_security_check, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_NETWORK, "ftppasswd", CMT_FTPPASS, (void*)&g_runtime.ftppasswd, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_NETWORK, "ftppass_hostnamegen", CMT_FTPPASS_HOSTNAMEGEN, (void*)&g_runtime.ftppass_hostnamegen, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_NETWORK, "pre_form_file", CMT_PRE_FORM_FILE, (void*)&g_runtime.pre_form_file, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_NETWORK, "siteconf_file", CMT_SITECONF_FILE, (void*)&siteconf_file, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_NETWORK, "user_agent", CMT_USERAGENT, (void*)&g_runtime.UserAgent, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_NETWORK, "no_referer", CMT_NOSENDREFERER, (void*)&g_runtime.NoSendReferer, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_NETWORK, "cross_origin_referer", CMT_CROSSORIGINREFERER, (void*)&g_runtime.CrossOriginReferer, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_NETWORK, "accept_language", CMT_ACCEPTLANG, (void*)&g_runtime.AcceptLang, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_NETWORK, "accept_encoding", CMT_ACCEPTENCODING, (void*)&g_runtime.AcceptEncoding, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_NETWORK, "accept_media", CMT_ACCEPTMEDIA, (void*)&g_runtime.AcceptMedia, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_NETWORK, "argv_is_url", CMT_ARGV_IS_URL, (void*)&g_runtime.ArgvIsURL, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_NETWORK, "retry_http", CMT_RETRY_HTTP, (void*)&g_runtime.retryAsHttp, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_NETWORK, "default_url", CMT_DEFAULT_URL, (void*)&g_runtime.DefaultURLString, P_INT, PI_SEL_C, (void*)defaulturls);
    opt_register(SETTINGS_NETWORK, "follow_redirection", CMT_FOLLOW_REDIRECTION, &g_runtime.FollowRedirection, P_INT, PI_TEXT, NULL);
    opt_register(SETTINGS_NETWORK, "meta_refresh", CMT_META_REFRESH, (void*)&g_runtime.MetaRefresh, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_NETWORK, "localhost_only", CMT_LOCALHOST_ONLY, (void*)&g_runtime.LocalhostOnly, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_NETWORK, "dns_order", CMT_DNS_ORDER, (void*)&g_runtime.DNS_order, P_INT, PI_SEL_C, (void*)dnsorders);

    opt_register(SETTINGS_COOKIE, "use_cookie", CMT_USECOOKIE, (void*)&g_runtime.use_cookie, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_COOKIE, "show_cookie", CMT_SHOWCOOKIE, (void*)&g_runtime.show_cookie, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_COOKIE, "accept_cookie", CMT_ACCEPTCOOKIE, (void*)&g_runtime.accept_cookie, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_COOKIE, "accept_bad_cookie", CMT_ACCEPTBADCOOKIE, (void*)&g_runtime.accept_bad_cookie, P_INT, PI_SEL_C, (void*)badcookiestr);
    opt_register(SETTINGS_COOKIE, "cookie_reject_domains", CMT_COOKIE_REJECT_DOMAINS, (void*)&g_runtime.cookie_reject_domains, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_COOKIE, "cookie_accept_domains", CMT_COOKIE_ACCEPT_DOMAINS, (void*)&g_runtime.cookie_accept_domains, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_COOKIE, "cookie_avoid_wrong_number_of_dots", CMT_COOKIE_AVOID_WONG_NUMBER_OF_DOTS, (void*)&g_runtime.cookie_avoid_wrong_number_of_dots, P_STRING, PI_TEXT, NULL);

    opt_register(SETTINGS_SSL, "ssl_forbid_method", CMT_SSL_FORBID_METHOD, (void*)&g_runtime.ssl_forbid_method, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_SSL, "ssl_min_version", CMT_SSL_MIN_VERSION, (void*)&g_runtime.ssl_min_version, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_SSL, "ssl_cipher", CMT_SSL_CIPHER, (void*)&g_runtime.ssl_cipher, P_STRING, PI_TEXT, NULL);
    opt_register(SETTINGS_SSL, "ssl_verify_server", CMT_SSL_VERIFY_SERVER, (void*)&g_runtime.ssl_verify_server, P_INT, PI_ONOFF, NULL);
    opt_register(SETTINGS_SSL, "ssl_cert_file", CMT_SSL_CERT_FILE, (void*)&g_runtime.ssl_cert_file, P_SSLPATH, PI_TEXT, NULL);
    opt_register(SETTINGS_SSL, "ssl_key_file", CMT_SSL_KEY_FILE, (void*)&g_runtime.ssl_key_file, P_SSLPATH, PI_TEXT, NULL);
    opt_register(SETTINGS_SSL, "ssl_ca_path", CMT_SSL_CA_PATH, (void*)&g_runtime.ssl_ca_path, P_SSLPATH, PI_TEXT, NULL);
    opt_register(SETTINGS_SSL, "ssl_ca_file", CMT_SSL_CA_FILE, (void*)&g_runtime.ssl_ca_file, P_SSLPATH, PI_TEXT, NULL);
    opt_register(SETTINGS_SSL, "ssl_ca_default", CMT_SSL_CA_DEFAULT, (void*)&g_runtime.ssl_ca_default, P_INT, PI_ONOFF, NULL);

    opt_register(SETTINGS_CHARSET, "display_charset", CMT_DISPLAY_CHARSET, (void*)&g_runtime.DisplayCharset, P_CODE, PI_CODE, NULL);
    opt_register(SETTINGS_CHARSET, "document_charset", CMT_DOCUMENT_CHARSET, (void*)&g_runtime.DocumentCharset, P_CODE, PI_CODE, NULL);
    opt_register(SETTINGS_CHARSET, "auto_detect", CMT_AUTO_DETECT, (void*)&WcOption.auto_detect, P_CHARINT, PI_SEL_C, (void*)auto_detect_str);
    opt_register(SETTINGS_CHARSET, "system_charset", CMT_SYSTEM_CHARSET, (void*)&g_runtime.SystemCharset, P_CODE, PI_CODE, NULL);
    opt_register(SETTINGS_CHARSET, "follow_locale", CMT_FOLLOW_LOCALE, (void*)&g_runtime.FollowLocale, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_CHARSET, "use_wide", CMT_USE_WIDE, (void*)&WcOption.use_wide, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_CHARSET, "use_combining", CMT_USE_COMBINING, (void*)&WcOption.use_combining, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_CHARSET, "east_asian_width", CMT_EAST_ASIAN_WIDTH, (void*)&WcOption.east_asian_width, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_CHARSET, "use_language_tag", CMT_USE_LANGUAGE_TAG, (void*)&WcOption.use_language_tag, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_CHARSET, "ucs_conv", CMT_UCS_CONV, (void*)&WcOption.ucs_conv, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_CHARSET, "pre_conv", CMT_PRE_CONV, (void*)&WcOption.pre_conv, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_CHARSET, "search_conv", CMT_SEARCH_CONV, (void*)&g_runtime.SearchConv, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_CHARSET, "fix_width_conv", CMT_FIX_WIDTH_CONV, (void*)&WcOption.fix_width_conv, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_CHARSET, "use_gb12345_map", CMT_USE_GB12345_MAP, (void*)&WcOption.use_gb12345_map, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_CHARSET, "use_jisx0201", CMT_USE_JISX0201, (void*)&WcOption.use_jisx0201, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_CHARSET, "use_jisc6226", CMT_USE_JISC6226, (void*)&WcOption.use_jisc6226, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_CHARSET, "use_jisx0201k", CMT_USE_JISX0201K, (void*)&WcOption.use_jisx0201k, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_CHARSET, "use_jisx0212", CMT_USE_JISX0212, (void*)&WcOption.use_jisx0212, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_CHARSET, "use_jisx0213", CMT_USE_JISX0213, (void*)&WcOption.use_jisx0213, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_CHARSET, "strict_iso2022", CMT_STRICT_ISO2022, (void*)&WcOption.strict_iso2022, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_CHARSET, "gb18030_as_ucs", CMT_GB18030_AS_UCS, (void*)&WcOption.gb18030_as_ucs, P_CHARINT, PI_ONOFF, NULL);
    opt_register(SETTINGS_CHARSET, "simple_preserve_space", CMT_SIMPLE_PRESERVE_SPACE, (void*)&g_runtime.SimplePreserveSpace, P_CHARINT, PI_ONOFF, NULL);
}
