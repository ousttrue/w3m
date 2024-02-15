#pragma once
#include "url.h"
#include <optional>
#include <string>
#include <list>

#define KEYMAP_FILE "keymap"
#define CGI_EXTENSION ".cgi"
#define BOOKMARK "bookmark.html"

#define MAILTO_OPTIONS_IGNORE 1
#define MAILTO_OPTIONS_USE_MAILTO_URL 2
#define DEFAULT_URL_EMPTY 0
#define DEFAULT_URL_CURRENT 1
#define DEFAULT_URL_LINK 2

extern const char *w3m_reqlog;
extern bool confirm_on_quit;
extern bool fmInitialized;
extern bool IsForkChild;
extern char *w3m_version;

extern std::list<std::string> fileToDelete;

extern char *document_root;

extern int label_topline;
extern int displayLineInfo;
extern int show_srch_str;
extern int displayImage;
extern int MailtoOptions;
extern const char *ExtBrowser;
extern char *ExtBrowser2;
extern char *ExtBrowser3;
extern char *ExtBrowser4;
extern char *ExtBrowser5;
extern char *ExtBrowser6;
extern char *ExtBrowser7;
extern char *ExtBrowser8;
extern char *ExtBrowser9;
extern int BackgroundExtViewer;
extern const char *pre_form_file;
extern const char *siteconf_file;
extern const char *ftppasswd;
extern int ftppass_hostnamegen;
extern int WrapDefault;
extern int IgnoreCase;
extern int WrapSearch;
extern int squeezeBlankLine;
extern const char *BookmarkFile;
extern int UseExternalDirBuffer;
extern const char *DirBufferCommand;
extern int UseDictCommand;
extern const char *DictCommand;
extern int FoldTextarea;
extern int DefaultURLString;
extern struct auth_cookie *Auth_cookie;
extern struct Cookie *First_cookie;
extern char UseAltEntity;
extern int no_rc_dir;
extern const char *mkd_tmp_dir;
extern const char *config_file;
extern int is_redisplay;
extern int clear_buffer;
extern int set_pixel_per_char;
extern const char *keymap_file;

void w3m_exit(int i);
void _quitfm(bool confirm);
