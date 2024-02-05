#include "w3m.h"
#include "history.h"
#include "terms.h"
#include "display.h"
#include "tabbuffer.h"
#include "httprequest.h"
#include "cookie.h"
#include "buffer.h"
#include "textlist.h"
#include "proto.h"
#include "myctype.h"

#define DEFAULT_COLS 80
#define SITECONF_FILE RC_DIR "/siteconf"
#define DEF_EDITOR "/usr/bin/vi"
#define DEF_MAILER "/usr/bin/mail"
#define DEF_EXT_BROWSER "/usr/bin/firefox"
#define PRE_FORM_FILE RC_DIR "/pre_form"

const char *w3m_reqlog = {};
bool fmInitialized = 0;
bool IsForkChild = 0;
bool confirm_on_quit = true;
int vi_prec_num = false;
int label_topline = false;
int nextpage_topline = false;
int displayLineInfo = false;
int show_srch_str = true;
int displayImage = false;
const char *Editor = DEF_EDITOR;
const char *Mailer = DEF_MAILER;
int MailtoOptions = MAILTO_OPTIONS_IGNORE;
const char *ExtBrowser = DEF_EXT_BROWSER;
char *ExtBrowser2 = nullptr;
char *ExtBrowser3 = nullptr;
char *ExtBrowser4 = nullptr;
char *ExtBrowser5 = nullptr;
char *ExtBrowser6 = nullptr;
char *ExtBrowser7 = nullptr;
char *ExtBrowser8 = nullptr;
char *ExtBrowser9 = nullptr;
int BackgroundExtViewer = true;
const char *pre_form_file = PRE_FORM_FILE;
const char *siteconf_file = SITECONF_FILE;
const char *ftppasswd = nullptr;
int ftppass_hostnamegen = true;
int WrapDefault = false;
int IgnoreCase = true;
int WrapSearch = false;
int squeezeBlankLine = false;
const char *BookmarkFile = nullptr;
int UseExternalDirBuffer = true;
const char *DirBufferCommand = "file:///$LIB/dirlist" CGI_EXTENSION;
int UseDictCommand = true;
const char *DictCommand = "file:///$LIB/w3mdict" CGI_EXTENSION;
int FoldTextarea = false;
int DefaultURLString = DEFAULT_URL_CURRENT;
struct auth_cookie *Auth_cookie = nullptr;
struct cookie *First_cookie = nullptr;
char UseAltEntity = false;
int no_rc_dir = false;
char *param_tmp_dir = nullptr;
const char *mkd_tmp_dir = nullptr;
const char *config_file = nullptr;
int is_redisplay = false;
int clear_buffer = true;
int set_pixel_per_char = false;
const char *keymap_file = KEYMAP_FILE;
char *HostName = nullptr;
char *CurrentDir = nullptr;
int CurrentPid = {};
TextList *fileToDelete = nullptr;
char *document_root = nullptr;
bool do_download = false;

void _quitfm(bool confirm) {
  const char *ans = "y";
  // if (checkDownloadList()) {
  //   ans = inputChar("Download process retains. "
  //                   "Do you want to exit w3m? (y/n)");
  // } else if (confirm) {
  //   ans = inputChar("Do you want to exit w3m? (y/n)");
  // }

  if (!(ans && TOLOWER(*ans) == 'y')) {
    // cancel
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }

  // exit
  term_title(""); /* XXX */
  fmTerm();
  save_cookies();
  if (UseHistory && SaveURLHist)
    saveHistory(URLHist, URLHistSize);
  w3m_exit(0);
}
