/* $Id: fm.h,v 1.149 2010/08/20 09:47:09 htrb Exp $ */
/*
 * w3m: WWW wo Miru utility
 *
 * by A.ITO  Feb. 1995
 *
 * You can use,copy,modify and distribute this program without any permission.
 */

#ifndef FM_H
#define FM_H

#define USE_IMAGE 1
#define INET6 1

#define KEYMAP_FILE "keymap"
#define MENU_FILE "menu"
#define MOUSE_FILE "mouse"
#define COOKIE_FILE "cookie"
#define HISTORY_FILE "history"
#define PRE_FORM_FILE RC_DIR "/pre_form"
#define SITECONF_FILE RC_DIR "/siteconf"
#define USER_MAILCAP RC_DIR "/mailcap"
#define SYS_MAILCAP CONF_DIR "/mailcap"
#define USER_MIMETYPES "~/.mime.types"
#define SYS_MIMETYPES ETC_DIR "/mime.types"
#define USER_URIMETHODMAP RC_DIR "/urimethodmap"
#define SYS_URIMETHODMAP CONF_DIR "/urimethodmap"
#define DEF_EDITOR "/usr/bin/vi"
#define DEF_MAILER "/usr/bin/mail"
#define DEF_EXT_BROWSER "/usr/bin/firefox"
#define DEF_IMAGE_VIEWER "display"
#define DEF_AUDIO_PLAYER "showaudio"

#include "config.h"
#include "func.h"
#include <stdio.h>

#ifdef MAINPROGRAM
#define global
#define init(x) = (x)
#else /* not MAINPROGRAM */
#define global extern
#define init(x)
#endif /* not MAINPROGRAM */

#define DEFUN(funcname, macroname, docstring) void funcname(void)

/*
 * Constants.
 */

/* mark URL, Message-ID */
#define CHK_URL 1
#define CHK_NMID 2

/* Completion status. */
#define CPL_OK 0
#define CPL_AMBIG 1
#define CPL_FAIL 2
#define CPL_MENU 3

#define CPL_NEVER 0x0
#define CPL_OFF 0x1
#define CPL_ON 0x2
#define CPL_ALWAYS 0x4
#define CPL_URL 0x8

#define IMG_FLAG_SKIP 1
#define IMG_FLAG_AUTO 2

#define IMG_FLAG_START 0
#define IMG_FLAG_STOP 1
#define IMG_FLAG_NEXT 2

#define IMG_FLAG_UNLOADED 0
#define IMG_FLAG_LOADED 1
#define IMG_FLAG_ERROR 2
#define IMG_FLAG_DONT_REMOVE 4

#define IS_EMPTY_PARSED_URL(pu) ((pu)->scheme == SCM_UNKNOWN && !(pu)->file)

/*
 * Macros.
 */
#define bpcmp(a, b)                                                            \
  (((a).line - (b).line) ? ((a).line - (b).line) : ((a).pos - (b).pos))

#define RELATIVE_WIDTH(w) (((w) >= 0) ? (int)((w) / pixel_per_char) : (w))
#define REAL_WIDTH(w, limit)                                                   \
  (((w) >= 0) ? (int)((w) / pixel_per_char) : -(w) * (limit) / 100)

#define EOL(l) (&(l)->ptr[(l)->length])
#define IS_EOL(p, l) ((p) == &(l)->ptr[(l)->length])

#define INLINE_IMG_NONE 0
#define INLINE_IMG_OSC5379 1
#define INLINE_IMG_SIXEL 2
#define INLINE_IMG_ITERM2 3
#define INLINE_IMG_KITTY 4

/*
 * Types.
 */

#define RB_STACK_SIZE 10

#define TAG_STACK_SIZE 10

#define FONT_STACK_SIZE 5

#define FONTSTAT_SIZE 7
#define FONTSTAT_MAX 127

#define in_bold fontstat[0]
#define in_under fontstat[1]
#define in_italic fontstat[2]
#define in_strike fontstat[3]
#define in_ins fontstat[4]
#define in_stand fontstat[5]

/* modes for align() */

#define ALIGN_CENTER 0
#define ALIGN_LEFT 1
#define ALIGN_RIGHT 2
#define ALIGN_MIDDLE 4
#define ALIGN_TOP 5
#define ALIGN_BOTTOM 6

#define VALIGN_MIDDLE 0
#define VALIGN_TOP 1
#define VALIGN_BOTTOM 2

/*
 * Globals.
 */

global int IndentIncr init(4);

global char SearchHeader init(false);
global const char *DefaultType init(NULL);
global char RenderFrame init(false);
global char TargetSelf init(false);
global char PermitSaveToPipe init(false);
global char DecodeCTE init(false);
global char AutoUncompress init(false);
global char PreserveTimestamp init(true);
global char ArgvIsURL init(true);
global char MetaRefresh init(false);
global char LocalhostOnly init(false);

extern unsigned char GlobalKeymap[];
extern unsigned char EscKeymap[];
extern unsigned char EscBKeymap[];
extern unsigned char EscDKeymap[];
extern FuncList w3mFuncList[];

global char *HTTP_proxy init(NULL);
global char *HTTPS_proxy init(NULL);
global char *FTP_proxy init(NULL);
global struct Url HTTP_proxy_parsed;
global struct Url HTTPS_proxy_parsed;
global struct Url FTP_proxy_parsed;
global char *NO_proxy init(NULL);
global int NOproxy_netaddr init(true);
#ifdef INET6
#define DNS_ORDER_UNSPEC 0
#define DNS_ORDER_INET_INET6 1
#define DNS_ORDER_INET6_INET 2
#define DNS_ORDER_INET_ONLY 4
#define DNS_ORDER_INET6_ONLY 6
global int DNS_order init(DNS_ORDER_UNSPEC);
extern int ai_family_order_table[7][3]; /* XXX */
#endif                                  /* INET6 */
global char NoCache init(false);
global char use_proxy init(true);
#define Do_not_use_proxy (!use_proxy)

global char *document_root init(NULL);
global char *personal_document_root init(NULL);
global char *cgi_bin init(NULL);
global char *index_file init(NULL);

#if defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE)
global char *MyProgramName init("w3m");
#endif /* defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE) */
/*
 * global Buffer *Currentbuf;
 * global Buffer *Firstbuf;
 */
global int open_tab_blank init(false);
global int open_tab_dl_list init(false);
global int close_tab_back init(false);
global int nTab;
global int TabCols init(10);
global int CurrentKey;
global const char *CurrentKeyData;
global const char *CurrentCmdData;
extern char *w3m_version;
extern int enable_inline_image;

global int w3m_debug;

global int override_content_type init(false);
global int override_user_agent init(false);

global int confirm_on_quit init(true);
global int emacs_like_lineedit init(false);
global int space_autocomplete init(false);
global int vi_prec_num init(false);
global int label_topline init(false);
global char *displayTitleTerm init(NULL);
global int displayLink init(false);
global int displayLinkNumber init(false);
global int displayLineInfo init(false);
global int DecodeURL init(false);
global int retryAsHttp init(true);
global int displayImage init(false); /* XXX: emacs-w3m use display_image=off */
global int pseudoInlines init(true);
global char *Editor init(DEF_EDITOR);
global char *Mailer init(DEF_MAILER);
#define MAILTO_OPTIONS_IGNORE 1
#define MAILTO_OPTIONS_USE_MAILTO_URL 2
global int MailtoOptions init(MAILTO_OPTIONS_IGNORE);
global char *ExtBrowser init(DEF_EXT_BROWSER);
global char *ExtBrowser2 init(NULL);
global char *ExtBrowser3 init(NULL);
global char *ExtBrowser4 init(NULL);
global char *ExtBrowser5 init(NULL);
global char *ExtBrowser6 init(NULL);
global char *ExtBrowser7 init(NULL);
global char *ExtBrowser8 init(NULL);
global char *ExtBrowser9 init(NULL);
global int BackgroundExtViewer init(true);
global char *pre_form_file init(PRE_FORM_FILE);
global char *siteconf_file init(SITECONF_FILE);
global char *ftppasswd init(NULL);
global int ftppass_hostnamegen init(true);
global int do_download init(false);
global char *UserAgent init(NULL);
global int NoSendReferer init(false);
global int CrossOriginReferer init(true);
global char *AcceptLang init(NULL);
global char *AcceptEncoding init(NULL);
global char *AcceptMedia init(NULL);
global int WrapDefault init(false);
global int squeezeBlankLine init(false);
global char *BookmarkFile init(NULL);
global int UseExternalDirBuffer init(true);
global char *DirBufferCommand init("file:///$LIB/dirlist" CGI_EXTENSION);
global int UseDictCommand init(true);
global char *DictCommand init("file:///$LIB/w3mdict" CGI_EXTENSION);
global int ignore_null_img_alt init(true);
#define DISPLAY_INS_DEL_SIMPLE 0
#define DISPLAY_INS_DEL_NORMAL 1
#define DISPLAY_INS_DEL_FONTIFY 2
global int displayInsDel init(DISPLAY_INS_DEL_NORMAL);
global int FoldTextarea init(false);
#define DEFAULT_URL_EMPTY 0
#define DEFAULT_URL_CURRENT 1
#define DEFAULT_URL_LINK 2
global int DefaultURLString init(DEFAULT_URL_CURRENT);
global int MarkAllPages init(false);

global struct auth_cookie *Auth_cookie init(NULL);
global struct cookie *First_cookie init(NULL);

global char *mailcap_files init(USER_MAILCAP ", " SYS_MAILCAP);
global char *mimetypes_files init(USER_MIMETYPES ", " SYS_MIMETYPES);

global int UseHistory init(true);
global int URLHistSize init(100);
global int SaveURLHist init(true);
global int multicolList init(false);

#define Str_conv_from_system(x) (x)
#define Str_conv_to_system(x) (x)
#define Str_conv_to_halfdump(x) (x)
#define conv_from_system(x) (x)
#define conv_to_system(x) (x)

#define wc_Str_conv(x, charset0, charset1) (x)
#define wc_Str_conv_strict(x, charset0, charset1) (x)
global char UseAltEntity init(false);
#define GRAPHIC_CHAR_ASCII 2
#define GRAPHIC_CHAR_DEC 1
#define GRAPHIC_CHAR_CHARSET 0
global char UseGraphicChar init(GRAPHIC_CHAR_CHARSET);
global char DisplayBorders init(false);
global char DisableCenter init(false);
extern char *graph_symbol[];
extern char *graph2_symbol[];
#define N_GRAPH_SYMBOL 32
#define N_SYMBOL (N_GRAPH_SYMBOL + 14)
global int no_rc_dir init(false);
global char *rc_dir init(NULL);
global char *config_file init(NULL);

global int view_unseenobject init(true);

global int ssl_verify_server init(true);
global char *ssl_cert_file init(NULL);
global char *ssl_key_file init(NULL);
global char *ssl_ca_path init(NULL);
global char *ssl_ca_file init("");
global int ssl_ca_default init(true);
global int ssl_path_modified init(false);
global char *ssl_forbid_method init("2, 3, t, 5");
global char *ssl_min_version init(NULL);
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
global char *ssl_cipher init("DEFAULT:!LOW:!RC4:!EXP");
#else
global char *ssl_cipher init(NULL);
#endif

global int is_redisplay init(false);
global int clear_buffer init(true);

global int use_lessopen init(false);

global char *keymap_file init(KEYMAP_FILE);

global int FollowRedirection init(10);

extern void deleteFiles(void);
void w3m_exit(int i);

#endif /* not FM_H */
