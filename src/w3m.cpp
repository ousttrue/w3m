#include "w3m.h"
#include "url_decode.h"
#include "app.h"
#include "etc.h"
#include "quote.h"
#include "http_request.h"
#include "cookie.h"
#include "proc.h"
#include "myctype.h"

#define DEFAULT_COLS 80
#define SITECONF_FILE RC_DIR "/siteconf"
#define DEF_EXT_BROWSER "/usr/bin/firefox"

const char *w3m_reqlog = {};
bool IsForkChild = 0;
int label_topline = false;
int show_srch_str = true;
int displayImage = false;
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

int DefaultURLString = DEFAULT_URL_CURRENT;
struct auth_cookie *Auth_cookie = nullptr;
struct Cookie *First_cookie = nullptr;
char UseAltEntity = false;
int no_rc_dir = false;
const char *config_file = nullptr;
int is_redisplay = false;
int clear_buffer = true;
int set_pixel_per_char = false;
const char *keymap_file = KEYMAP_FILE;
char *document_root = nullptr;
