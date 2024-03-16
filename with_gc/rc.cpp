/*
 * Initialization file etc.
 */
#include "rc.h"
#include "option.h"
#include "search.h"
#include "quote.h"
#include "app.h"
#include "loaddirectory.h"
#include "auth_pass.h"
#include "http_auth.h"
#include "etc.h"
#include "alloc.h"
#include "mimetypes.h"
#include "compression.h"
#include "html/readbuffer.h"
#include "linein.h"
#include "cookie.h"
#include "http_request.h"
#include "myctype.h"
#include "proc.h"
#include "keyvalue.h"
#include "local_cgi.h"
#include "regex.h"
#include "mailcap.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#ifndef RC_DIR
#define RC_DIR "~/.w3m"
#endif

int DNS_order = DNS_ORDER_UNSPEC;

#define HAVE_FACCESSAT 1
#define W3MCONFIG "w3mconfig"
#define CONFIG_FILE "config"

#define set_no_proxy(domains) (NO_proxy_domains = make_domain_list(domains))

const char *rc_dir = nullptr;
bool ArgvIsURL = true;
bool LocalhostOnly = false;
bool retryAsHttp = true;
bool AutoUncompress = false;
bool PreserveTimestamp = true;

#define _(Text) Text
#define N_(Text) Text

#define DEFAULT_COLS 80
#define SITECONF_FILE RC_DIR "/siteconf"

const char *w3m_reqlog = {};
int label_topline = false;
int displayImage = false;

int BackgroundExtViewer = true;

const char *siteconf_file = SITECONF_FILE;
const char *ftppasswd = nullptr;
int ftppass_hostnamegen = true;
int WrapDefault = false;

const char *BookmarkFile = nullptr;
const char *DirBufferCommand = "file:///$LIB/dirlist" CGI_EXTENSION;

struct auth_cookie *Auth_cookie = nullptr;
struct Cookie *First_cookie = nullptr;
int no_rc_dir = false;
const char *config_file = nullptr;
int is_redisplay = false;
int clear_buffer = true;
int set_pixel_per_char = false;
const char *keymap_file = KEYMAP_FILE;
char *document_root = nullptr;

/* FIXME: gettextize here */

#define CMT_HELPER N_("External Viewer Setup")
#define CMT_PIXEL_PER_LINE N_("Number of pixels per line (4.0...64.0)")
#define CMT_PAGERLINE N_("Number of remembered lines when used as a pager")
#define CMT_HISTORY N_("Use URL history")
#define CMT_HISTSIZE N_("Number of remembered URL")
#define CMT_SAVEHIST N_("Save URL history")
#define CMT_FRAME N_("Render frames automatically")
#define CMT_ARGV_IS_URL N_("Treat argument without scheme as URL")
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
#define CMT_HINTENSITY_COLOR N_("Use high-intensity colors")
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
#define CMT_TMP N_("Directory for temporary files")
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

static void interpret_rc(FILE *f) {
  Str *line;
  Str *tmp;
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
    Option::instance().set_param(tmp->ptr, p);
  }
}

static void parse_cookie(void) {
  make_domain_list(Cookie_reject_domains, cookie_reject_domains);
  make_domain_list(Cookie_accept_domains, cookie_accept_domains);
  make_domain_list(Cookie_avoid_wrong_number_of_dots_domains,
                   cookie_avoid_wrong_number_of_dots);
}

void sync_with_option(void) {
  WrapSearch = WrapDefault;
  parse_cookie();
  initMailcap();
  initMimeTypes();
  displayImage = false; /* XXX */
  loadPasswd();

  if (AcceptLang == NULL || *AcceptLang == '\0') {
    /* TRANSLATORS:
     * AcceptLang default: this is used in Accept-Language: HTTP request
     * header. For example, ja.po should translate it as
     * "ja;q=1.0, en;q=0.5" like that.
     */
    AcceptLang = _("en;q=1.0");
  }
  if (AcceptEncoding == NULL || *AcceptEncoding == '\0')
    AcceptEncoding = Strnew_charp(acceptableEncoding())->ptr;
  if (AcceptMedia == NULL || *AcceptMedia == '\0')
    AcceptMedia = acceptableMimeTypes();
  App::instance().initKeymap(false);
}

void init_rc(void) {
  int i;
  FILE *f;

  if (rc_dir != NULL)
    goto open_rc;

  rc_dir = allocStr(getenv("W3M_DIR"), -1);
  if (rc_dir == NULL || *rc_dir == '\0')
    rc_dir = allocStr(RC_DIR, -1);
  if (rc_dir == NULL || *rc_dir == '\0')
    goto rc_dir_err;
  rc_dir = expandPath(rc_dir);

  i = strlen(rc_dir);
  if (i > 1 && rc_dir[i - 1] == '/')
    ((char *)rc_dir)[i - 1] = '\0';

  if (do_recursive_mkdir(rc_dir) == -1)
    goto rc_dir_err;

  no_rc_dir = false;

  if (config_file == NULL)
    config_file = rcFile(CONFIG_FILE);

  Option::instance().create_option_search_table();

open_rc:
  /* open config file */
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
  return;

rc_dir_err:
  no_rc_dir = true;
  Option::instance().create_option_search_table();
  goto open_rc;
}

void panel_set_option(keyvalue *arg) {
  FILE *f = NULL;
  if (config_file == NULL) {
    App::instance().disp_message("There's no config file... config not saved");
  } else {
    f = fopen(config_file, "wt");
    if (f == NULL) {
    }
  }

  Str *s = nullptr;
  while (arg) {
    /*  InnerCharset -> SystemCharset */
    if (arg->value.size()) {
      auto p = arg->value;
      if (Option::instance().set_param(arg->arg, p)) {
        auto tmp = Sprintf("%s %s\n", arg->arg.c_str(), p.c_str());
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
  backBf({});
}

const char *rcFile(const char *base) {
  if (base && (base[0] == '/' ||
               (base[0] == '.' &&
                (base[1] == '/' || (base[1] == '.' && base[2] == '/'))) ||
               (base[0] == '~' && base[1] == '/')))
    /* /file, ./file, ../file, ~/file */
    return expandPath((char *)base);
  return expandPath(Strnew_m_charp(rc_dir, "/", base, NULL)->ptr);
}

const char *auxbinFile(const char *base) {
  return expandPath(Strnew_m_charp(w3m_auxbin_dir(), "/", base, NULL)->ptr);
}

const char *etcFile(const char *base) {
  return expandPath(Strnew_m_charp(w3m_etc_dir(), "/", base, NULL)->ptr);
}

const char *confFile(const char *base) {
  return expandPath(Strnew_m_charp(w3m_conf_dir(), "/", base, NULL)->ptr);
}

static const char *w3m_dir(const char *name, const char *dft) {
#ifdef USE_PATH_ENVVAR
  char *value = getenv(name);
  return value ? value : dft;
#else
  return dft;
#endif
}
const char *w3m_auxbin_dir(void) {
  return w3m_dir("W3M_AUXBIN_DIR", AUXBIN_DIR);
}
const char *w3m_lib_dir(void) { return w3m_dir("W3M_LIB_DIR", CGIBIN_DIR); }
const char *w3m_etc_dir(void) { return w3m_dir("W3M_ETC_DIR", ETC_DIR); }
const char *w3m_conf_dir(void) { return w3m_dir("W3M_CONF_DIR", CONF_DIR); }
const char *w3m_help_dir(void) { return w3m_dir("W3M_HELP_DIR", HELP_DIR); }
