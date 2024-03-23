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
#include "linein.h"
#include "cookie.h"
#include "http_request.h"
#include "myctype.h"
#include "proc.h"
#include "keyvalue.h"
#include "local_cgi.h"
#include "mailcap.h"
#include "Str.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#ifndef RC_DIR
#define RC_DIR "~/.w3m"
#endif

#define HAVE_FACCESSAT 1
#define W3MCONFIG "w3mconfig"
#define CONFIG_FILE "config"

#define set_no_proxy(domains) (NO_proxy_domains = make_domain_list(domains))

std::string rc_dir;
bool ArgvIsURL = true;

#define _(Text) Text
#define N_(Text) Text

#define DEFAULT_COLS 80
#define SITECONF_FILE RC_DIR "/siteconf"

const char *w3m_reqlog = {};
int displayImage = false;

const char *siteconf_file = SITECONF_FILE;
const char *ftppasswd = nullptr;
int ftppass_hostnamegen = true;
int WrapDefault = false;

std::string BookmarkFile;

struct auth_cookie *Auth_cookie = nullptr;
std::string config_file;
int is_redisplay = false;
int clear_buffer = true;
int set_pixel_per_char = false;

/* FIXME: gettextize here */

#define CMT_HELPER N_("External Viewer Setup")
#define CMT_PIXEL_PER_LINE N_("Number of pixels per line (4.0...64.0)")
#define CMT_PAGERLINE N_("Number of remembered lines when used as a pager")
#define CMT_FRAME N_("Render frames automatically")
#define CMT_ARGV_IS_URL N_("Treat argument without scheme as URL")
#define CMT_GRAPHIC_CHAR N_("Character type for border of table and menu")
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
#define CMT_TMP N_("Directory for temporary files")
#define CMT_CONFIRM_QQ N_("Confirm when quitting with q")
#define CMT_VI_PREC_NUM N_("Enable vi-like numeric prefix")
#define CMT_FOLD_LINE N_("Fold lines of plain text file")
#define CMT_SHOW_NUM N_("Show line numbers")
#define CMT_URIMETHODMAP N_("List of urimethodmap files")
#define CMT_MAILER N_("Mailer")
#define CMT_PRE_FORM_FILE N_("File for setting form on loading")
#define CMT_SITECONF_FILE N_("File for preferences for each site")
#define CMT_FTPPASS N_("Password for anonymous FTP (your mail address)")
#define CMT_FTPPASS_HOSTNAMEGEN N_("Generate domain part of password for FTP")
#define CMT_WRAP N_("Wrap search")
#define CMT_AUTO_UNCOMPRESS                                                    \
  N_("Uncompress compressed data automatically when downloading")
#define CMT_BGEXTVIEW N_("Run external viewer in the background")
#define CMT_RETRY_HTTP N_("Prepend http:// to URL automatically")
#define CMT_DECODE_CTE N_("Decode Content-Transfer-Encoding when saving")
#define CMT_CLEAR_BUF N_("Free memory of undisplayed buffers")
#define CMT_USE_LESSOPEN N_("Use LESSOPEN")
#define CMT_SSL_CERT_FILE N_("PEM encoded certificate file of client")
#define CMT_SSL_KEY_FILE N_("PEM encoded private key file of client")
#define CMT_SSL_CA_PATH                                                        \
  N_("Path to directory for PEM encoded certificates of CAs")
#define CMT_SSL_CA_FILE N_("File consisting of PEM encoded certificates of CAs")
#ifdef SSL_CTX_set_min_proto_version
#endif

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
  loadPasswd();

  if (AcceptLang.empty()) {
    /* TRANSLATORS:
     * AcceptLang default: this is used in Accept-Language: HTTP request
     * header. For example, ja.po should translate it as
     * "ja;q=1.0, en;q=0.5" like that.
     */
    AcceptLang = _("en;q=1.0");
  }
  if (AcceptEncoding.empty())
    AcceptEncoding = Strnew_charp(acceptableEncoding())->ptr;
  if (AcceptMedia.empty())
    AcceptMedia = acceptableMimeTypes();
  App::instance().initKeymap(false);
}

void init_rc(void) {
  int i;
  FILE *f;
  const char *p;

  if (rc_dir.size())
    goto open_rc;

  p = getenv("W3M_DIR");
  rc_dir = p ? p : "";
  if (rc_dir.empty())
    rc_dir = RC_DIR;
  if (rc_dir.empty())
    goto rc_dir_err;
  rc_dir = expandPath(rc_dir);

  i = rc_dir.size();
  if (i > 1 && rc_dir[i - 1] == '/') {
    rc_dir.pop_back();
  }

  if (do_recursive_mkdir(rc_dir.c_str()) == -1)
    goto rc_dir_err;

  no_rc_dir = false;

  if (config_file.empty()) {
    config_file = rcFile(CONFIG_FILE);
  }

  Option::instance().create_option_search_table();

open_rc:
  /* open config file */
  if ((f = fopen(etcFile(W3MCONFIG).c_str(), "rt")) != NULL) {
    interpret_rc(f);
    fclose(f);
  }
  if ((f = fopen(confFile(CONFIG_FILE).c_str(), "rt")) != NULL) {
    interpret_rc(f);
    fclose(f);
  }
  if (config_file.size() && (f = fopen(config_file.c_str(), "rt")) != NULL) {
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
  if (config_file.empty()) {
    App::instance().disp_message("There's no config file... config not saved");
  } else {
    f = fopen(config_file.c_str(), "wt");
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

std::string rcFile(std::string_view base) {
  if (base.size() &&
      (base[0] == '/' ||
       (base[0] == '.' &&
        (base[1] == '/' || (base[1] == '.' && base[2] == '/'))) ||
       (base[0] == '~' && base[1] == '/'))) {
    /* /file, ./file, ../file, ~/file */
    return expandPath(base);
  }

  std::stringstream ss;
  ss << rc_dir << "/" << base;
  return expandPath(ss.str());
}

std::string etcFile(const char *base) {
  return expandPath(Strnew_m_charp(w3m_etc_dir(), "/", base, NULL)->ptr);
}

std::string confFile(const char *base) {
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

const char *w3m_lib_dir(void) { return w3m_dir("W3M_LIB_DIR", CGIBIN_DIR); }
const char *w3m_etc_dir(void) { return w3m_dir("W3M_ETC_DIR", ETC_DIR); }
const char *w3m_conf_dir(void) { return w3m_dir("W3M_CONF_DIR", CONF_DIR); }
const char *w3m_help_dir(void) { return w3m_dir("W3M_HELP_DIR", HELP_DIR); }
