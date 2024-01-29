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
#define PAGER_MAX_LINE 10000 /* Maximum line kept as pager */

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
 * Types.
 */


#define LINK_TYPE_NONE 0
#define LINK_TYPE_REL 1
#define LINK_TYPE_REV 2
struct LinkList {
  char *url;
  char *title; /* Next, Contents, ... */
  char *ctype; /* Content-Type */
  char type;   /* Rel, Rev */
  LinkList *next;
};

#define AL_UNSET 0
#define AL_EXPLICIT 1
#define AL_IMPLICIT 2
#define AL_IMPLICIT_ONCE 3

struct AlarmEvent {
  int sec;
  short status;
  int cmd;
  void *data;
};

#define COPY_BUFROOT(dstbuf, srcbuf)                                           \
  {                                                                            \
    (dstbuf)->rootX = (srcbuf)->rootX;                                         \
    (dstbuf)->rootY = (srcbuf)->rootY;                                         \
    (dstbuf)->COLS = (srcbuf)->COLS;                                           \
    (dstbuf)->LINES = (srcbuf)->LINES;                                         \
  }

#define COPY_BUFPOSITION(dstbuf, srcbuf)                                       \
  {                                                                            \
    (dstbuf)->topLine = (srcbuf)->topLine;                                     \
    (dstbuf)->currentLine = (srcbuf)->currentLine;                             \
    (dstbuf)->pos = (srcbuf)->pos;                                             \
    (dstbuf)->cursorX = (srcbuf)->cursorX;                                     \
    (dstbuf)->cursorY = (srcbuf)->cursorY;                                     \
    (dstbuf)->visualpos = (srcbuf)->visualpos;                                 \
    (dstbuf)->currentColumn = (srcbuf)->currentColumn;                         \
  }
#define SAVE_BUFPOSITION(sbufp) COPY_BUFPOSITION(sbufp, Currentbuf)
#define RESTORE_BUFPOSITION(sbufp) COPY_BUFPOSITION(Currentbuf, sbufp)
#define TOP_LINENUMBER(buf) ((buf)->topLine ? (buf)->topLine->linenumber : 1)
#define CUR_LINENUMBER(buf)                                                    \
  ((buf)->currentLine ? (buf)->currentLine->linenumber : 1)

#define NO_BUFFER ((Buffer *)1)

#define _INIT_BUFFER_WIDTH (COLS - (showLineNum ? 6 : 1))
#define INIT_BUFFER_WIDTH ((_INIT_BUFFER_WIDTH > 0) ? _INIT_BUFFER_WIDTH : 0)
#define FOLD_BUFFER_WIDTH (FoldLine ? (INIT_BUFFER_WIDTH + 1) : -1)

#define in_bold fontstat[0]
#define in_under fontstat[1]
#define in_italic fontstat[2]
#define in_strike fontstat[3]
#define in_ins fontstat[4]
#define in_stand fontstat[5]

/* state of token scanning finite state machine */
#define R_ST_NORMAL 0  /* normal */
#define R_ST_TAG0 1    /* within tag, just after < */
#define R_ST_TAG 2     /* within tag */
#define R_ST_QUOTE 3   /* within single quote */
#define R_ST_DQUOTE 4  /* within double quote */
#define R_ST_EQL 5     /* = */
#define R_ST_AMP 6     /* within ampersand quote */
#define R_ST_EOL 7     /* end of file */
#define R_ST_CMNT1 8   /* <!  */
#define R_ST_CMNT2 9   /* <!- */
#define R_ST_CMNT 10   /* within comment */
#define R_ST_NCMNT1 11 /* comment - */
#define R_ST_NCMNT2 12 /* comment -- */
#define R_ST_NCMNT3 13 /* comment -- space */
#define R_ST_IRRTAG 14 /* within irregular tag */
#define R_ST_VALUE 15  /* within tag attribule value */

#define ST_IS_REAL_TAG(s)                                                      \
  ((s) == R_ST_TAG || (s) == R_ST_TAG0 || (s) == R_ST_EQL || (s) == R_ST_VALUE)

/* is this '<' really means the beginning of a tag? */
#define REALLY_THE_BEGINNING_OF_A_TAG(p)                                       \
  (IS_ALPHA(p[1]) || p[1] == '/' || p[1] == '!' || p[1] == '?' ||              \
   p[1] == '\0' || p[1] == '_')

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

#define HTST_UNKNOWN 255
#define HTST_MISSING 254
#define HTST_NORMAL 0
#define HTST_CONNECT 1

#define TMPF_DFL 0
#define TMPF_SRC 1
#define TMPF_FRAME 2
#define TMPF_CACHE 3
#define TMPF_COOKIE 4
#define TMPF_HIST 5
#define MAX_TMPF_TYPE 6

#define set_no_proxy(domains) (NO_proxy_domains = make_domain_list(domains))

/*
 * Globals.
 */

extern int LINES, COLS;
#define LASTLINE (LINES - 1)

global int Tabstop init(8);
global int IndentIncr init(4);
global int ShowEffect init(TRUE);
global int PagerMax init(PAGER_MAX_LINE);

global char SearchHeader init(FALSE);
global const char *DefaultType init(nullptr);
global char RenderFrame init(FALSE);
global char TargetSelf init(FALSE);
global char PermitSaveToPipe init(FALSE);
global char DecodeCTE init(FALSE);
global char AutoUncompress init(FALSE);
global char PreserveTimestamp init(TRUE);
global char ArgvIsURL init(TRUE);
global char MetaRefresh init(FALSE);
global char LocalhostOnly init(FALSE);
global char *HostName init(nullptr);

extern unsigned char GlobalKeymap[];
extern unsigned char EscKeymap[];
extern unsigned char EscBKeymap[];
extern unsigned char EscDKeymap[];

global char *HTTP_proxy init(nullptr);
global char *HTTPS_proxy init(nullptr);
global char *FTP_proxy init(nullptr);
global char *NO_proxy init(nullptr);
global int NOproxy_netaddr init(TRUE);
#ifdef INET6
#define DNS_ORDER_UNSPEC 0
#define DNS_ORDER_INET_INET6 1
#define DNS_ORDER_INET6_INET 2
#define DNS_ORDER_INET_ONLY 4
#define DNS_ORDER_INET6_ONLY 6
global int DNS_order init(DNS_ORDER_UNSPEC);
extern int ai_family_order_table[7][3]; /* XXX */
#endif                                  /* INET6 */
struct TextList;
global TextList *NO_proxy_domains;
global char use_proxy init(TRUE);
#define Do_not_use_proxy (!use_proxy)
global int Do_not_use_ti_te init(FALSE);

global char *document_root init(nullptr);
global char *personal_document_root init(nullptr);
global char *cgi_bin init(nullptr);
global char *index_file init(nullptr);

global char *CurrentDir;
global int CurrentPid;
/*
 * global Buffer *Currentbuf;
 * global Buffer *Firstbuf;
 */
struct TabBuffer;
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
struct DownloadList;
global DownloadList *FirstDL init(nullptr);
global DownloadList *LastDL init(nullptr);
global int CurrentKey;
global char *CurrentKeyData;
global char *CurrentCmdData;
global char *w3m_reqlog;
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
#ifdef USE_MARK
global int use_mark init(FALSE);
#endif
global int emacs_like_lineedit init(FALSE);
global int space_autocomplete init(FALSE);
global int vi_prec_num init(FALSE);
global int label_topline init(FALSE);
global int nextpage_topline init(FALSE);
global char *displayTitleTerm init(nullptr);
global int displayLink init(FALSE);
global int displayLinkNumber init(FALSE);
global int displayLineInfo init(FALSE);
global int DecodeURL init(FALSE);
global int retryAsHttp init(TRUE);
global int showLineNum init(FALSE);
global int show_srch_str init(TRUE);
global int displayImage init(FALSE); /* XXX: emacs-w3m use display_image=off */
global int pseudoInlines init(TRUE);
global char *Editor init(DEF_EDITOR);
#ifdef USE_W3MMAILER
global char *Mailer init(nullptr);
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
global int MarkAllPages init(FALSE);

#ifdef USE_MIGEMO
global int use_migemo init(FALSE);
global int migemo_active init(0);
global char *migemo_command init(DEF_MIGEMO_COMMAND);
#endif /* USE_MIGEMO */

global struct auth_cookie *Auth_cookie init(nullptr);
global struct cookie *First_cookie init(nullptr);

global char *mailcap_files init(USER_MAILCAP ", " SYS_MAILCAP);
global char *mimetypes_files init(USER_MIMETYPES ", " SYS_MIMETYPES);

global TextList *fileToDelete;

struct Hist;
extern Hist *LoadHist;
extern Hist *SaveHist;
extern Hist *URLHist;
extern Hist *ShellHist;
extern Hist *TextHist;
global int UseHistory init(TRUE);
global int URLHistSize init(100);
global int SaveURLHist init(TRUE);
global int multicolList init(FALSE);

#define Str_conv_from_system(x) (x)
#define Str_conv_to_system(x) (x)
#define Str_conv_to_halfdump(x) (x)
#define conv_from_system(x) (x)
#define conv_to_system(x) (x)
#define url_quote_conv(x, c) url_quote(x)
#define wc_Str_conv(x, charset0, charset1) (x)
#define wc_Str_conv_strict(x, charset0, charset1) (x)
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
global int no_rc_dir init(FALSE);
global char *rc_dir init(nullptr);
global char *tmp_dir;
global char *param_tmp_dir init(nullptr);
#ifdef HAVE_MKDTEMP
global char *mkd_tmp_dir init(nullptr);
#endif
global char *config_file init(nullptr);


global int view_unseenobject init(TRUE);

#if defined(USE_SSL) && defined(USE_SSL_VERIFY)
global int ssl_verify_server init(TRUE);
global char *ssl_cert_file init(nullptr);
global char *ssl_key_file init(nullptr);
global char *ssl_ca_path init(nullptr);
global char *ssl_ca_file init(DEF_CAFILE);
global int ssl_ca_default init(TRUE);
global int ssl_path_modified init(FALSE);
#endif /* defined(USE_SSL) &&                                                  \
        * defined(USE_SSL_VERIFY) */
global char *ssl_forbid_method init("2, 3, t, 5");
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
global char *ssl_cipher init("DEFAULT:!LOW:!RC4:!EXP");
#else
global char *ssl_cipher init(nullptr);
#endif

global int is_redisplay init(FALSE);
global int clear_buffer init(TRUE);
global double pixel_per_char init(DEFAULT_PIXEL_PER_CHAR);
global int pixel_per_char_i init(DEFAULT_PIXEL_PER_CHAR);
global int set_pixel_per_char init(FALSE);
global int use_lessopen init(FALSE);

global char *keymap_file init(KEYMAP_FILE);

#define get_mctype(c) (IS_CNTRL(*(c)) ? PC_CTRL : PC_ASCII)
#define get_mclen(c) 1
#define get_mcwidth(c) 1
#define get_strwidth(c) strlen(c)
#define get_Str_strwidth(c) ((c)->length)

global int FollowRedirection init(10);

global int w3m_backend init(FALSE);
struct TextLineList;
global TextLineList *backend_halfdump_buf;
global TextList *backend_batch_commands init(nullptr);
int backend(void);
extern void deleteFiles(void);
void w3m_exit(int i);

#endif /* not FM_H */
