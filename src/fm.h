/*
 * w3m: WWW wo Miru utility
 *
 * by A.ITO  Feb. 1995
 *
 * You can use,copy,modify and distribute this program without any permission.
 */
#pragma once

#define USE_IMAGE 1
#define KEYMAP_FILE "keymap"
#define MENU_FILE "menu"
#define MOUSE_FILE "mouse"
#define HISTORY_FILE "history"
#define PRE_FORM_FILE RC_DIR "/pre_form"
#define USER_MAILCAP RC_DIR "/mailcap"
#define SYS_MAILCAP CONF_DIR "/mailcap"
#define USER_URIMETHODMAP RC_DIR "/urimethodmap"
#define SYS_URIMETHODMAP CONF_DIR "/urimethodmap"
#define DEF_EDITOR "/usr/bin/vi"
#define DEF_MAILER "/usr/bin/mail"
#define DEF_EXT_BROWSER "/usr/bin/firefox"
#define DEF_IMAGE_VIEWER "display"
#define DEF_AUDIO_PLAYER "showaudio"

#ifdef MAINPROGRAM
#define global
#define init(x) = (x)
#else /* not MAINPROGRAM */
#define global extern
#define init(x)
#endif /* not MAINPROGRAM */

#define DEFUN(funcname, macroname, docstring) void funcname(struct Current current)

global char TargetSelf init(false);
global char PermitSaveToPipe init(false);
global char DecodeCTE init(false);
global char PreserveTimestamp init(true);
global char ArgvIsURL init(true);
global char *personal_document_root init(nullptr);
global char *cgi_bin init(nullptr);
#if defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE)
global char *MyProgramName init("w3m");
#endif /* defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE) */
global int open_tab_blank init(false);
global int open_tab_dl_list init(false);
global int w3m_debug;
global int confirm_on_quit init(true);
global int emacs_like_lineedit init(false);
global int space_autocomplete init(false);
global char *displayTitleTerm init(nullptr);
global int displayImage init(false); /* XXX: emacs-w3m use display_image=off */
global char *Editor init(DEF_EDITOR);
global char *Mailer init(DEF_MAILER);
global char *ExtBrowser init(DEF_EXT_BROWSER);
global char *ExtBrowser2 init(nullptr);
global char *ExtBrowser3 init(nullptr);
global char *ExtBrowser4 init(nullptr);
global char *ExtBrowser5 init(nullptr);
global char *ExtBrowser6 init(nullptr);
global char *ExtBrowser7 init(nullptr);
global char *ExtBrowser8 init(nullptr);
global char *ExtBrowser9 init(nullptr);
global int BackgroundExtViewer init(true);
global char *pre_form_file init(PRE_FORM_FILE);
global char *ftppasswd init(nullptr);
global int ftppass_hostnamegen init(true);
global int WrapDefault init(false);
global const char *BookmarkFile init(nullptr);
global int UseDictCommand init(true);
global int FoldTextarea init(false);
global struct auth_cookie *Auth_cookie init(nullptr);
global char *mailcap_files init(USER_MAILCAP ", " SYS_MAILCAP);
global int UseHistory init(true);
global int URLHistSize init(100);
global int SaveURLHist init(true);
global int multicolList init(false);
global char UseAltEntity init(false);
global const char *config_file init(nullptr);

global int use_lessopen init(false);
global char *keymap_file init(KEYMAP_FILE);
