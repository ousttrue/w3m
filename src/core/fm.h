#pragma once

#include "config.h"
#include "textlist.h"
#include <wc.h>
#include <Str.h>

#ifdef MAINPROGRAM
#define global
#define init(x) = (x)
#else /* not MAINPROGRAM */
#define global extern
#define init(x)
#endif /* not MAINPROGRAM */

#define DEFUN(funcname, macroname, docstring) void funcname(void)

#ifdef FALSE
#undef FALSE
#endif
#ifdef TRUE
#undef TRUE
#endif
#define FALSE 0
#define TRUE 1

#ifdef MAINPROGRAM
int REV_LB[MAX_LB] = {
    LB_N_INFO,
    LB_INFO,
    LB_N_SOURCE,
};
#else /* not MAINPROGRAM */
extern int REV_LB[];
#endif /* not MAINPROGRAM */

/*
 * Macros.
 */

/* is this '<' really means the beginning of a tag? */
#define REALLY_THE_BEGINNING_OF_A_TAG(p) \
    (IS_ALPHA(p[1]) || p[1] == '/' || p[1] == '!' || p[1] == '?' || p[1] == '\0' || p[1] == '_')

/* modes for align() */

#define VALIGN_MIDDLE 0
#define VALIGN_TOP 1
#define VALIGN_BOTTOM 2

/*
 * Globals.
 */

global int ShowEffect init(TRUE);
#define PAGER_MAX_LINE 10000 /* Maximum line kept as pager */
global int PagerMax init(PAGER_MAX_LINE);

// global char SearchHeader init(FALSE);
global char* DefaultType init(NULL);
global char TargetSelf init(FALSE);
global char PermitSaveToPipe init(FALSE);
global char DecodeCTE init(FALSE);
global char AutoUncompress init(FALSE);
global char PreserveTimestamp init(TRUE);

global int CurrentKey;
global char* CurrentKeyData;
global char* CurrentCmdData;

global int confirm_on_quit init(TRUE);
global int use_mark init(FALSE);
global int label_topline init(FALSE);
global int nextpage_topline init(FALSE);
global int displayLinkNumber init(FALSE);
global int show_srch_str init(TRUE);
global char* Imgdisplay init(IMGDISPLAY);
global int useExtImageViewer init(TRUE);
global int maxLoadImage init(4);
global int image_map_list init(TRUE);
global int pseudoInlines init(TRUE);
global char* Editor init(DEF_EDITOR);
#ifdef USE_W3MMAILER
global char* Mailer init(NULL);
#else
global char* Mailer init(DEF_MAILER);
#endif
#ifdef USE_W3MMAILER
#define MAILTO_OPTIONS_USE_W3MMAILER 0
#endif
#define MAILTO_OPTIONS_IGNORE 1
#define MAILTO_OPTIONS_USE_MAILTO_URL 2
global int MailtoOptions init(MAILTO_OPTIONS_IGNORE);
global char* ExtBrowser init(DEF_EXT_BROWSER);
global char* ExtBrowser2 init(NULL);
global char* ExtBrowser3 init(NULL);
global char* ExtBrowser4 init(NULL);
global char* ExtBrowser5 init(NULL);
global char* ExtBrowser6 init(NULL);
global char* ExtBrowser7 init(NULL);
global char* ExtBrowser8 init(NULL);
global char* ExtBrowser9 init(NULL);
global int BackgroundExtViewer init(TRUE);
global int disable_secret_security_check init(FALSE);
global char* passwd_file init(PASSWD_FILE);
global char* pre_form_file init(PRE_FORM_FILE);
global char* siteconf_file init(SITECONF_FILE);
global int WrapDefault init(FALSE);
global int IgnoreCase init(TRUE);
global int WrapSearch init(FALSE);
global int squeezeBlankLine init(FALSE);
global char* BookmarkFile init(NULL);
global int UseExternalDirBuffer init(TRUE);

#define CGI_EXTENSION ".cgi"
// #define CGI_EXTENSION ".cmd"
global char* DirBufferCommand init("file:///$LIB/dirlist" CGI_EXTENSION);
global int UseDictCommand init(TRUE);
global char* DictCommand init("file:///$LIB/w3mdict" CGI_EXTENSION);
global int ignore_null_img_alt init(TRUE);
global int FoldTextarea init(FALSE);
#define DEFAULT_URL_EMPTY 0
#define DEFAULT_URL_CURRENT 1
#define DEFAULT_URL_LINK 2
global int DefaultURLString init(DEFAULT_URL_CURRENT);
global int MarkAllPages init(FALSE);

global struct auth_cookie* Auth_cookie init(NULL);
global struct cookie* First_cookie init(NULL);

global char* mailcap_files init(USER_MAILCAP ", " SYS_MAILCAP);


global char ExtHalfdump init(FALSE);
global char FollowLocale init(TRUE);
global char SearchConv init(TRUE);
global char SimplePreserveSpace init(FALSE);

global char UseAltEntity init(FALSE);
global int no_rc_dir init(FALSE);
global char* param_tmp_dir init(NULL);
#ifdef HAVE_MKDTEMP
global char* mkd_tmp_dir init(NULL);
#endif
global char* config_file init(NULL);

global int default_use_cookie init(TRUE);
global char* cookie_reject_domains init(NULL);
global char* cookie_accept_domains init(NULL);
global char* cookie_avoid_wrong_number_of_dots init(NULL);
global TextList* Cookie_reject_domains;
global TextList* Cookie_accept_domains;
global TextList* Cookie_avoid_wrong_number_of_dots_domains;


global int is_redisplay init(FALSE);
global int clear_buffer init(TRUE);
global double image_scale init(100);

global char* keymap_file init(KEYMAP_FILE);

global int FollowRedirection init(10);

void w3m_exit(int i);

