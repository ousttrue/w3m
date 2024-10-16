/*
 * Initialization file etc.
 */
#include "rc.h"
#include "alloc.h"
#include "buffer/buffer.h"
#include "buffer/display.h"
#include "buffer/document.h"
#include "buffer/image.h"
#include "buffer/search.h"
#include "buffer/w3mhelperpanel.h"
#include "core.h"
#include "file/file.h"
#include "file/tmpfile.h"
#include "fm.h"
#include "func.h"
#include "html/html_readbuffer.h"
#include "html/html_renderer.h"
#include "html/html_tag.h"
#include "html/html_text.h"
#include "input/http_auth.h"
#include "input/http_cookie.h"
#include "input/isocket.h"
#include "input/loader.h"
#include "input/localcgi.h"
#include "input/url.h"
#include "input/url_stream.h"
#include "proto.h"
#include "siteconf.h"
#include "term/terms.h"
#include "term/termsize.h"
#include "text/myctype.h"
#include "text/regex.h"
#include "text/text.h"
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

bool no_rc_dir = false;
const char *rc_dir = nullptr;

int MailtoOptions = MAILTO_OPTIONS_IGNORE;

int DefaultURLString = DEFAULT_URL_CURRENT;

#define W3MCONFIG "w3mconfig"
#define CONFIG_FILE "config"
const char *DictCommand = "file:///$LIB/w3mdict" CGI_EXTENSION;

enum ParamType {
  P_INT = 0,
  P_SHORT = 1,
  P_CHARINT = 2,
  P_CHAR = 3,
  P_STRING = 4,
  P_SSLPATH = 5,
  P_PIXELS = 8,
  P_NZINT = 9,
  P_SCALE = 10,
};

enum ParamInputType {
  PI_TEXT = 0,
  PI_ONOFF = 1,
  PI_SEL_C = 2,
};

struct param_ptr {
  const char *name;
  enum ParamType type;
  enum ParamInputType inputtype;
  void *varptr;
  const char *comment;
  void *select;
};

struct param_section {
  char *name;
  struct param_ptr *params;
};

struct rc_search_table {
  struct param_ptr *param;
  short uniq_pos;
};

static struct rc_search_table *RC_search_table;
static int RC_table_size;

/* FIXME: gettextize here */

#define CMT_HELPER N_("External Viewer Setup")
#define CMT_TABSTOP N_("Tab width in characters")
#define CMT_INDENT_INCR N_("Indent for HTML rendering")
#define CMT_PIXEL_PER_CHAR N_("Number of pixels per character (4.0...32.0)")
#define CMT_PIXEL_PER_LINE N_("Number of pixels per line (4.0...64.0)")
#define CMT_PAGERLINE N_("Number of remembered lines when used as a pager")
#define CMT_HISTORY N_("Use URL history")
#define CMT_HISTSIZE N_("Number of remembered URL")
#define CMT_SAVEHIST N_("Save URL history")
#define CMT_FRAME N_("Render frames automatically")
#define CMT_ARGV_IS_URL N_("Treat argument without scheme as URL")
#define CMT_TSELF N_("Use _self as default target")
#define CMT_OPEN_TAB_BLANK                                                     \
  N_("Open link on new tab if target is _blank or _new")
#define CMT_OPEN_TAB_DL_LIST N_("Open download list panel on new tab")
#define CMT_DISPLINK N_("Display link URL automatically")
#define CMT_DISPLINKNUMBER N_("Display link numbers")
#define CMT_DECODE_URL N_("Display decoded URL")
#define CMT_DISPLINEINFO N_("Display current line number")
#define CMT_DISP_IMAGE N_("Display inline images")
#define CMT_PSEUDO_INLINES                                                     \
  N_("Display pseudo-ALTs for inline images with no ALT or TITLE string")
#define CMT_MULTICOL N_("Display file names in multi-column format")
#define CMT_ALT_ENTITY N_("Use ASCII equivalents to display entities")
#define CMT_GRAPHIC_CHAR N_("Character type for border of table and menu")
#define CMT_DISP_BORDERS N_("Display table borders, ignore value of BORDER")
#define CMT_DISABLE_CENTER N_("Disable center alignment")
#define CMT_FOLD_TEXTAREA N_("Fold lines in TEXTAREA")
#define CMT_DISP_INS_DEL N_("Display INS, DEL, S and STRIKE element")
#define CMT_COLOR N_("Display with color")
#define CMT_B_COLOR N_("Color of normal character")
#define CMT_A_COLOR N_("Color of anchor")
#define CMT_I_COLOR N_("Color of image link")
#define CMT_F_COLOR N_("Color of form")
#define CMT_ACTIVE_STYLE N_("Enable coloring of active link")
#define CMT_C_COLOR N_("Color of currently active link")
#define CMT_VISITED_ANCHOR N_("Use visited link color")
#define CMT_V_COLOR N_("Color of visited link")
#define CMT_BG_COLOR N_("Color of background")
#define CMT_MARK_COLOR N_("Color of mark")
#define CMT_USE_PROXY N_("Use proxy")
#define CMT_HTTP_PROXY N_("URL of HTTP proxy host")
#define CMT_HTTPS_PROXY N_("URL of HTTPS proxy host")
#define CMT_FTP_PROXY N_("URL of FTP proxy host")
#define CMT_NO_PROXY N_("Domains to be accessed directly (no proxy)")
#define CMT_NOPROXY_NETADDR N_("Check noproxy by network address")
#define CMT_NO_CACHE N_("Disable cache")
#define CMT_DNS_ORDER N_("Order of name resolution")
#define CMT_DROOT N_("Directory corresponding to / (document root)")
#define CMT_PDROOT N_("Directory corresponding to /~user")
#define CMT_CGIBIN N_("Directory corresponding to /cgi-bin")
#define CMT_CONFIRM_QQ N_("Confirm when quitting with q")
#define CMT_CLOSE_TAB_BACK N_("Close tab if buffer is last when back")
#define CMT_EMACS_LIKE_LINEEDIT N_("Enable Emacs-style line editing")
#define CMT_SPACE_AUTOCOMPLETE                                                 \
  N_("Space key triggers file completion while editing URLs")
#define CMT_VI_PREC_NUM N_("Enable vi-like numeric prefix")
#define CMT_LABEL_TOPLINE N_("Move cursor to top line when going to label")
#define CMT_NEXTPAGE_TOPLINE                                                   \
  N_("Move cursor to top line when moving to next page")
#define CMT_FOLD_LINE N_("Fold lines of plain text file")
#define CMT_SHOW_NUM N_("Show line numbers")
#define CMT_SHOW_SRCH_STR N_("Show search string")
#define CMT_MIMETYPES N_("List of mime.types files")
#define CMT_MAILCAP N_("List of mailcap files")
#define CMT_URIMETHODMAP N_("List of urimethodmap files")
#define CMT_EDITOR N_("Editor")
#define CMT_MAILER N_("Mailer")
#define CMT_MAILTO_OPTIONS N_("How to call Mailer for mailto URLs with options")
#define CMT_EXTBRZ N_("External browser")
#define CMT_EXTBRZ2 N_("2nd external browser")
#define CMT_EXTBRZ3 N_("3rd external browser")
#define CMT_EXTBRZ4 N_("4th external browser")
#define CMT_EXTBRZ5 N_("5th external browser")
#define CMT_EXTBRZ6 N_("6th external browser")
#define CMT_EXTBRZ7 N_("7th external browser")
#define CMT_EXTBRZ8 N_("8th external browser")
#define CMT_EXTBRZ9 N_("9th external browser")
#define CMT_DISABLE_SECRET_SECURITY_CHECK                                      \
  N_("Disable secret file security check")
#define CMT_PASSWDFILE N_("Password file")
#define CMT_PRE_FORM_FILE N_("File for setting form on loading")
#define CMT_SITECONF_FILE N_("File for preferences for each site")
#define CMT_FTPPASS N_("Password for anonymous FTP (your mail address)")
#define CMT_FTPPASS_HOSTNAMEGEN N_("Generate domain part of password for FTP")
#define CMT_USERAGENT N_("User-Agent identification string")
#define CMT_ACCEPTENCODING N_("Accept-Encoding header")
#define CMT_ACCEPTMEDIA N_("Accept header")
#define CMT_ACCEPTLANG N_("Accept-Language header")
#define CMT_MARK_ALL_PAGES N_("Treat URL-like strings as links in all pages")
#define CMT_WRAP N_("Wrap search")
#define CMT_VIEW_UNSEENOBJECTS N_("Display unseen objects (e.g. bgimage tag)")
#define CMT_AUTO_UNCOMPRESS                                                    \
  N_("Uncompress compressed data automatically when downloading")
#define CMT_BGEXTVIEW N_("Run external viewer in the background")
#define CMT_EXT_DIRLIST N_("Use external program for directory listing")
#define CMT_DIRLIST_CMD N_("URL of directory listing command")
#define CMT_USE_DICTCOMMAND N_("Enable dictionary lookup through CGI")
#define CMT_DICTCOMMAND N_("URL of dictionary lookup command")
#define CMT_IGNORE_NULL_IMG_ALT N_("Display link name for images lacking ALT")
#define CMT_IFILE N_("Index file for directories")
#define CMT_RETRY_HTTP N_("Prepend http:// to URL automatically")
#define CMT_DEFAULT_URL N_("Default value for open-URL command")
#define CMT_DECODE_CTE N_("Decode Content-Transfer-Encoding when saving")
#define CMT_PRESERVE_TIMESTAMP N_("Preserve timestamp when saving")
#define CMT_CLEAR_BUF N_("Free memory of undisplayed buffers")
#define CMT_NOSENDREFERER N_("Suppress `Referer:' header")
#define CMT_CROSSORIGINREFERER                                                 \
  N_("Exclude pathname and query string from `Referer:' header when cross "    \
     "domain communication")
#define CMT_IGNORE_CASE N_("Search case-insensitively")
#define CMT_USE_LESSOPEN N_("Use LESSOPEN")
#define CMT_SSL_VERIFY_SERVER N_("Perform SSL server verification")
#define CMT_SSL_CERT_FILE N_("PEM encoded certificate file of client")
#define CMT_SSL_KEY_FILE N_("PEM encoded private key file of client")
#define CMT_SSL_CA_PATH                                                        \
  N_("Path to directory for PEM encoded certificates of CAs")
#define CMT_SSL_CA_FILE N_("File consisting of PEM encoded certificates of CAs")
#define CMT_SSL_CA_DEFAULT                                                     \
  N_("Use default locations for PEM encoded certificates of CAs")
#define CMT_SSL_FORBID_METHOD                                                  \
  N_("List of forbidden SSL methods (2: SSLv2, 3: SSLv3, t: TLSv1.0, 5: "      \
     "TLSv1.1, 6: TLSv1.2, 7: TLSv1.3)")
#ifdef SSL_CTX_set_min_proto_version
#define CMT_SSL_MIN_VERSION                                                    \
  N_("Minimum SSL version (all, TLSv1.0, TLSv1.1, TLSv1.2, or TLSv1.3)")
#endif
#define CMT_SSL_CIPHER                                                         \
  N_("SSL ciphers for TLSv1.2 and below (e.g. DEFAULT:@SECLEVEL=2)")
#define CMT_USECOOKIE N_("Enable cookie processing")
#define CMT_SHOWCOOKIE N_("Print a message when receiving a cookie")
#define CMT_ACCEPTCOOKIE N_("Accept cookies")
#define CMT_ACCEPTBADCOOKIE N_("Action to be taken on invalid cookie")
#define CMT_COOKIE_REJECT_DOMAINS N_("Domains to reject cookies from")
#define CMT_COOKIE_ACCEPT_DOMAINS N_("Domains to accept cookies from")
#define CMT_COOKIE_AVOID_WONG_NUMBER_OF_DOTS                                   \
  N_("Domains to avoid [wrong number of dots]")
#define CMT_FOLLOW_REDIRECTION N_("Number of redirections to follow")
#define CMT_META_REFRESH N_("Enable processing of meta-refresh tag")
#define CMT_LOCALHOST_ONLY N_("Restrict connections only to localhost")

#define CMT_KEYMAP_FILE N_("keymap file")

struct sel_c {
  int value;
  char *cvalue;
  char *text;
};

#if 1 /* ANSI-C ? */
#define N_STR(x) #x
#define N_S(x) (x), N_STR(x)
#else /* for traditional cpp? */
static char n_s[][2] = {
    {'0', 0},
    {'1', 0},
    {'2', 0},
};
#define N_S(x) (x), n_s[(x)]
#endif

#define _(Text) Text
#define N_(Text) Text
#define gettext(Text) Text

static struct sel_c defaulturls[] = {
    {N_S(DEFAULT_URL_EMPTY), N_("none")},
    {N_S(DEFAULT_URL_CURRENT), N_("current URL")},
    {N_S(DEFAULT_URL_LINK), N_("link URL")},
    {0, NULL, NULL}};

static struct sel_c displayinsdel[] = {
    {N_S(DISPLAY_INS_DEL_SIMPLE), N_("simple")},
    {N_S(DISPLAY_INS_DEL_NORMAL), N_("use tag")},
    {N_S(DISPLAY_INS_DEL_FONTIFY), N_("fontify")},
    {0, NULL, NULL}};

#ifdef INET6
static struct sel_c dnsorders[] = {
    {N_S(DNS_ORDER_UNSPEC), N_("unspecified")},
    {N_S(DNS_ORDER_INET_INET6), N_("inet inet6")},
    {N_S(DNS_ORDER_INET6_INET), N_("inet6 inet")},
    {N_S(DNS_ORDER_INET_ONLY), N_("inet only")},
    {N_S(DNS_ORDER_INET6_ONLY), N_("inet6 only")},
    {0, NULL, NULL}};
#endif /* INET6 */

static struct sel_c badcookiestr[] = {
    {N_S(ACCEPT_BAD_COOKIE_DISCARD), N_("discard")},
    {N_S(ACCEPT_BAD_COOKIE_ASK), N_("ask")},
    {0, NULL, NULL}};

static struct sel_c mailtooptionsstr[] = {
    {N_S(MAILTO_OPTIONS_IGNORE), N_("ignore options and use only the address")},
    {N_S(MAILTO_OPTIONS_USE_MAILTO_URL), N_("use full mailto URL")},
    {0, NULL, NULL}};

static struct sel_c graphic_char_str[] = {
    {N_S(GRAPHIC_CHAR_ASCII), N_("ASCII")},
    {N_S(GRAPHIC_CHAR_CHARSET), N_("charset specific")},
    {N_S(GRAPHIC_CHAR_DEC), N_("DEC special graphics")},
    {0, NULL, NULL}};

struct param_ptr params1[] = {
    {"tabstop", P_NZINT, PI_TEXT, (void *)&Tabstop, CMT_TABSTOP, NULL},
    {"indent_incr", P_NZINT, PI_TEXT, (void *)&IndentIncr, CMT_INDENT_INCR,
     NULL},
    {"pixel_per_char", P_PIXELS, PI_TEXT, (void *)&pixel_per_char,
     CMT_PIXEL_PER_CHAR, NULL},
    // {"frame", P_CHARINT, PI_ONOFF, (void *)&RenderFrame, CMT_FRAME, NULL},
    {"target_self", P_CHARINT, PI_ONOFF, (void *)&TargetSelf, CMT_TSELF, NULL},
    {"open_tab_blank", P_INT, PI_ONOFF, (void *)&open_tab_blank,
     CMT_OPEN_TAB_BLANK, NULL},
    {"open_tab_dl_list", P_INT, PI_ONOFF, (void *)&open_tab_dl_list,
     CMT_OPEN_TAB_DL_LIST, NULL},
    {"display_link", P_INT, PI_ONOFF, (void *)&displayLink, CMT_DISPLINK, NULL},
    {"display_link_number", P_INT, PI_ONOFF, (void *)&displayLinkNumber,
     CMT_DISPLINKNUMBER, NULL},
    {"decode_url", P_INT, PI_ONOFF, (void *)&DecodeURL, CMT_DECODE_URL, NULL},
    {"display_lineinfo", P_INT, PI_ONOFF, (void *)&displayLineInfo,
     CMT_DISPLINEINFO, NULL},
    {"ext_dirlist", P_INT, PI_ONOFF, (void *)&UseExternalDirBuffer,
     CMT_EXT_DIRLIST, NULL},
    {"dirlist_cmd", P_STRING, PI_TEXT, (void *)&DirBufferCommand,
     CMT_DIRLIST_CMD, NULL},
    {"use_dictcommand", P_INT, PI_ONOFF, (void *)&UseDictCommand,
     CMT_USE_DICTCOMMAND, NULL},
    {"dictcommand", P_STRING, PI_TEXT, (void *)&DictCommand, CMT_DICTCOMMAND,
     NULL},
    {"multicol", P_INT, PI_ONOFF, (void *)&multicolList, CMT_MULTICOL, NULL},
    {"alt_entity", P_CHARINT, PI_ONOFF, (void *)&UseAltEntity, CMT_ALT_ENTITY,
     NULL},
    {"graphic_char", P_CHARINT, PI_SEL_C, (void *)&UseGraphicChar,
     CMT_GRAPHIC_CHAR, (void *)graphic_char_str},
    {"display_borders", P_CHARINT, PI_ONOFF, (void *)&DisplayBorders,
     CMT_DISP_BORDERS, NULL},
    {"disable_center", P_CHARINT, PI_ONOFF, (void *)&DisableCenter,
     CMT_DISABLE_CENTER, NULL},
    {"fold_textarea", P_CHARINT, PI_ONOFF, (void *)&FoldTextarea,
     CMT_FOLD_TEXTAREA, NULL},
    {"display_ins_del", P_INT, PI_SEL_C, (void *)&displayInsDel,
     CMT_DISP_INS_DEL, displayinsdel},
    {"ignore_null_img_alt", P_INT, PI_ONOFF, (void *)&ignore_null_img_alt,
     CMT_IGNORE_NULL_IMG_ALT, NULL},
    {"view_unseenobject", P_INT, PI_ONOFF, (void *)&view_unseenobject,
     CMT_VIEW_UNSEENOBJECTS, NULL},
    /* XXX: emacs-w3m force to off display_image even if image options off */
    {"display_image", P_INT, PI_ONOFF, (void *)&displayImage, CMT_DISP_IMAGE,
     NULL},
    {"pseudo_inlines", P_INT, PI_ONOFF, (void *)&pseudoInlines,
     CMT_PSEUDO_INLINES, NULL},
    {"fold_line", P_INT, PI_ONOFF, (void *)&FoldLine, CMT_FOLD_LINE, NULL},
    {"show_lnum", P_INT, PI_ONOFF, (void *)&showLineNum, CMT_SHOW_NUM, NULL},
    {"show_srch_str", P_INT, PI_ONOFF, (void *)&show_srch_str,
     CMT_SHOW_SRCH_STR, NULL},
    {"label_topline", P_INT, PI_ONOFF, (void *)&label_topline,
     CMT_LABEL_TOPLINE, NULL},
    {"nextpage_topline", P_INT, PI_ONOFF, (void *)&nextpage_topline,
     CMT_NEXTPAGE_TOPLINE, NULL},
    {NULL, 0, 0, NULL, NULL, NULL},
};

struct param_ptr params3[] = {
    {"use_history", P_INT, PI_ONOFF, (void *)&UseHistory, CMT_HISTORY, NULL},
    {"history", P_INT, PI_TEXT, (void *)&URLHistSize, CMT_HISTSIZE, NULL},
    {"save_hist", P_INT, PI_ONOFF, (void *)&SaveURLHist, CMT_SAVEHIST, NULL},
    {"confirm_qq", P_INT, PI_ONOFF, (void *)&confirm_on_quit, CMT_CONFIRM_QQ,
     NULL},
    {"close_tab_back", P_INT, PI_ONOFF, (void *)&close_tab_back,
     CMT_CLOSE_TAB_BACK, NULL},
    {"emacs_like_lineedit", P_INT, PI_ONOFF, (void *)&emacs_like_lineedit,
     CMT_EMACS_LIKE_LINEEDIT, NULL},
    {"space_autocomplete", P_INT, PI_ONOFF, (void *)&space_autocomplete,
     CMT_SPACE_AUTOCOMPLETE, NULL},
    {"vi_prec_num", P_INT, PI_ONOFF, (void *)&vi_prec_num, CMT_VI_PREC_NUM,
     NULL},
    {"mark_all_pages", P_INT, PI_ONOFF, (void *)&MarkAllPages,
     CMT_MARK_ALL_PAGES, NULL},
    {"wrap_search", P_INT, PI_ONOFF, (void *)&WrapDefault, CMT_WRAP, NULL},
    {"ignorecase_search", P_INT, PI_ONOFF, (void *)&IgnoreCase, CMT_IGNORE_CASE,
     NULL},
    {"clear_buffer", P_INT, PI_ONOFF, (void *)&clear_buffer, CMT_CLEAR_BUF,
     NULL},
    {"decode_cte", P_CHARINT, PI_ONOFF, (void *)&DecodeCTE, CMT_DECODE_CTE,
     NULL},
    {"auto_uncompress", P_CHARINT, PI_ONOFF, (void *)&AutoUncompress,
     CMT_AUTO_UNCOMPRESS, NULL},
    {"preserve_timestamp", P_CHARINT, PI_ONOFF, (void *)&PreserveTimestamp,
     CMT_PRESERVE_TIMESTAMP, NULL},
    {"keymap_file", P_STRING, PI_TEXT, (void *)&keymap_file, CMT_KEYMAP_FILE,
     NULL},
    {NULL, 0, 0, NULL, NULL, NULL},
};

struct param_ptr params4[] = {
    {"use_proxy", P_CHARINT, PI_ONOFF, (void *)&use_proxy, CMT_USE_PROXY, NULL},
    {"http_proxy", P_STRING, PI_TEXT, (void *)&HTTP_proxy, CMT_HTTP_PROXY,
     NULL},
    {"https_proxy", P_STRING, PI_TEXT, (void *)&HTTPS_proxy, CMT_HTTPS_PROXY,
     NULL},
    {"ftp_proxy", P_STRING, PI_TEXT, (void *)&FTP_proxy, CMT_FTP_PROXY, NULL},
    {"no_proxy", P_STRING, PI_TEXT, (void *)&NO_proxy, CMT_NO_PROXY, NULL},
    {"noproxy_netaddr", P_INT, PI_ONOFF, (void *)&NOproxy_netaddr,
     CMT_NOPROXY_NETADDR, NULL},
    {"no_cache", P_CHARINT, PI_ONOFF, (void *)&NoCache, CMT_NO_CACHE, NULL},

    {NULL, 0, 0, NULL, NULL, NULL},
};

struct param_ptr params5[] = {
    {"document_root", P_STRING, PI_TEXT, (void *)&document_root, CMT_DROOT,
     NULL},
    {"personal_document_root", P_STRING, PI_TEXT,
     (void *)&personal_document_root, CMT_PDROOT, NULL},
    {"cgi_bin", P_STRING, PI_TEXT, (void *)&cgi_bin, CMT_CGIBIN, NULL},
    {"index_file", P_STRING, PI_TEXT, (void *)&index_file, CMT_IFILE, NULL},
    {NULL, 0, 0, NULL, NULL, NULL},
};

struct param_ptr params6[] = {
    {"mime_types", P_STRING, PI_TEXT, (void *)&mimetypes_files, CMT_MIMETYPES,
     NULL},
    {"mailcap", P_STRING, PI_TEXT, (void *)&mailcap_files, CMT_MAILCAP, NULL},
    {"editor", P_STRING, PI_TEXT, (void *)&Editor, CMT_EDITOR, NULL},
    {"mailto_options", P_INT, PI_SEL_C, (void *)&MailtoOptions,
     CMT_MAILTO_OPTIONS, (void *)mailtooptionsstr},
    {"mailer", P_STRING, PI_TEXT, (void *)&Mailer, CMT_MAILER, NULL},
    {"extbrowser", P_STRING, PI_TEXT, (void *)&ExtBrowser, CMT_EXTBRZ, NULL},
    {"extbrowser2", P_STRING, PI_TEXT, (void *)&ExtBrowser2, CMT_EXTBRZ2, NULL},
    {"extbrowser3", P_STRING, PI_TEXT, (void *)&ExtBrowser3, CMT_EXTBRZ3, NULL},
    {"extbrowser4", P_STRING, PI_TEXT, (void *)&ExtBrowser4, CMT_EXTBRZ4, NULL},
    {"extbrowser5", P_STRING, PI_TEXT, (void *)&ExtBrowser5, CMT_EXTBRZ5, NULL},
    {"extbrowser6", P_STRING, PI_TEXT, (void *)&ExtBrowser6, CMT_EXTBRZ6, NULL},
    {"extbrowser7", P_STRING, PI_TEXT, (void *)&ExtBrowser7, CMT_EXTBRZ7, NULL},
    {"extbrowser8", P_STRING, PI_TEXT, (void *)&ExtBrowser8, CMT_EXTBRZ8, NULL},
    {"extbrowser9", P_STRING, PI_TEXT, (void *)&ExtBrowser9, CMT_EXTBRZ9, NULL},
    {"bgextviewer", P_INT, PI_ONOFF, (void *)&BackgroundExtViewer,
     CMT_BGEXTVIEW, NULL},
    {"use_lessopen", P_INT, PI_ONOFF, (void *)&use_lessopen, CMT_USE_LESSOPEN,
     NULL},
    {NULL, 0, 0, NULL, NULL, NULL},
};

struct param_ptr params7[] = {
    {"ssl_forbid_method", P_STRING, PI_TEXT, (void *)&ssl_forbid_method,
     CMT_SSL_FORBID_METHOD, NULL},
#ifdef SSL_CTX_set_min_proto_version
    {"ssl_min_version", P_STRING, PI_TEXT, (void *)&ssl_min_version,
     CMT_SSL_MIN_VERSION, NULL},
#endif
    {"ssl_cipher", P_STRING, PI_TEXT, (void *)&ssl_cipher, CMT_SSL_CIPHER,
     NULL},
    {"ssl_verify_server", P_INT, PI_ONOFF, (void *)&ssl_verify_server,
     CMT_SSL_VERIFY_SERVER, NULL},
    {"ssl_cert_file", P_SSLPATH, PI_TEXT, (void *)&ssl_cert_file,
     CMT_SSL_CERT_FILE, NULL},
    {"ssl_key_file", P_SSLPATH, PI_TEXT, (void *)&ssl_key_file,
     CMT_SSL_KEY_FILE, NULL},
    {"ssl_ca_path", P_SSLPATH, PI_TEXT, (void *)&ssl_ca_path, CMT_SSL_CA_PATH,
     NULL},
    {"ssl_ca_file", P_SSLPATH, PI_TEXT, (void *)&ssl_ca_file, CMT_SSL_CA_FILE,
     NULL},
    {"ssl_ca_default", P_INT, PI_ONOFF, (void *)&ssl_ca_default,
     CMT_SSL_CA_DEFAULT, NULL},
    {NULL, 0, 0, NULL, NULL, NULL},
};

struct param_ptr params8[] = {
    {"use_cookie", P_INT, PI_ONOFF, (void *)&use_cookie, CMT_USECOOKIE, NULL},
    {"show_cookie", P_INT, PI_ONOFF, (void *)&show_cookie, CMT_SHOWCOOKIE,
     NULL},
    {"accept_cookie", P_INT, PI_ONOFF, (void *)&accept_cookie, CMT_ACCEPTCOOKIE,
     NULL},
    {"accept_bad_cookie", P_INT, PI_SEL_C, (void *)&accept_bad_cookie,
     CMT_ACCEPTBADCOOKIE, (void *)badcookiestr},
    {"cookie_reject_domains", P_STRING, PI_TEXT, (void *)&cookie_reject_domains,
     CMT_COOKIE_REJECT_DOMAINS, NULL},
    {"cookie_accept_domains", P_STRING, PI_TEXT, (void *)&cookie_accept_domains,
     CMT_COOKIE_ACCEPT_DOMAINS, NULL},
    {"cookie_avoid_wrong_number_of_dots", P_STRING, PI_TEXT,
     (void *)&cookie_avoid_wrong_number_of_dots,
     CMT_COOKIE_AVOID_WONG_NUMBER_OF_DOTS, NULL},
    {NULL, 0, 0, NULL, NULL, NULL},
};

struct param_ptr params9[] = {
    {"passwd_file", P_STRING, PI_TEXT, (void *)&passwd_file, CMT_PASSWDFILE,
     NULL},
    {"disable_secret_security_check", P_INT, PI_ONOFF,
     (void *)&disable_secret_security_check, CMT_DISABLE_SECRET_SECURITY_CHECK,
     NULL},
    {"ftppasswd", P_STRING, PI_TEXT, (void *)&ftppasswd, CMT_FTPPASS, NULL},
    {"ftppass_hostnamegen", P_INT, PI_ONOFF, (void *)&ftppass_hostnamegen,
     CMT_FTPPASS_HOSTNAMEGEN, NULL},
    {"pre_form_file", P_STRING, PI_TEXT, (void *)&pre_form_file,
     CMT_PRE_FORM_FILE, NULL},
    {"siteconf_file", P_STRING, PI_TEXT, (void *)&siteconf_file,
     CMT_SITECONF_FILE, NULL},
    {"user_agent", P_STRING, PI_TEXT, (void *)&UserAgent, CMT_USERAGENT, NULL},
    {"no_referer", P_INT, PI_ONOFF, (void *)&NoSendReferer, CMT_NOSENDREFERER,
     NULL},
    {"cross_origin_referer", P_INT, PI_ONOFF, (void *)&CrossOriginReferer,
     CMT_CROSSORIGINREFERER, NULL},
    {"accept_language", P_STRING, PI_TEXT, (void *)&AcceptLang, CMT_ACCEPTLANG,
     NULL},
    {"accept_encoding", P_STRING, PI_TEXT, (void *)&AcceptEncoding,
     CMT_ACCEPTENCODING, NULL},
    {"accept_media", P_STRING, PI_TEXT, (void *)&AcceptMedia, CMT_ACCEPTMEDIA,
     NULL},
    {"argv_is_url", P_CHARINT, PI_ONOFF, (void *)&ArgvIsURL, CMT_ARGV_IS_URL,
     NULL},
    {"retry_http", P_INT, PI_ONOFF, (void *)&retryAsHttp, CMT_RETRY_HTTP, NULL},
    {"default_url", P_INT, PI_SEL_C, (void *)&DefaultURLString, CMT_DEFAULT_URL,
     (void *)defaulturls},
    {"follow_redirection", P_INT, PI_TEXT, &FollowRedirection,
     CMT_FOLLOW_REDIRECTION, NULL},
    {"meta_refresh", P_INT, PI_ONOFF, (void *)&MetaRefresh, CMT_META_REFRESH,
     NULL},
    {"localhost_only", P_CHARINT, PI_ONOFF, (void *)&LocalhostOnly,
     CMT_LOCALHOST_ONLY, NULL},
#ifdef INET6
    {"dns_order", P_INT, PI_SEL_C, (void *)&DNS_order, CMT_DNS_ORDER,
     (void *)dnsorders},
#endif /* INET6 */
    {NULL, 0, 0, NULL, NULL, NULL},
};

struct param_section sections[] = {{N_("Display Settings"), params1},
                                   {N_("Miscellaneous Settings"), params3},
                                   {N_("Directory Settings"), params5},
                                   {N_("External Program Settings"), params6},
                                   {N_("Network Settings"), params9},
                                   {N_("Proxy Settings"), params4},
                                   {N_("SSL Settings"), params7},
                                   {N_("Cookie Settings"), params8},
                                   {NULL, NULL}};

static Str to_str(struct param_ptr *p);

static int compare_table(struct rc_search_table *a, struct rc_search_table *b) {
  return strcmp(a->param->name, b->param->name);
}

static void create_option_search_table() {
  int i, j, k;
  int diff1, diff2;
  const char *p, *q;

  /* count table size */
  RC_table_size = 0;
  for (j = 0; sections[j].name != NULL; j++) {
    i = 0;
    while (sections[j].params[i].name) {
      i++;
      RC_table_size++;
    }
  }

  RC_search_table = New_N(struct rc_search_table, RC_table_size);
  k = 0;
  for (j = 0; sections[j].name != NULL; j++) {
    i = 0;
    while (sections[j].params[i].name) {
      RC_search_table[k].param = &sections[j].params[i];
      k++;
      i++;
    }
  }

  qsort(RC_search_table, RC_table_size, sizeof(struct rc_search_table),
        (int (*)(const void *, const void *))compare_table);

  diff2 = 0;
  for (i = 0; i < RC_table_size - 1; i++) {
    p = RC_search_table[i].param->name;
    q = RC_search_table[i + 1].param->name;
    for (j = 0; p[j] != '\0' && q[j] != '\0' && p[j] == q[j]; j++)
      ;
    diff1 = j;
    if (diff1 > diff2)
      RC_search_table[i].uniq_pos = diff1 + 1;
    else
      RC_search_table[i].uniq_pos = diff2 + 1;
    diff2 = diff1;
  }
}

static struct param_ptr *search_param(const char *name) {
  size_t b, e, i;
  int cmp;
  int len = strlen(name);

  for (b = 0, e = RC_table_size - 1; b <= e;) {
    i = (b + e) / 2;
    cmp = strncmp(name, RC_search_table[i].param->name, len);

    if (!cmp) {
      if (len >= RC_search_table[i].uniq_pos) {
        return RC_search_table[i].param;
      } else {
        while ((cmp = strcmp(name, RC_search_table[i].param->name)) <= 0)
          if (!cmp)
            return RC_search_table[i].param;
          else if (i == 0)
            return NULL;
          else
            i--;
        /* ambiguous */
        return NULL;
      }
    } else if (cmp < 0) {
      if (i == 0)
        return NULL;
      e = i - 1;
    } else
      b = i + 1;
  }
  return NULL;
}

/* show parameter with bad options invokation */
void show_params(FILE *fp) {
  int i, j, l;
  const char *t = "";
  const char *cmt;

  fputs("\nconfiguration parameters\n", fp);
  for (j = 0; sections[j].name != NULL; j++) {
    cmt = sections[j].name;
    fprintf(fp, "  section[%d]: %s\n", j, cmt);
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
      case P_PIXELS:
        t = "number";
        break;
      case P_SCALE:
        t = "percent";
        break;
      }
      cmt = sections[j].params[i].comment;
      l = 30 - (strlen(sections[j].params[i].name) + strlen(t));
      if (l < 0)
        l = 1;
      fprintf(fp, "    -o %s=<%s>%*s%s\n", sections[j].params[i].name, t, l,
              " ", cmt);
      i++;
    }
  }
}

bool str_to_bool(const char *value, bool old) {
  if (value == NULL)
    return 1;
  switch (TOLOWER(*value)) {
  case '0':
  case 'f': /* false */
  case 'n': /* no */
  case 'u': /* undef */
    return 0;
  case 'o':
    if (TOLOWER(value[1]) == 'f') /* off */
      return 0;
    return 1; /* on */
  case 't':
    if (TOLOWER(value[1]) == 'o') /* toggle */
      return !old;
    return 1; /* true */
  case '!':
  case 'r': /* reverse */
  case 'x': /* exchange */
    return !old;
  }
  return 1;
}

static int set_param(const char *name, const char *value) {
  struct param_ptr *p;
  double ppc;

  if (value == NULL)
    return 0;
  p = search_param(name);
  if (p == NULL)
    return 0;
  switch (p->type) {
  case P_INT:
    if (atoi(value) >= 0) {
      if (p->inputtype == PI_ONOFF) {
        if (strcmp("show_srch_str", name) == 0) {
          auto a = 0;
        }
        *((bool *)p->varptr) = str_to_bool(value, *(bool *)p->varptr);
      } else {
        *(int *)p->varptr = atoi(value);
      }
    }
    break;
  case P_NZINT:
    if (atoi(value) > 0)
      *(int *)p->varptr = atoi(value);
    break;
  case P_SHORT:
    *(short *)p->varptr = (p->inputtype == PI_ONOFF)
                              ? str_to_bool(value, *(short *)p->varptr)
                              : atoi(value);
    break;
  case P_CHARINT:
    *(char *)p->varptr = (p->inputtype == PI_ONOFF)
                             ? str_to_bool(value, *(char *)p->varptr)
                             : atoi(value);
    break;
  case P_CHAR:
    *(char *)p->varptr = value[0];
    break;
  case P_STRING:
    *(const char **)p->varptr = value;
    break;
  case P_SSLPATH:
    if (value != NULL && value[0] != '\0')
      *(const char **)p->varptr = rcFile(value);
    else
      *(char **)p->varptr = NULL;
    ssl_path_modified = 1;
    break;
  case P_PIXELS:
    ppc = atof(value);
    if (ppc >= MINIMUM_PIXEL_PER_CHAR && ppc <= MAXIMUM_PIXEL_PER_CHAR * 2)
      *(double *)p->varptr = ppc;
    break;
  case P_SCALE:
    ppc = atof(value);
    if (ppc >= 10 && ppc <= 1000)
      *(double *)p->varptr = ppc;
    break;
  }
  return 1;
}

int set_param_option(const char *option) {
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
  if (set_param(tmp->ptr, p))
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
  if (set_param(q, "0"))
    goto option_assigned;
  return 0;
option_assigned:
  return 1;
}

const char *get_param_option(const char *name) {
  struct param_ptr *p;
  p = search_param(name);
  return p ? to_str(p)->ptr : NULL;
}

static void interpret_rc(FILE *f) {
  Str line;
  Str tmp;
  char *p;

  for (;;) {
    line = Strfgets(f);
    if (line->length == 0) /* end of file */
      break;
    Strchop(line);
    if (line->length == 0) /* blank line */
      continue;
    Strremovefirstspaces(line);
    if (line->ptr[0] == '#') /* comment */
      continue;
    tmp = Strnew();
    p = line->ptr;
    while (*p && !IS_SPACE(*p))
      Strcat_char(tmp, *p++);
    while (*p && IS_SPACE(*p))
      p++;
    Strlower(tmp);
    set_param(tmp->ptr, p);
  }
}

void parse_proxy() {
  if (non_null(HTTP_proxy))
    parseURL(HTTP_proxy, &HTTP_proxy_parsed, NULL);
  if (non_null(HTTPS_proxy))
    parseURL(HTTPS_proxy, &HTTPS_proxy_parsed, NULL);
  if (non_null(FTP_proxy))
    parseURL(FTP_proxy, &FTP_proxy_parsed, NULL);
  if (non_null(NO_proxy))
    set_no_proxy(NO_proxy);
}

void parse_cookie() {
  if (non_null(cookie_reject_domains))
    Cookie_reject_domains = make_domain_list(cookie_reject_domains);
  if (non_null(cookie_accept_domains))
    Cookie_accept_domains = make_domain_list(cookie_accept_domains);
  if (non_null(cookie_avoid_wrong_number_of_dots))
    Cookie_avoid_wrong_number_of_dots_domains =
        make_domain_list(cookie_avoid_wrong_number_of_dots);
}

void sync_with_option(void) {
  WrapSearch = WrapDefault;
  parse_proxy();
  parse_cookie();
  initMimeTypes();
  displayImage = false; /* XXX */
  loadPasswd();
  loadPreForm();
  loadSiteconf();

  if (AcceptLang == NULL || *AcceptLang == '\0') {
    /* TRANSLATORS:
     * AcceptLang default: this is used in Accept-Language: HTTP request
     * header. For example, ja.po should translate it as
     * "ja;q=1.0, en;q=0.5" like that.
     */
    AcceptLang = _("en;q=1.0");
  }
  if (AcceptEncoding == NULL || *AcceptEncoding == '\0')
    AcceptEncoding = acceptableEncoding();

  initKeymap(false);
}

static char optionpanel_src1[] =
    "<html><head><title>Option Setting Panel</title></head><body>\
<h1 align=center>Option Setting Panel<br>(w3m version %s)</b></h1>\
<form method=post action=\"file:///$LIB/" W3MHELPERPANEL_CMDNAME "\">\
<input type=hidden name=mode value=panel>\
<input type=hidden name=cookie value=\"%s\">\
<input type=submit value=\"%s\">\
</form><br>\
<form method=internal action=option>";

static Str optionpanel_str = NULL;

static Str to_str(struct param_ptr *p) {
  switch (p->type) {
  case P_INT:
  case P_NZINT:
    return Sprintf("%d", *(int *)p->varptr);
  case P_SHORT:
    return Sprintf("%d", *(short *)p->varptr);
  case P_CHARINT:
    return Sprintf("%d", *(char *)p->varptr);
  case P_CHAR:
    return Sprintf("%c", *(char *)p->varptr);
  case P_STRING:
  case P_SSLPATH:
    /*  SystemCharset -> InnerCharset */
    return Strnew_charp(*(char **)p->varptr);
  case P_PIXELS:
  case P_SCALE:
    return Sprintf("%g", *(double *)p->varptr);
  }
  /* not reached */
  return NULL;
}

struct Document *load_option_panel() {
  if (optionpanel_str == NULL) {
    optionpanel_str = Sprintf(optionpanel_src1, w3m_version,
                              html_quote(localCookie()->ptr), _(CMT_HELPER));
  }
  auto src = Strdup(optionpanel_str);

  Strcat_charp(src, "<table><tr><td>");
  for (int i = 0; sections[i].name != NULL; i++) {
    Strcat_m_charp(src, "<h1>", sections[i].name, "</h1>", NULL);
    auto p = sections[i].params;
    Strcat_charp(src, "<table width=100% cellpadding=0>");
    while (p->name) {
      Strcat_m_charp(src, "<tr><td>", p->comment, NULL);
      Strcat(src, Sprintf("</td><td width=%d>", (int)(28 * pixel_per_char)));
      switch (p->inputtype) {
      case PI_TEXT:
        Strcat_m_charp(src, "<input type=text name=", p->name, " value=\"",
                       html_quote(to_str(p)->ptr), "\">", NULL);
        break;
      case PI_ONOFF: {
        auto x = atoi(to_str(p)->ptr);
        Strcat_m_charp(src, "<input type=radio name=", p->name, " value=1",
                       (x ? " checked" : ""),
                       ">YES&nbsp;&nbsp;<input type=radio name=", p->name,
                       " value=0", (x ? "" : " checked"), ">NO", NULL);
        break;
      }
      case PI_SEL_C: {
        auto tmp = to_str(p);
        Strcat_m_charp(src, "<select name=", p->name, ">", NULL);
        for (auto s = (struct sel_c *)p->select; s->text != NULL; s++) {
          Strcat_charp(src, "<option value=");
          Strcat(src, Sprintf("%s\n", s->cvalue));
          if ((p->type != P_CHAR && s->value == atoi(tmp->ptr)) ||
              (p->type == P_CHAR && (char)s->value == *(tmp->ptr)))
            Strcat_charp(src, " selected");
          Strcat_char(src, '>');
          Strcat_charp(src, s->text);
        }
        Strcat_charp(src, "</select>");
        break;
      }
      }
      Strcat_charp(src, "</td></tr>\n");
      p++;
    }
    Strcat_charp(
        src, "<tr><td></td><td><p><input type=submit value=\"OK\"></td></tr>");
    Strcat_charp(src, "</table><hr width=50%>");
  }
  Strcat_charp(src, "</table></form></body></html>");
  struct Url url;
  return loadHTML(INIT_BUFFER_WIDTH, src->ptr, url, nullptr, CHARSET_UTF8);
}

void panel_set_option(struct LocalCgiHtml *arg) {

  FILE *f = NULL;
  if (config_file == NULL) {
    disp_message("There's no config file... config not saved", false);
  } else {
    f = fopen(config_file, "wt");
    if (f == NULL) {
      disp_message("Can't write option!", false);
    }
  }

  Str s = Strnew();
  while (arg) {
    /*  InnerCharset -> SystemCharset */
    if (arg->value) {
      auto p = arg->value;
      if (set_param(arg->arg, p)) {
        auto tmp = Sprintf("%s %s\n", arg->arg, p);
        Strcat(tmp, s);
        s = tmp;
      }
    }
    arg = arg->next;
  }
  if (f) {
    fputs(s->ptr, f);
    fclose(f);
  }
  sync_with_option();
  backBf();
}

const char *rcFile(const char *base) {
  if (base && (base[0] == '/' ||
               (base[0] == '.' &&
                (base[1] == '/' || (base[1] == '.' && base[2] == '/'))) ||
               (base[0] == '~' && base[1] == '/')))
    /* /file, ./file, ../file, ~/file */
    return expandPath(base);
  return expandPath(Strnew_m_charp(rc_dir, "/", base, NULL)->ptr);
}

#ifdef _WIN32
#include <direct.h>
#define mkdir(dir, mod) _mkdir(dir)
#endif

/* open config file */
void open_rc() {
  FILE *f;
  if ((f = fopen(etcFile(W3MCONFIG), "rt")) != NULL) {
    interpret_rc(f);
    fclose(f);
  }
  if ((f = fopen(confFile(CONFIG_FILE), "rt")) != NULL) {
    interpret_rc(f);
    fclose(f);
  }
  if (config_file && (f = fopen(config_file, "rt")) != NULL) {
    interpret_rc(f);
    fclose(f);
  }
}

void init_rc(void) {
  if (!rc_dir) {

    auto _rc_dir = allocStr(expandPath(RC_DIR), -1);
    int i = strlen(_rc_dir);
    if (i > 1 && _rc_dir[i - 1] == '/') {
      _rc_dir[i - 1] = '\0';
    }
    rc_dir = _rc_dir;

    struct stat st;
    if (stat(rc_dir, &st) < 0) {
      if (errno == ENOENT) { /* no directory */
        if (mkdir(rc_dir, 0700) < 0) {
          /* fprintf(stderr, "Can't create config directory (%s)!\n", rc_dir);
           */
          goto rc_dir_err;
        } else {
          stat(rc_dir, &st);
        }
      } else {
        /* fprintf(stderr, "Can't open config directory (%s)!\n", rc_dir); */
        goto rc_dir_err;
      }
    }
    if (!S_ISDIR(st.st_mode)) {
      /* not a directory */
      /* fprintf(stderr, "%s is not a directory!\n", rc_dir); */
      goto rc_dir_err;
    }
    if (!(st.st_mode & S_IWUSR)) {
      /* fprintf(stderr, "%s is not writable!\n", rc_dir); */
      goto rc_dir_err;
    }
    no_rc_dir = false;
    set_tmpdir(rc_dir);

    if (config_file == NULL)
      config_file = rcFile(CONFIG_FILE);

    create_option_search_table();
  }

  open_rc();
  return;

rc_dir_err:
  no_rc_dir = true;
  tmpfile_init_no_rcdir(rc_dir);
  create_option_search_table();
  open_rc();
}
