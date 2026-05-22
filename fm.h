/* vi: set sw=4 ts=8 ai sm noet : */
/* 
 * w3m: WWW wo Miru utility
 * 
 * by A.ITO  Feb. 1995
 * 
 * You can use,copy,modify and distribute this program without any permission.
 */

#ifndef FM_H
#define FM_H

#include "config.h"	/* At top for defines below */

#ifndef USE_COLOR
#undef USE_ANSI_COLOR
#undef USE_BG_COLOR
#endif

#include "ctrlcode.h"
#include "buffer.h"
#include "file.h"
#include "form.h"
#include "frame.h"
#include "func.h"
#include "html.h"
#include "menu.h"
#include "parsetag.h"
#include "parsetagx.h"
#include "terms.h"
#include "textlist.h"

#include <gc/gc.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef USE_M17N
#include "libwc/wc.h"
#include "libwc/wtf.h"
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) gettext (String)
#define N_(String) (String)
#else
# define _(Text) Text
# define N_(Text) Text
#endif

#ifdef MAINPROGRAM
#define global
#define init(x) =(x)
#else				/* not MAINPROGRAM */
#define global extern
#define init(x)
#endif				/* not MAINPROGRAM */

#define DEFUN(funcname, macroname, docstring) void funcname(void)

/* 
 * Constants.
 */
#define LINELEN	256		/* Initial line length */
#define PAGER_MAX_LINE	10000	/* Maximum line kept as pager */

#define DEFAULT_COLS 80

#ifdef USE_IMAGE
#define MAX_IMAGE 1000
#define MAX_IMAGE_SIZE 2048

#define DEFAULT_PIXEL_PER_CHAR  7.0	/* arbitrary */
#define DEFAULT_PIXEL_PER_LINE  14.0	/* arbitrary */
#else
#define DEFAULT_PIXEL_PER_CHAR  8.0	/* arbitrary */
#endif
#define MINIMUM_PIXEL_PER_CHAR  4.0
#define MAXIMUM_PIXEL_PER_CHAR  32.0

#ifdef FALSE
#undef FALSE
#endif

#ifdef TRUE
#undef TRUE
#endif

#define FALSE 0
#define TRUE   1

#define SHELLBUFFERNAME	"*Shellout*"
#define PIPEBUFFERNAME	"*stream*"
#define CPIPEBUFFERNAME	"*stream(closed)*"
#ifdef USE_DICT
#define DICTBUFFERNAME "*dictionary*"
#endif				/* USE_DICT */

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

#ifdef MAINPROGRAM
int REV_LB[MAX_LB] = {
    LB_N_FRAME, LB_FRAME, LB_N_INFO, LB_INFO, LB_N_SOURCE,
};
#else				/* not MAINPROGRAM */
extern int REV_LB[];
#endif				/* not MAINPROGRAM */

/* 
 * Macros.
 */

#ifdef __EMX__
#define strcasecmp	stricmp
#define strncasecmp	strnicmp
#endif				/* __EMX__ */


#define SKIP_BLANKS(p) do{while(*(p)&&IS_SPACE(*(p)))(p)++;}while(0)
#define SKIP_NON_BLANKS(p) do{while(*(p)&&!IS_SPACE(*(p)))(p)++;}while(0)
#define IS_ENDL(c) ((c)=='\0'||(c)=='\r'||(c)=='\n')
#define IS_ENDT(c) (IS_ENDL(c)||(c)==';')

#define bpcmp(a,b) \
  (((a).line - (b).line) ? ((a).line - (b).line) : ((a).pos - (b).pos))

#define RELATIVE_WIDTH(w)   (((w)>=0)?(int)((w)/pixel_per_char):(w))
#define REAL_WIDTH(w,limit) (((w)>=0)?(int)((w)/pixel_per_char):-(w)*(limit)/100)

#define EOL(l) (&(l)->ptr[(l)->length])
#define IS_EOL(p,l) ((p)==&(l)->ptr[(l)->length])

/* 
 * Types.
 */

#define NO_REFERER ((char*)-1)

typedef struct _TabBuffer {
    struct _TabBuffer *nextTab;
    struct _TabBuffer *prevTab;
    Buffer *currentBuffer;
    Buffer *firstBuffer;
    short x1;
    short x2;
    short y;
} TabBuffer;

typedef struct _DownloadList {
    pid_t pid;
    char *url;
    char *save;
    char *lock;
    size_t size;
    time_t time;
    int running;
    int err;
    struct _DownloadList *next;
    struct _DownloadList *prev;
} DownloadList;
#define DOWNLOAD_LIST_TITLE "Download List Panel"

#define COPY_BUFROOT(dstbuf, srcbuf) {\
 (dstbuf)->rootX = (srcbuf)->rootX; \
 (dstbuf)->rootY = (srcbuf)->rootY; \
 (dstbuf)->COLS = (srcbuf)->COLS; \
 (dstbuf)->LINES = (srcbuf)->LINES; \
}

#define COPY_BUFPOSITION(dstbuf, srcbuf) {\
 (dstbuf)->topLine = (srcbuf)->topLine; \
 (dstbuf)->currentLine = (srcbuf)->currentLine; \
 (dstbuf)->pos = (srcbuf)->pos; \
 (dstbuf)->cursorX = (srcbuf)->cursorX; \
 (dstbuf)->cursorY = (srcbuf)->cursorY; \
 (dstbuf)->visualpos = (srcbuf)->visualpos; \
 (dstbuf)->currentColumn = (srcbuf)->currentColumn; \
}
#define SAVE_BUFPOSITION(sbufp) COPY_BUFPOSITION(sbufp, Currentbuf)
#define RESTORE_BUFPOSITION(sbufp) COPY_BUFPOSITION(Currentbuf, sbufp)
#define TOP_LINENUMBER(buf) ((buf)->topLine ? (buf)->topLine->linenumber : 1)
#define CUR_LINENUMBER(buf) ((buf)->currentLine ? (buf)->currentLine->linenumber : 1)

#define NO_BUFFER ((Buffer*)1)

#define TMPF_DFL	0
#define TMPF_SRC	1
#define TMPF_FRAME	2
#define TMPF_CACHE	3
#define TMPF_COOKIE	4
#define TMPF_HIST	5
#define MAX_TMPF_TYPE	6

/* 
 * Globals.
 */

#if defined(__CYGWIN__)
extern int LASTLINE;
#else				/* not defined(__CYGWIN__) */
#define LASTLINE (LINES-1)
#endif				/* not defined(__CYGWIN__) */

global int Tabstop init(8);
global int IndentIncr init(4);
global int MaxCols init(0);
global int ShowEffect init(TRUE);
global int PagerMax init(PAGER_MAX_LINE);
global int MessageDelay init(2);

global char SearchHeader init(FALSE);
global char *DefaultType init(NULL);
global char RenderFrame init(FALSE);
global char TargetSelf init(FALSE);
global char PermitSaveToPipe init(FALSE);
global char DecodeCTE init(FALSE);
global char AutoUncompress init(FALSE);
global char PreserveTimestamp init(TRUE);
global char ArgvIsURL init(TRUE);
global char MetaRefresh init(FALSE);
global char LocalhostOnly init(FALSE);
global char* HostName init(NULL);

global char fmInitialized init(FALSE);
global char QuietMessage init(FALSE);
global char TrapSignal init(TRUE);
#define TRAP_ON if (TrapSignal) { \
    prevtrap = mySignal(SIGINT, KeyAbort); \
    if (fmInitialized) \
	term_cbreak(); \
}
#define TRAP_OFF if (TrapSignal) { \
    if (fmInitialized) \
	term_raw(); \
    if (prevtrap) \
	mySignal(SIGINT, prevtrap); \
}

extern unsigned char GlobalKeymap[];
extern unsigned char EscKeymap[];
extern unsigned char EscBKeymap[];
extern unsigned char EscDKeymap[];
#ifdef __EMX__
extern unsigned char PcKeymap[];
#endif

global char *HTTP_proxy init(NULL);
#ifdef USE_SSL
global char *HTTPS_proxy init(NULL);
#endif				/* USE_SSL */
#ifdef USE_GOPHER
global char *GOPHER_proxy init(NULL);
#endif				/* USE_GOPHER */
global char *FTP_proxy init(NULL);
global ParsedURL HTTP_proxy_parsed;
#ifdef USE_SSL
global ParsedURL HTTPS_proxy_parsed;
#endif				/* USE_SSL */
#ifdef USE_GOPHER
global ParsedURL GOPHER_proxy_parsed;
#endif				/* USE_GOPHER */
global ParsedURL FTP_proxy_parsed;
global char *NO_proxy init(NULL);
global int NOproxy_netaddr init(TRUE);
#ifdef INET6
#define DNS_ORDER_UNSPEC     0
#define DNS_ORDER_INET_INET6 1
#define DNS_ORDER_INET6_INET 2
#define DNS_ORDER_INET_ONLY  4
#define DNS_ORDER_INET6_ONLY 6
global int DNS_order init(DNS_ORDER_UNSPEC);
extern int ai_family_order_table[7][3];	/* XXX */
#endif				/* INET6 */
global char NoCache init(FALSE);
global char use_proxy init(TRUE);
#define Do_not_use_proxy (!use_proxy)
global int Do_not_use_ti_te init(FALSE);
#ifdef USE_NNTP
global char *NNTP_server init(NULL);
global char *NNTP_mode init(NULL);
global int MaxNewsMessage init(50);
#endif

global char *document_root init(NULL);
global char *personal_document_root init(NULL);
global char *cgi_bin init(NULL);
global char *index_file init(NULL);

global char *CurrentDir;
global int CurrentPid;
#if defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE)
global char *MyProgramName init("w3m");
#endif /* defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE) */
global TabBuffer *CurrentTab;
global TabBuffer *FirstTab;
global TabBuffer *LastTab;
global int open_tab_blank init(FALSE);
global int open_tab_dl_list init(FALSE);
global int close_tab_back init(FALSE);
global int nTab;
global int TabCols init(10);
#define NO_TABBUFFER ((TabBuffer *)1)
#define Currentbuf (CurrentTab->currentBuffer)
#define Firstbuf (CurrentTab->firstBuffer)
global DownloadList *FirstDL init(NULL);
global DownloadList *LastDL init(NULL);
global int CurrentKey;
global char *CurrentKeyData;
global char *CurrentCmdData;
global char *w3m_reqlog;
extern int enable_inline_image;

#define DUMP_BUFFER   0x01
#define DUMP_HEAD     0x02
#define DUMP_SOURCE   0x04
#define DUMP_EXTRA    0x08
#define DUMP_HALFDUMP 0x10
#define DUMP_FRAME    0x20
global int w3m_debug;
global int w3m_dump init(0);
#define w3m_halfdump (w3m_dump & DUMP_HALFDUMP)
global int w3m_halfload init(FALSE);
global Str header_string init(NULL);
global int override_content_type init(FALSE);
global int override_user_agent init(FALSE);

#ifdef USE_COLOR
global int useColor init(TRUE);
global int highIntensityColors init(FALSE);
global int basic_color init(8);	/* don't change */
global int anchor_color init(4);	/* blue  */
global int image_color init(2);	/* green */
global int form_color init(1);	/* red   */
#ifdef USE_BG_COLOR
global int bg_color init(8);	/* don't change */
global int mark_color init(6);	/* cyan */
#endif				/* USE_BG_COLOR */
global int useActiveColor init(FALSE);
global int active_color init(6);	/* cyan */
global int useVisitedColor init(FALSE);
global int visited_color init(5);	/* magenta  */
#endif				/* USE_COLOR */
global int confirm_on_quit init(TRUE);
global int exit_on_last init(FALSE);
#ifdef USE_MARK
global int use_mark init(FALSE);
#endif
global int emacs_like_lineedit init(FALSE);
global int rl_paste init(FALSE);
global int space_autocomplete init(FALSE);
global int vi_prec_num init(FALSE);
global int label_topline init(FALSE);
global int nextpage_topline init(FALSE);
global char *displayTitleTerm init(NULL);
global int displayLink init(FALSE);
global int displayLinkNumber init(FALSE);
global int zeroBasedLinkNo init(FALSE);
global int displayLineInfo init(FALSE);
global int displayColumnNumber init(FALSE);
global int DecodeURL init(FALSE);
global int retryAsHttp init(TRUE);
global int showLineNum init(FALSE);
global int show_srch_str init(TRUE);
#ifdef USE_IMAGE
global char *Imgdisplay init(IMGDISPLAY);
global int activeImage init(FALSE);
global int displayImage init(TRUE);
global int autoImage init(TRUE);
global int useExtImageViewer init(TRUE);
global int maxLoadImage init(4);
global int image_map_list init(TRUE);
#else
global int displayImage init(FALSE);	/* XXX: emacs-w3m use display_image=off */
#endif
global int pseudoInlines init(TRUE);
global const char *Editor init(DEF_EDITOR);
#ifdef USE_W3MMAILER
global char *Mailer init(NULL);
#else
global char *Mailer init(DEF_MAILER);
#endif
#ifdef USE_W3MMAILER
#define MAILTO_OPTIONS_USE_W3MMAILER 0
#endif
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
global int BackgroundExtViewer init(TRUE);
global int disable_secret_security_check init(FALSE);
global char *passwd_file init(NULL);
global char *pre_form_file init(NULL);
global char *siteconf_file init(NULL);
global char *ftppasswd init(NULL);
global int ftppass_hostnamegen init(TRUE);
global int do_download init(FALSE);
#ifdef USE_IMAGE
global char *image_source init(NULL);
#endif
global char *UserAgent init(NULL);
global int NoSendReferer init(FALSE);
global int CrossOriginReferer init(TRUE);
global char *AcceptLang init(NULL);
global char *AcceptEncoding init(NULL);
global char *AcceptMedia init(NULL);
global int WrapDefault init(FALSE);
global int IgnoreCase init(TRUE);
global int SmartCase init(FALSE);
global int WrapSearch init(FALSE);
global int squeezeBlankLine init(FALSE);
global char *BookmarkFile init(NULL);
global int UseExternalDirBuffer init(TRUE);
global const char *DirBufferCommand init("file:///$LIB/dirlist" CGI_EXTENSION);
#ifdef USE_DICT
global int UseDictCommand init(TRUE);
global const char *DictCommand init("file:///$LIB/w3mdict" CGI_EXTENSION);
global const char *DictPrompt init("(dictionary)!");
#endif				/* USE_DICT */
global int ignore_null_img_alt init(TRUE);
#define DISPLAY_INS_DEL_SIMPLE	0
#define DISPLAY_INS_DEL_NORMAL	1
#define DISPLAY_INS_DEL_FONTIFY	2
global int displayInsDel init(DISPLAY_INS_DEL_NORMAL);
global int FoldTextarea init(FALSE);
global int FoldPre init(FALSE);
global int FoldLine init(FALSE);
#define DEFAULT_URL_EMPTY	0
#define DEFAULT_URL_CURRENT	1
#define DEFAULT_URL_LINK	2
global int DefaultURLString init(DEFAULT_URL_CURRENT);
global int MarkAllPages init(FALSE);

#ifdef USE_MIGEMO
global int use_migemo init(FALSE);
global int migemo_active init(0);
global char *migemo_command init(DEF_MIGEMO_COMMAND);
#endif				/* USE_MIGEMO */

global char *mailcap_files init(NULL);
global char *mimetypes_files init(NULL);
#ifdef USE_EXTERNAL_URI_LOADER
global char *urimethodmap_files init(NULL);
#endif

global TextList *fileToDelete;

#ifdef USE_HISTORY
global int UseHistory init(TRUE);
global int URLHistSize init(100);
global int SaveURLHist init(TRUE);
#endif				/* USE_HISTORY */
global int multicolList init(FALSE);

#ifdef USE_M17N
global wc_ces InnerCharset init(WC_CES_WTF);	/* Don't change */
global wc_ces DisplayCharset init(DISPLAY_CHARSET);
global wc_ces DocumentCharset init(DOCUMENT_CHARSET);
global wc_ces SystemCharset init(SYSTEM_CHARSET);
global wc_ces BookmarkCharset init(SYSTEM_CHARSET);
global char ExtHalfdump init(FALSE);
global char FollowLocale init(TRUE);
global char UseContentCharset init(TRUE);
global char SearchConv init(TRUE);
global char SimplePreserveSpace init(FALSE);
#define Str_conv_from_system(x) wc_Str_conv((x), SystemCharset, InnerCharset)
#define Str_conv_to_system(x) wc_Str_conv_strict((x), InnerCharset, SystemCharset)
#define Str_conv_to_halfdump(x) (ExtHalfdump ? wc_Str_conv((x), InnerCharset, DisplayCharset) : (x))
#define conv_from_system(x) wc_conv((x), SystemCharset, InnerCharset)->ptr
#define conv_to_system(x) wc_conv_strict((x), InnerCharset, SystemCharset)->ptr
#define url_quote_conv(x,c) url_quote(wc_conv_strict((x), InnerCharset, (c))->ptr)
#else
#define Str_conv_from_system(x) (x)
#define Str_conv_to_system(x) (x)
#define Str_conv_to_halfdump(x) (x)
#define conv_from_system(x) (x)
#define conv_to_system(x) (x)
#define url_quote_conv(x,c) url_quote(x)
#define wc_Str_conv(x,charset0,charset1) (x)
#define wc_Str_conv_strict(x,charset0,charset1) (x)
#endif
global char UseAltEntity init(FALSE);
#define GRAPHIC_CHAR_ASCII 2
#define GRAPHIC_CHAR_DEC 1
#define GRAPHIC_CHAR_CHARSET 0
global char UseGraphicChar init(GRAPHIC_CHAR_CHARSET);
global char DisplayBorders init(FALSE);
global char DisableCenter init(FALSE);
extern char *graph_symbol[];
extern char *graph2_symbol[];
extern int symbol_width;
extern int symbol_width0;
#define N_GRAPH_SYMBOL 32
#define N_SYMBOL (N_GRAPH_SYMBOL + 14)
#define SYMBOL_BASE 0x20
global char *rc_dir init(NULL);
global char *tmp_dir;
global char *param_tmp_dir init(NULL);
global char *param_dl_dir init(NULL);
#ifdef HAVE_MKDTEMP
global char *mkd_tmp_dir init(NULL);
#endif
global char *config_file init(NULL);

#ifdef USE_MOUSE
global int use_mouse init(TRUE);
global int reverse_mouse init(FALSE);
global int relative_wheel_scroll init(FALSE);
global int fixed_wheel_scroll_count init(5);
global int relative_wheel_scroll_ratio init(30);
typedef struct _MouseActionMap {
    void (*func) (void);
    char *data;
} MouseActionMap;
typedef struct _MouseAction {
    char *menu_str;
    char *lastline_str;
    int menu_width;
    int lastline_width;
    int in_action;
    int cursorX;
    int cursorY;
    MouseActionMap default_map[3];
    MouseActionMap anchor_map[3];
    MouseActionMap active_map[3];
    MouseActionMap tab_map[3];
    MouseActionMap *menu_map[3];
    MouseActionMap *lastline_map[3];
} MouseAction;
global MouseAction mouse_action;
#define LIMIT_MOUSE_MENU 100
#endif				/* USE_MOUSE */

#ifdef USE_IMAGE
global int view_unseenobject init(FALSE);
#else
global int view_unseenobject init(TRUE);
#endif

#if defined(USE_SSL) && defined(USE_SSL_VERIFY)
global int ssl_verify_server init(TRUE);
global char *ssl_cert_file init(NULL);
global char *ssl_key_file init(NULL);
global char *ssl_ca_path init(NULL);
global char *ssl_ca_file init(DEF_CAFILE);
global int ssl_ca_default init(TRUE);
global int ssl_path_modified init(FALSE);
#endif				/* defined(USE_SSL) &&
				 * defined(USE_SSL_VERIFY) */
#ifdef USE_SSL
#include <openssl/ssl.h>
global char *ssl_forbid_method init("2, 3, t, 5");
#ifdef SSL_CTX_set_min_proto_version
global char *ssl_min_version init(NULL);
#endif
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
global char *ssl_cipher init("DEFAULT:!LOW:!RC4:!EXP");
#else
global char *ssl_cipher init(NULL);
#endif
#endif				/* USE_SSL */

global int is_redisplay init(FALSE);
global int clear_buffer init(TRUE);
global double pixel_per_char init(DEFAULT_PIXEL_PER_CHAR);
global int pixel_per_char_i init(DEFAULT_PIXEL_PER_CHAR);
global int set_pixel_per_char init(FALSE);
#ifdef USE_IMAGE
global double pixel_per_line init(DEFAULT_PIXEL_PER_LINE);
global int pixel_per_line_i init(DEFAULT_PIXEL_PER_LINE);
global int set_pixel_per_line init(FALSE);
global double image_scale init(100);
#endif
global int use_lessopen init(FALSE);

global char *keymap_file init(KEYMAP_FILE);

#ifdef USE_M17N
#define get_mctype(c) ((Lineprop)wtf_type((const wc_uchar *)(c)) << 8)
#define get_mclen(c) wtf_len1((const wc_uchar *)(c))
#define get_mcwidth(c) wtf_width((const wc_uchar *)(c))
#define get_strwidth(c) wtf_strwidth((const wc_uchar *)(c))
#define get_Str_strwidth(c) wtf_strwidth((wc_uchar *)((c)->ptr))
#else
#define get_mctype(c) (IS_CNTRL(*(c)) ? PC_CTRL : PC_ASCII)
#define get_mclen(c) 1
#define get_mcwidth(c) 1
#define get_strwidth(c) strlen(c)
#define get_Str_strwidth(c) ((c)->length)
#endif

global int FollowRedirection init(10);

global int w3m_backend init(FALSE);
global TextLineList *backend_halfdump_buf;
global TextList *backend_batch_commands init(NULL);
int backend(void);

#ifdef USE_ALARM
#define AL_UNSET         0
#define AL_EXPLICIT      1
#define AL_IMPLICIT      2
#define AL_IMPLICIT_ONCE 3

typedef struct _AlarmEvent {
    int sec;
    short status;
    int cmd;
    void *data;
} AlarmEvent;
#endif

/* 
 * Externals
 */

#endif				/* not FM_H */
