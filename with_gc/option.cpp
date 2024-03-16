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
#include "dict.h"
#include "loaddirectory.h"
#include "entity.h"
#include "html/html_feed_env.h"
#include "search.h"
#include "history.h"
#include "linein.h"
#include "file_util.h"
#include "local_cgi.h"
#include "ioutil.h"
#include "url_stream.h"
#include "mimetypes.h"
#include "mailcap.h"
#include "etc.h"
#include "ssl_util.h"
#include "cookie.h"

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
  //
  // OPTION_DISPLAY
  //
  push_param_int(OPTION_DISPLAY, "tabstop", &Tabstop, "Tab width in characters",
                 true);
  push_param_int(OPTION_DISPLAY, "indent_incr", &IndentIncr,
                 "Indent for HTML rendering", true);
  push_param(OPTION_DISPLAY,
             std::make_shared<param_pixels>(
                 "pixel_per_char", &pixel_per_char,
                 "Number of pixels per character (4.0...32.0)"));
  push_param_bool(OPTION_DISPLAY, "open_tab_blank", &open_tab_blank,
                  "Open link on new tab if target is _blank or _new");
  push_param_bool(OPTION_DISPLAY, "open_tab_dl_list", &open_tab_dl_list,
                  "Open download list panel on new tab");
  push_param_bool(OPTION_DISPLAY, "display_link", &displayLink,
                  "Display link URL automatically");
  push_param_bool(OPTION_DISPLAY, "display_link_number", &displayLinkNumber,
                  "Display link numbers");
  push_param_bool(OPTION_DISPLAY, "decode_url", &DecodeURL,
                  "Display decoded URL");
  push_param_bool(OPTION_DISPLAY, "display_lineinfo", &displayLineInfo,
                  "Display current line number");
  push_param_bool(OPTION_DISPLAY, "ext_dirlist", &UseExternalDirBuffer,
                  "Use external program for directory listing");
  push_param_string(OPTION_DISPLAY, "dirlist_cmd", &DirBufferCommand,
                    "URL of directory listing command");
  push_param_bool(OPTION_DISPLAY, "use_dictcommand", &UseDictCommand,
                  "Enable dictionary lookup through CGI");
  push_param_string(OPTION_DISPLAY, "dictcommand", &DictCommand,
                    "URL of dictionary lookup command");
  push_param_bool(OPTION_DISPLAY, "multicol", &multicolList,
                  "Display file names in multi-column format");
  push_param_bool(OPTION_DISPLAY, "alt_entity", &UseAltEntity,
                  "Use ASCII equivalents to display entities");
  push_param_bool(OPTION_DISPLAY, "display_borders", &DisplayBorders,
                  "Display table borders, ignore value of BORDER");
  push_param_bool(OPTION_DISPLAY, "disable_center", &DisableCenter,
                  "Disable center alignment");
  push_param_bool(OPTION_DISPLAY, "fold_textarea", &FoldTextarea,
                  "Fold lines in TEXTAREA");
  push_param_int_select(OPTION_DISPLAY, "display_ins_del", &displayInsDel,
                        "Display INS, DEL, S and STRIKE element",
                        displayinsdel);
  push_param_bool(OPTION_DISPLAY, "ignore_null_img_alt", &ignore_null_img_alt,
                  "Display link name for images lacking ALT");
  push_param_bool(OPTION_DISPLAY, "view_unseenobject", &view_unseenobject,
                  "Display unseen objects (e.g. bgimage tag)");
  push_param_bool(
      OPTION_DISPLAY, "pseudo_inlines", &pseudoInlines,
      "Display pseudo-ALTs for inline images with no ALT or TITLE string");
  push_param_bool(OPTION_DISPLAY, "show_srch_str", &show_srch_str,
                  "Show search string");

  //
  // OPTION_MICC
  //
  push_param_bool(OPTION_MICC, "use_history", &UseHistory, "Use URL history");
  push_param_int(OPTION_MICC, "history", &URLHistSize,
                 "Number of remembered URL", false);
  push_param_bool(OPTION_MICC, "save_hist", &SaveURLHist, "Save URL history");
  push_param_bool(OPTION_MICC, "close_tab_back", &close_tab_back,
                  "Close tab if buffer is last when back");
  push_param_bool(OPTION_MICC, "emacs_like_lineedit", &emacs_like_lineedit,
                  "Enable Emacs-style line editing");
  push_param_bool(OPTION_MICC, "space_autocomplete", &space_autocomplete,
                  "Space key triggers file completion while editing URLs");
  push_param_bool(OPTION_MICC, "mark_all_pages", &MarkAllPages,
                  "Treat URL-like strings as links in all pages");
  push_param_bool(OPTION_MICC, "ignorecase_search", &IgnoreCase,
                  "Search case-insensitively");
  push_param_bool(OPTION_MICC, "preserve_timestamp", &PreserveTimestamp,
                  "Preserve timestamp when saving");
  push_param_string(OPTION_MICC, "keymap_file", &keymap_file, "keymap file");

  //
  // OPTION_PROXY
  //
  push_param_bool(OPTION_PROXY, "no_cache", &NoCache, "Disable cache");

  //
  // OPTION_DIRECTORY
  //
  push_param_string(OPTION_DIRECTORY, "document_root", &document_root,
                    "Directory corresponding to / (document root)");
  push_param_string(OPTION_DIRECTORY, "personal_document_root",
                    &ioutil::personal_document_root,
                    "Directory corresponding to /~user");
  push_param_string(OPTION_DIRECTORY, "cgi_bin", &cgi_bin,
                    "Directory corresponding to /cgi-bin");
  push_param_string(OPTION_DIRECTORY, "index_file", &index_file,
                    "Index file for directories");

  //
  // OPTION_EXTERNAL_PROGRAM
  //
  push_param_string(OPTION_EXTERNAL_PROGRAM, "mime_types", &mimetypes_files,
                    "List of mime.types files");
  push_param_string(OPTION_EXTERNAL_PROGRAM, "mailcap", &mailcap_files,
                    "List of mailcap files");
  push_param_string(OPTION_EXTERNAL_PROGRAM, "editor", &ioutil::editor,
                    "Editor");
  push_param_string(OPTION_EXTERNAL_PROGRAM, "extbrowser", &ExtBrowser,
                    "External browser");
  push_param_string(OPTION_EXTERNAL_PROGRAM, "extbrowser2", &ExtBrowser2,
                    "2nd external browser");
  push_param_string(OPTION_EXTERNAL_PROGRAM, "extbrowser3", &ExtBrowser3,
                    "3rd external browser");
  push_param_string(OPTION_EXTERNAL_PROGRAM, "extbrowser4", &ExtBrowser4,
                    "4th external browser");
  push_param_string(OPTION_EXTERNAL_PROGRAM, "extbrowser5", &ExtBrowser5,
                    "5th external browser");
  push_param_string(OPTION_EXTERNAL_PROGRAM, "extbrowser6", &ExtBrowser6,
                    "6th external browser");
  push_param_string(OPTION_EXTERNAL_PROGRAM, "extbrowser7", &ExtBrowser7,
                    "7th external browser");
  push_param_string(OPTION_EXTERNAL_PROGRAM, "extbrowser8", &ExtBrowser8,
                    "8th external browser");
  push_param_string(OPTION_EXTERNAL_PROGRAM, "extbrowser9", &ExtBrowser9,
                    "9th external browser");

  //
  // OPTION_SSL
  //
  push_param_string(
      OPTION_SSL, "ssl_forbid_method", &ssl_forbid_method,
      "List of forbidden SSL methods (2: SSLv2, 3: SSLv3, t: TLSv1.0, 5: "
      "TLSv1.1, 6: TLSv1.2, 7: TLSv1.3)");
  push_param_string(
      OPTION_SSL, "ssl_min_version", &ssl_min_version,
      "Minimum SSL version (all, TLSv1.0, TLSv1.1, TLSv1.2, or TLSv1.3)");
  push_param_string(
      OPTION_SSL, "ssl_cipher", &ssl_cipher,
      "SSL ciphers for TLSv1.2 and below (e.g. DEFAULT:@SECLEVEL=2)");
  push_param_bool(OPTION_SSL, "ssl_verify_server", &ssl_verify_server,
                  "Perform SSL server verification");
  // ("ssl_cert_file", P_SSLPATH, PI_TEXT, (void *)&ssl_cert_file,
  // CMT_SSL_CERT_FILE);,
  // ("ssl_key_file", P_SSLPATH, PI_TEXT, (void *)&ssl_key_file,
  // CMT_SSL_KEY_FILE);,
  // ("ssl_ca_path", P_SSLPATH, PI_TEXT, (void *)&ssl_ca_path,
  // CMT_SSL_CA_PATH);,
  // ("ssl_ca_file", P_SSLPATH, PI_TEXT, (void *)&ssl_ca_file,
  // CMT_SSL_CA_FILE);,
  push_param_bool(OPTION_SSL, "ssl_ca_default", &ssl_ca_default,
                  "Use default locations for PEM encoded certificates of CAs");

  //
  // OPTION_COOKIE
  //
  push_param_bool(OPTION_COOKIE, "use_cookie", &use_cookie,
                  "Enable cookie processing");
  push_param_bool(OPTION_COOKIE, "show_cookie", &show_cookie,
                  "Print a message when receiving a cookie");
  push_param_bool(OPTION_COOKIE, "accept_cookie", &accept_cookie,
                  "Accept cookies");
  push_param_int_select(OPTION_COOKIE, "accept_bad_cookie", &accept_bad_cookie,
                        "Action to be taken on invalid cookie", badcookiestr);
  push_param_string(OPTION_COOKIE, "cookie_reject_domains",
                    &cookie_reject_domains, "Domains to reject cookies from");
  push_param_string(OPTION_COOKIE, "cookie_accept_domains",
                    &cookie_accept_domains, "Domains to accept cookies from");
  push_param_string(OPTION_COOKIE, "cookie_avoid_wrong_number_of_dots",
                    &cookie_avoid_wrong_number_of_dots,
                    "Domains to avoid [wrong number of dots]");

  // std::list<std::shared_ptr<param_ptr>> params9 = {
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
  // };
}

void Option::create_option_search_table() {
  for (auto &section : sections) {
    for (auto &param : section.params) {
      RC_search_table.insert({param->name, param});
    }
  }
}

void Option::push_param(const std::string &section,
                        const std::shared_ptr<param_ptr> &param) {
  auto found = std::find_if(
      sections.begin(), sections.end(),
      [section](const param_section &s) { return s.name == section; });
  if (found == sections.end()) {
    assert(false);
    return;
  }
  found->params.push_back(param);
  RC_search_table.insert({param->name, param});
}

void Option::push_param_int(const std::string &section, const std::string &name,
                            int *p, const std::string &comment, bool notZero) {
  push_param(section, std::make_shared<param_int>(name, p, comment, notZero));
}

void Option::push_param_int_select(const std::string &section,
                                   const std::string &name, int *p,
                                   const std::string &comment,
                                   struct sel_c *select) {
  push_param(section,
             std::make_shared<param_int_select>(name, p, comment, select));
}
void Option::push_param_bool(const std::string &section,
                             const std::string &name, bool *p,
                             const std::string &comment) {
  push_param(section, std::make_shared<param_bool>(name, p, comment));
}
void Option::push_param_string(const std::string &section,
                               const std::string &name, std::string *p,
                               const std::string &comment) {
  push_param(section, std::make_shared<param_string>(name, p, comment));
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
