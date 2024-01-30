/*
 * w3m: WWW wo Miru utility
 *
 * by A.ITO  Feb. 1995
 *
 * You can use,copy,modify and distribute this program without any permission.
 */

#ifndef FM_H
#define FM_H

#include "config.h"

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
#define LINELEN 256          /* Initial line length */

#define MAXIMUM_COLS 1024
#define DEFAULT_COLS 80

#define DEFAULT_PIXEL_PER_CHAR 8.0 /* arbitrary */
#define MINIMUM_PIXEL_PER_CHAR 4.0
#define MAXIMUM_PIXEL_PER_CHAR 32.0

#ifdef FALSE
#undef FALSE
#endif

#ifdef TRUE
#undef TRUE
#endif

#define FALSE 0
#define TRUE 1

#define SHELLBUFFERNAME "*Shellout*"
#define PIPEBUFFERNAME "*stream*"
#define CPIPEBUFFERNAME "*stream(closed)*"
#define DICTBUFFERNAME "*dictionary*"

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

/*
 * Globals.
 */


global int Tabstop init(8);
global int IndentIncr init(4);
global int ShowEffect init(TRUE);

global char SearchHeader init(FALSE);
global const char *DefaultType init(nullptr);

global char TargetSelf init(FALSE);
global char PermitSaveToPipe init(FALSE);
global char DecodeCTE init(FALSE);
global char AutoUncompress init(FALSE);
global char PreserveTimestamp init(TRUE);
global char MetaRefresh init(FALSE);

extern unsigned char GlobalKeymap[];
extern unsigned char EscKeymap[];
extern unsigned char EscBKeymap[];
extern unsigned char EscDKeymap[];


global char *personal_document_root init(nullptr);


global int CurrentKey;
global char *CurrentKeyData;
global char *CurrentCmdData;
extern int enable_inline_image;

#define DUMP_BUFFER 0x01
#define DUMP_HEAD 0x02
#define DUMP_SOURCE 0x04
#define DUMP_EXTRA 0x08
#define DUMP_HALFDUMP 0x10
#define DUMP_FRAME 0x20

global int w3m_debug;
global int w3m_dump init(0);
#define w3m_halfdump (w3m_dump & DUMP_HALFDUMP)
global int w3m_halfload init(FALSE);

global int confirm_on_quit init(TRUE);
global int vi_prec_num init(FALSE);
global int label_topline init(FALSE);
global int nextpage_topline init(FALSE);
global int displayLink init(FALSE);
global int displayLinkNumber init(FALSE);
global int displayLineInfo init(FALSE);
global int showLineNum init(FALSE);
global int show_srch_str init(TRUE);
global int displayImage init(FALSE); /* XXX: emacs-w3m use display_image=off */
global int pseudoInlines init(TRUE);
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
global int BackgroundExtViewer init(TRUE);
global int disable_secret_security_check init(FALSE);
global char *passwd_file init(PASSWD_FILE);
global char *pre_form_file init(PRE_FORM_FILE);
global char *siteconf_file init(SITECONF_FILE);
global char *ftppasswd init(nullptr);
global int ftppass_hostnamegen init(TRUE);
global int do_download init(FALSE);

global int WrapDefault init(FALSE);
global int IgnoreCase init(TRUE);
global int WrapSearch init(FALSE);
global int squeezeBlankLine init(FALSE);
global char *BookmarkFile init(nullptr);
global int UseExternalDirBuffer init(TRUE);
global char *DirBufferCommand init("file:///$LIB/dirlist" CGI_EXTENSION);
global int UseDictCommand init(TRUE);
global char *DictCommand init("file:///$LIB/w3mdict" CGI_EXTENSION);
global int ignore_null_img_alt init(TRUE);
#define DISPLAY_INS_DEL_SIMPLE 0
#define DISPLAY_INS_DEL_NORMAL 1
#define DISPLAY_INS_DEL_FONTIFY 2
global int displayInsDel init(DISPLAY_INS_DEL_NORMAL);
global int FoldTextarea init(FALSE);
global int FoldLine init(FALSE);
#define DEFAULT_URL_EMPTY 0
#define DEFAULT_URL_CURRENT 1
#define DEFAULT_URL_LINK 2
global int DefaultURLString init(DEFAULT_URL_CURRENT);


global struct auth_cookie *Auth_cookie init(nullptr);
global struct cookie *First_cookie init(nullptr);

global char *mailcap_files init(USER_MAILCAP ", " SYS_MAILCAP);


#define url_quote_conv(x, c) url_quote(x)
#define wc_Str_conv(x, charset0, charset1) (x)
#define wc_Str_conv_strict(x, charset0, charset1) (x)
global char UseAltEntity init(FALSE);

global char DisplayBorders init(FALSE);
global char DisableCenter init(FALSE);
extern char *graph_symbol[];
extern char *graph2_symbol[];
extern int symbol_width;
extern int symbol_width0;
#define N_GRAPH_SYMBOL 32
#define N_SYMBOL (N_GRAPH_SYMBOL + 14)
#define SYMBOL_BASE 0x20

global int no_rc_dir init(FALSE);

global char *param_tmp_dir init(nullptr);
#ifdef HAVE_MKDTEMP
global char *mkd_tmp_dir init(nullptr);
#endif
global char *config_file init(nullptr);


global int view_unseenobject init(TRUE);


global int is_redisplay init(FALSE);
global int clear_buffer init(TRUE);
global double pixel_per_char init(DEFAULT_PIXEL_PER_CHAR);
global int pixel_per_char_i init(DEFAULT_PIXEL_PER_CHAR);
global int set_pixel_per_char init(FALSE);
global int use_lessopen init(FALSE);

global char *keymap_file init(KEYMAP_FILE);

global int FollowRedirection init(10);

global int w3m_backend init(FALSE);
struct TextLineList;
global TextLineList *backend_halfdump_buf;
struct TextList;
global TextList *backend_batch_commands init(nullptr);
int backend(void);
extern void deleteFiles(void);

#endif /* not FM_H */
