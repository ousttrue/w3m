#include "option.h"
#define N_(Text) Text
#include <sstream>
#include "Str.h"
#include "myctype.h"
#include "option_param.h"
#include "cookie.h"
#include "html/table.h"

// extern
#include "html/readbuffer.h"
#include "tabbuffer.h"
#include "app.h"
#include "url_decode.h"
#include "http_session.h"
#include "buffer.h"

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

#define MAILTO_OPTIONS_IGNORE 1
#define MAILTO_OPTIONS_USE_MAILTO_URL 2

static struct sel_c mailtooptionsstr[] = {
    {N_S(MAILTO_OPTIONS_IGNORE), N_("ignore options and use only the address")},
    {N_S(MAILTO_OPTIONS_USE_MAILTO_URL), N_("use full mailto URL")},
    {0, NULL, NULL}};

int set_param_option(const char *option) {
  Str *tmp = Strnew();
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
  if (Option::instance().set_param(tmp->ptr, p))
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
  if (Option::instance().set_param(q, "0"))
    goto option_assigned;
  return 0;
option_assigned:
  return 1;
}

Option::Option() {
  std::list<std::shared_ptr<param_ptr>> params1 = {
      std::make_shared<param_int>("tabstop", PI_TEXT, &Tabstop,
                                  "Tab width in characters", true),
      std::make_shared<param_int>("indent_incr", PI_TEXT, &IndentIncr,
                                  "Indent for HTML rendering", true),
      std::make_shared<param_pixels>(
          "pixel_per_char", PI_TEXT, &pixel_per_char,
          "Number of pixels per character (4.0...32.0)"),
      std::make_shared<param_bool>(
          "open_tab_blank", &open_tab_blank,
          "Open link on new tab if target is _blank or _new"),
      std::make_shared<param_bool>("open_tab_dl_list", &open_tab_dl_list,
                                   "Open download list panel on new tab"),
      std::make_shared<param_bool>("display_link", &displayLink,
                                   "Display link URL automatically"),
      std::make_shared<param_bool>("display_link_number", &displayLinkNumber,
                                   "Display link numbers"),
      std::make_shared<param_bool>("decode_url", &DecodeURL,
                                   "Display decoded URL"),
      std::make_shared<param_bool>("display_lineinfo", &displayLineInfo,
                                   "Display current line number"),
      std::make_shared<param_bool>(
          "ext_dirlist", &UseExternalDirBuffer,
          "Use external program for directory listing"),
      // {"dirlist_cmd", P_STRING, PI_TEXT, (void *)&DirBufferCommand,
      //  CMT_DIRLIST_CMD, NULL},
      // {"use_dictcommand", P_INT, PI_ONOFF, (void *)&UseDictCommand,
      //  CMT_USE_DICTCOMMAND, NULL},
      // {"dictcommand", P_STRING, PI_TEXT, (void *)&DictCommand,
      // CMT_DICTCOMMAND,
      //  NULL},
      // {"multicol", P_INT, PI_ONOFF, (void *)&multicolList, CMT_MULTICOL,
      // NULL},
      // {"alt_entity", P_CHARINT, PI_ONOFF, (void *)&UseAltEntity,
      // CMT_ALT_ENTITY,
      //  NULL},
      // {"display_borders", P_CHARINT, PI_ONOFF, (void *)&DisplayBorders,
      //  CMT_DISP_BORDERS, NULL},
      // {"disable_center", P_CHARINT, PI_ONOFF, (void *)&DisableCenter,
      //  CMT_DISABLE_CENTER, NULL},
      // {"fold_textarea", P_CHARINT, PI_ONOFF, (void *)&FoldTextarea,
      //  CMT_FOLD_TEXTAREA, NULL},
      // {"display_ins_del", P_INT, PI_SEL_C, (void *)&displayInsDel,
      //  CMT_DISP_INS_DEL, displayinsdel},
      // {"ignore_null_img_alt", P_INT, PI_ONOFF, (void *)&ignore_null_img_alt,
      //  CMT_IGNORE_NULL_IMG_ALT, NULL},
      // {"view_unseenobject", P_INT, PI_ONOFF, (void *)&view_unseenobject,
      //  CMT_VIEW_UNSEENOBJECTS, NULL},
      // /* XXX: emacs-w3m force to off display_image even if image options off
      // */
      // {"display_image", P_INT, PI_ONOFF, (void *)&displayImage,
      // CMT_DISP_IMAGE,
      //  NULL},
      // {"pseudo_inlines", P_INT, PI_ONOFF, (void *)&pseudoInlines,
      //  CMT_PSEUDO_INLINES, NULL},
      // {"show_srch_str", P_INT, PI_ONOFF, (void *)&show_srch_str,
      //  CMT_SHOW_SRCH_STR, NULL},
      // {"label_topline", P_INT, PI_ONOFF, (void *)&label_topline,
      //  CMT_LABEL_TOPLINE, NULL},
      // {"nextpage_topline", P_INT, PI_ONOFF, (void *)&nextpage_topline,
      //  CMT_NEXTPAGE_TOPLINE, NULL},
  };

  std::list<std::shared_ptr<param_ptr>> params3 = {
      // {"use_history", P_INT, PI_ONOFF, (void *)&UseHistory, CMT_HISTORY,
      // NULL},
      // {"history", P_INT, PI_TEXT, (void *)&URLHistSize, CMT_HISTSIZE, NULL},
      // {"save_hist", P_INT, PI_ONOFF, (void *)&SaveURLHist, CMT_SAVEHIST,
      // NULL},
      // {"close_tab_back", P_INT, PI_ONOFF, (void *)&close_tab_back,
      //  CMT_CLOSE_TAB_BACK, NULL},
      // {"emacs_like_lineedit", P_INT, PI_ONOFF, (void *)&emacs_like_lineedit,
      //  CMT_EMACS_LIKE_LINEEDIT, NULL},
      // {"space_autocomplete", P_INT, PI_ONOFF, (void *)&space_autocomplete,
      //  CMT_SPACE_AUTOCOMPLETE, NULL},
      // {"mark_all_pages", P_INT, PI_ONOFF, (void *)&MarkAllPages,
      //  CMT_MARK_ALL_PAGES, NULL},
      // {"wrap_search", P_INT, PI_ONOFF, (void *)&WrapDefault, CMT_WRAP, NULL},
      // {"ignorecase_search", P_INT, PI_ONOFF, (void *)&IgnoreCase,
      //  CMT_IGNORE_CASE, NULL},
      // {"clear_buffer", P_INT, PI_ONOFF, (void *)&clear_buffer, CMT_CLEAR_BUF,
      //  NULL},
      // {"decode_cte", P_CHARINT, PI_ONOFF, (void *)&DecodeCTE, CMT_DECODE_CTE,
      //  NULL},
      // {"auto_uncompress", P_CHARINT, PI_ONOFF, (void *)&AutoUncompress,
      //  CMT_AUTO_UNCOMPRESS, NULL},
      // {"preserve_timestamp", P_CHARINT, PI_ONOFF, (void *)&PreserveTimestamp,
      //  CMT_PRESERVE_TIMESTAMP, NULL},
      // {"keymap_file", P_STRING, PI_TEXT, (void *)&keymap_file,
      // CMT_KEYMAP_FILE,
      //  NULL},
  };

  std::list<std::shared_ptr<param_ptr>> params4 = {
      // {"no_cache", P_CHARINT, PI_ONOFF, (void *)&NoCache, CMT_NO_CACHE,
      // NULL},
  };

  std::list<std::shared_ptr<param_ptr>> params5 = {
      // {"document_root", P_STRING, PI_TEXT, (void *)&document_root, CMT_DROOT,
      //  NULL},
      // {"personal_document_root", P_STRING, PI_TEXT,
      //  (void *)&personal_document_root, CMT_PDROOT, NULL},
      // {"cgi_bin", P_STRING, PI_TEXT, (void *)&cgi_bin, CMT_CGIBIN, NULL},
      // {"index_file", P_STRING, PI_TEXT, (void *)&index_file, CMT_IFILE,
      // NULL},
  };

  std::list<std::shared_ptr<param_ptr>> params6 = {
      // {"mime_types", P_STRING, PI_TEXT, (void *)&mimetypes_files,
      // CMT_MIMETYPES,
      //  NULL},
      // {"mailcap", P_STRING, PI_TEXT, (void *)&mailcap_files, CMT_MAILCAP,
      // NULL},
      // // {"editor", P_STRING, PI_TEXT, (void *)&Editor, CMT_EDITOR, NULL},
      // {"mailto_options", P_INT, PI_SEL_C, (void *)&MailtoOptions,
      //  CMT_MAILTO_OPTIONS, (void *)mailtooptionsstr},
      // {"extbrowser", P_STRING, PI_TEXT, (void *)&ExtBrowser, CMT_EXTBRZ,
      // NULL},
      // {"extbrowser2", P_STRING, PI_TEXT, (void *)&ExtBrowser2, CMT_EXTBRZ2,
      //  NULL},
      // {"extbrowser3", P_STRING, PI_TEXT, (void *)&ExtBrowser3, CMT_EXTBRZ3,
      //  NULL},
      // {"extbrowser4", P_STRING, PI_TEXT, (void *)&ExtBrowser4, CMT_EXTBRZ4,
      //  NULL},
      // {"extbrowser5", P_STRING, PI_TEXT, (void *)&ExtBrowser5, CMT_EXTBRZ5,
      //  NULL},
      // {"extbrowser6", P_STRING, PI_TEXT, (void *)&ExtBrowser6, CMT_EXTBRZ6,
      //  NULL},
      // {"extbrowser7", P_STRING, PI_TEXT, (void *)&ExtBrowser7, CMT_EXTBRZ7,
      //  NULL},
      // {"extbrowser8", P_STRING, PI_TEXT, (void *)&ExtBrowser8, CMT_EXTBRZ8,
      //  NULL},
      // {"extbrowser9", P_STRING, PI_TEXT, (void *)&ExtBrowser9, CMT_EXTBRZ9,
      //  NULL},
      // {"bgextviewer", P_INT, PI_ONOFF, (void *)&BackgroundExtViewer,
      //  CMT_BGEXTVIEW, NULL},
  };

  std::list<std::shared_ptr<param_ptr>> params7 = {
      // {"ssl_forbid_method", P_STRING, PI_TEXT, (void *)&ssl_forbid_method,
      //  CMT_SSL_FORBID_METHOD, NULL},
      // {"ssl_min_version", P_STRING, PI_TEXT, (void *)&ssl_min_version,
      //  CMT_SSL_MIN_VERSION, NULL},
      // {"ssl_cipher", P_STRING, PI_TEXT, (void *)&ssl_cipher, CMT_SSL_CIPHER,
      //  NULL},
      // {"ssl_verify_server", P_INT, PI_ONOFF, (void *)&ssl_verify_server,
      //  CMT_SSL_VERIFY_SERVER, NULL},
      // {"ssl_cert_file", P_SSLPATH, PI_TEXT, (void *)&ssl_cert_file,
      //  CMT_SSL_CERT_FILE, NULL},
      // {"ssl_key_file", P_SSLPATH, PI_TEXT, (void *)&ssl_key_file,
      //  CMT_SSL_KEY_FILE, NULL},
      // {"ssl_ca_path", P_SSLPATH, PI_TEXT, (void *)&ssl_ca_path,
      // CMT_SSL_CA_PATH,
      //  NULL},
      // {"ssl_ca_file", P_SSLPATH, PI_TEXT, (void *)&ssl_ca_file,
      // CMT_SSL_CA_FILE,
      //  NULL},
      // {"ssl_ca_default", P_INT, PI_ONOFF, (void *)&ssl_ca_default,
      //  CMT_SSL_CA_DEFAULT, NULL},
  };

  std::list<std::shared_ptr<param_ptr>> params8 = {
      // {"use_cookie", P_INT, PI_ONOFF, (void *)&use_cookie, CMT_USECOOKIE,
      // NULL},
      // {"show_cookie", P_INT, PI_ONOFF, (void *)&show_cookie, CMT_SHOWCOOKIE,
      //  NULL},
      // {"accept_cookie", P_INT, PI_ONOFF, (void *)&accept_cookie,
      //  CMT_ACCEPTCOOKIE, NULL},
      // {"accept_bad_cookie", P_INT, PI_SEL_C, (void *)&accept_bad_cookie,
      //  CMT_ACCEPTBADCOOKIE, (void *)badcookiestr},
      // {"cookie_reject_domains", P_STRING, PI_TEXT,
      //  (void *)&cookie_reject_domains, CMT_COOKIE_REJECT_DOMAINS, NULL},
      // {"cookie_accept_domains", P_STRING, PI_TEXT,
      //  (void *)&cookie_accept_domains, CMT_COOKIE_ACCEPT_DOMAINS, NULL},
      // {"cookie_avoid_wrong_number_of_dots", P_STRING, PI_TEXT,
      //  (void *)&cookie_avoid_wrong_number_of_dots,
      //  CMT_COOKIE_AVOID_WONG_NUMBER_OF_DOTS, NULL},
  };

  std::list<std::shared_ptr<param_ptr>> params9 = {
      // {"passwd_file", P_STRING, PI_TEXT, (void *)&passwd_file,
      // CMT_PASSWDFILE,
      //  NULL},
      // {"disable_secret_security_check", P_INT, PI_ONOFF,
      //  (void *)&disable_secret_security_check,
      //  CMT_DISABLE_SECRET_SECURITY_CHECK, NULL},
      // {"ftppasswd", P_STRING, PI_TEXT, (void *)&ftppasswd, CMT_FTPPASS,
      // NULL},
      // {"ftppass_hostnamegen", P_INT, PI_ONOFF, (void *)&ftppass_hostnamegen,
      //  CMT_FTPPASS_HOSTNAMEGEN, NULL},
      // {"siteconf_file", P_STRING, PI_TEXT, (void *)&siteconf_file,
      //  CMT_SITECONF_FILE, NULL},
      // {"user_agent", P_STRING, PI_TEXT, (void *)&UserAgent, CMT_USERAGENT,
      //  NULL},
      // {"no_referer", P_INT, PI_ONOFF, (void *)&NoSendReferer,
      // CMT_NOSENDREFERER,
      //  NULL},
      // {"cross_origin_referer", P_INT, PI_ONOFF, (void *)&CrossOriginReferer,
      //  CMT_CROSSORIGINREFERER, NULL},
      // {"accept_language", P_STRING, PI_TEXT, (void *)&AcceptLang,
      //  CMT_ACCEPTLANG, NULL},
      // {"accept_encoding", P_STRING, PI_TEXT, (void *)&AcceptEncoding,
      //  CMT_ACCEPTENCODING, NULL},
      // {"accept_media", P_STRING, PI_TEXT, (void *)&AcceptMedia,
      // CMT_ACCEPTMEDIA,
      //  NULL},
      // {"argv_is_url", P_CHARINT, PI_ONOFF, (void *)&ArgvIsURL,
      // CMT_ARGV_IS_URL,
      //  NULL},
      // {"retry_http", P_INT, PI_ONOFF, (void *)&retryAsHttp, CMT_RETRY_HTTP,
      //  NULL},
      // {"default_url", P_INT, PI_SEL_C, (void *)&DefaultURLString,
      //  CMT_DEFAULT_URL, (void *)defaulturls},
      // {"follow_redirection", P_INT, PI_TEXT, &FollowRedirection,
      //  CMT_FOLLOW_REDIRECTION, NULL},
      // {"meta_refresh", P_CHARINT, PI_ONOFF, (void *)&MetaRefresh,
      //  CMT_META_REFRESH, NULL},
      // {"localhost_only", P_CHARINT, PI_ONOFF, (void *)&LocalhostOnly,
      //  CMT_LOCALHOST_ONLY, NULL},
      // {"dns_order", P_INT, PI_SEL_C, (void *)&DNS_order, CMT_DNS_ORDER,
      //  (void *)dnsorders},
  };

  sections = {
      {N_("Display Settings"), params1},
      {N_("Miscellaneous Settings"), params3},
      {N_("Directory Settings"), params5},
      {N_("External Program Settings"), params6},
      {N_("Network Settings"), params9},
      {N_("Proxy Settings"), params4},
      {N_("SSL Settings"), params7},
      {N_("Cookie Settings"), params8},
  };

  create_option_search_table();
}

void Option::create_option_search_table() {
  for (auto &section : sections) {
    for (auto &param : section.params) {
      RC_search_table.insert({param->name, param});
    }
  }
}

Option::~Option() {}

Option &Option::instance() {
  static Option s_instance;
  return s_instance;
}

std::shared_ptr<param_ptr> Option::search_param(const std::string &name) const {
  auto found = RC_search_table.find(name);
  if (found != RC_search_table.end()) {
    return found->second;
  }
  return nullptr;
}

std::string Option::get_param_option(const char *name) const {
  auto p = search_param(name);
  return p ? p->to_str() : "";
}

int Option::set_param(const std::string &name, const std::string &value) {
  if (value.empty()) {
    return 0;
  }

  auto p = search_param(name.c_str());
  if (p == NULL) {
    return 0;
  }

  return p->setParseValue(value);
}

#define W3MHELPERPANEL_CMDNAME "w3mhelperpanel"

static char optionpanel_src1[] =
    "<html><head><title>Option Setting Panel</title></head><body>\
<h1 align=center>Option Setting Panel<br>(w3m version %s)</b></h1>\
<form method=post action=\"file:///$LIB/" W3MHELPERPANEL_CMDNAME "\">\
<input type=hidden name=mode value=panel>\
<input type=hidden name=cookie value=\"%s\">\
<input type=submit value=\"%s\">\
</form><br>\
<form method=internal action=option>";

std::string Option::load_option_panel() {
  std::stringstream src;
  src << "<table><tr><td>";

  for (auto &section : sections) {
    src << "<h1>" << section.name << "</h1>";
    src << "<table width=100% cellpadding=0>";
    for (auto &p : section.params) {
      src << p->toOptionPanelHtml();
    }
    src << "<tr><td></td><td><p><input type=submit value=\"OK\"></td></tr>"
        << "</table><hr width=50%>";
  }
  src << "</table></form></body></html>";
  return src.str();
}
