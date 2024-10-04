/*
 * w3m: WWW wo Miru utility
 *
 * by A.ITO  Feb. 1995
 *
 * You can use,copy,modify and distribute this program without any permission.
 */
#pragma once

#define USE_IMAGE 1
#define INET6 1

#define KEYMAP_FILE "keymap"
#define MENU_FILE "menu"
#define MOUSE_FILE "mouse"
#define COOKIE_FILE "cookie"
#define HISTORY_FILE "history"
#define PRE_FORM_FILE RC_DIR "/pre_form"
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

#ifdef MAINPROGRAM
#define global
#define init(x) = (x)
#else /* not MAINPROGRAM */
#define global extern
#define init(x)
#endif /* not MAINPROGRAM */

#define DEFUN(funcname, macroname, docstring) void funcname(void)

/*
 * Macros.
 */

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

global const char *DefaultType init(nullptr);
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

global char *HTTP_proxy init(nullptr);
global char *HTTPS_proxy init(nullptr);
global char *FTP_proxy init(nullptr);
global struct Url HTTP_proxy_parsed;
global struct Url HTTPS_proxy_parsed;
global struct Url FTP_proxy_parsed;
global char *NO_proxy init(nullptr);
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

global char *document_root init(nullptr);
global char *personal_document_root init(nullptr);
global char *cgi_bin init(nullptr);
global char *index_file init(nullptr);

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
global char *displayTitleTerm init(nullptr);
global int displayLink init(false);
global int displayLinkNumber init(false);
global int displayLineInfo init(false);
global int retryAsHttp init(true);
global int displayImage init(false); /* XXX: emacs-w3m use display_image=off */
global int pseudoInlines init(true);
global char *Editor init(DEF_EDITOR);
global char *Mailer init(DEF_MAILER);
#define MAILTO_OPTIONS_IGNORE 1
#define MAILTO_OPTIONS_USE_MAILTO_URL 2
global int MailtoOptions init(MAILTO_OPTIONS_IGNORE);
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
global char *UserAgent init(nullptr);
global int NoSendReferer init(false);
global int CrossOriginReferer init(true);
global char *AcceptLang init(nullptr);
global char *AcceptEncoding init(nullptr);
global char *AcceptMedia init(nullptr);
global int WrapDefault init(false);
global int squeezeBlankLine init(false);
global char *BookmarkFile init(nullptr);
global int UseExternalDirBuffer init(true);
global int UseDictCommand init(true);
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

global struct auth_cookie *Auth_cookie init(nullptr);
global struct cookie *First_cookie init(nullptr);

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
global char DisplayBorders init(false);
global char DisableCenter init(false);
extern char *graph_symbol[];
extern char *graph2_symbol[];
#define N_GRAPH_SYMBOL 32
#define N_SYMBOL (N_GRAPH_SYMBOL + 14)
global int no_rc_dir init(false);
global char *rc_dir init(nullptr);
global char *config_file init(nullptr);
global int view_unseenobject init(true);
global int ssl_verify_server init(true);
global char *ssl_cert_file init(nullptr);
global char *ssl_key_file init(nullptr);
global char *ssl_ca_path init(nullptr);
global char *ssl_ca_file init("");
global int ssl_ca_default init(true);
global int ssl_path_modified init(false);
global char *ssl_forbid_method init("2, 3, t, 5");
global char *ssl_min_version init(nullptr);
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
global char *ssl_cipher init("DEFAULT:!LOW:!RC4:!EXP");
#else
global char *ssl_cipher init(nullptr);
#endif
global int is_redisplay init(false);
global int clear_buffer init(true);
global int use_lessopen init(false);
global char *keymap_file init(KEYMAP_FILE);
global int FollowRedirection init(10);
