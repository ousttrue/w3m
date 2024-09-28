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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* strcasestr() */
#endif

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

#define MAXIMUM_COLS 1024
#define DEFAULT_COLS 80

#define DEFAULT_PIXEL_PER_CHAR 8.0 /* arbitrary */
#define MINIMUM_PIXEL_PER_CHAR 4.0
#define MAXIMUM_PIXEL_PER_CHAR 32.0

#define SHELLBUFFERNAME "*Shellout*"
#define PIPEBUFFERNAME "*stream*"
#define CPIPEBUFFERNAME "*stream(closed)*"
#define DICTBUFFERNAME "*dictionary*"

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

/* Flags for displayBuffer() */
#define B_NORMAL 0
#define B_FORCE_REDRAW 1
#define B_REDRAW 2
#define B_SCROLL 3
#define B_REDRAW_IMAGE 4

/* Buffer Property */
#define BP_NORMAL 0x0
#define BP_PIPE 0x1
#define BP_FRAME 0x2
#define BP_INTERNAL 0x8
#define BP_NO_URL 0x10
#define BP_REDIRECTED 0x20
#define BP_CLOSE 0x40

/* Search Result */
#define SR_FOUND 0x1
#define SR_NOTFOUND 0x2
#define SR_WRAPPED 0x4

/* mark URL, Message-ID */
#define CHK_URL 1
#define CHK_NMID 2

/* Flags for calcPosition() */
#define CP_AUTO 0
#define CP_FORCE 1

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

/* Flags for inputLine() */
#define IN_STRING 0x10
#define IN_FILENAME 0x20
#define IN_PASSWORD 0x40
#define IN_COMMAND 0x80
#define IN_URL 0x100
#define IN_CHAR 0x200

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
#define SCONF_RESERVED 0
#define SCONF_SUBSTITUTE_URL 1
#define SCONF_URL_CHARSET 2
#define SCONF_NO_REFERER_FROM 3
#define SCONF_NO_REFERER_TO 4
#define SCONF_USER_AGENT 5
#define SCONF_N_FIELD 6
#define query_SCONF_SUBSTITUTE_URL(pu)                                         \
  ((const char *)querySiteconf(pu, SCONF_SUBSTITUTE_URL))
#define query_SCONF_USER_AGENT(pu)                                             \
  ((const char *)querySiteconf(pu, SCONF_USER_AGENT))
#define query_SCONF_URL_CHARSET(pu)                                            \
  ((const wc_ces *)querySiteconf(pu, SCONF_URL_CHARSET))
#define query_SCONF_NO_REFERER_FROM(pu)                                        \
  ((const int *)querySiteconf(pu, SCONF_NO_REFERER_FROM))
#define query_SCONF_NO_REFERER_TO(pu)                                          \
  ((const int *)querySiteconf(pu, SCONF_NO_REFERER_TO))

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

#define RB_PRE 0x01
#define RB_SCRIPT 0x02
#define RB_STYLE 0x04
#define RB_PLAIN 0x08
#define RB_LEFT 0x10
#define RB_CENTER 0x20
#define RB_RIGHT 0x40
#define RB_ALIGN (RB_LEFT | RB_CENTER | RB_RIGHT)
#define RB_NOBR 0x80
#define RB_P 0x100
#define RB_PRE_INT 0x200
#define RB_IN_DT 0x400
#define RB_INTXTA 0x800
#define RB_INSELECT 0x1000
#define RB_IGNORE_P 0x2000
#define RB_TITLE 0x4000
#define RB_NFLUSHED 0x8000
#define RB_NOFRAMES 0x10000
#define RB_INTABLE 0x20000
#define RB_PREMODE                                                             \
  (RB_PRE | RB_PRE_INT | RB_SCRIPT | RB_STYLE | RB_PLAIN | RB_INTXTA)
#define RB_SPECIAL                                                             \
  (RB_PRE | RB_PRE_INT | RB_SCRIPT | RB_STYLE | RB_PLAIN | RB_NOBR)
#define RB_PLAIN_PRE 0x40000

#define RB_FILL 0x80000
#define RB_DEL 0x100000
#define RB_S 0x200000
#define RB_HTML5 0x400000

#define RB_GET_ALIGN(obuf) ((obuf)->flag & RB_ALIGN)
#define RB_SET_ALIGN(obuf, align)                                              \
  do {                                                                         \
    (obuf)->flag &= ~RB_ALIGN;                                                 \
    (obuf)->flag |= (align);                                                   \
  } while (0)
#define RB_SAVE_FLAG(obuf)                                                     \
  {                                                                            \
    if ((obuf)->flag_sp < RB_STACK_SIZE)                                       \
      (obuf)->flag_stack[(obuf)->flag_sp++] = RB_GET_ALIGN(obuf);              \
  }
#define RB_RESTORE_FLAG(obuf)                                                  \
  {                                                                            \
    if ((obuf)->flag_sp > 0)                                                   \
      RB_SET_ALIGN(obuf, (obuf)->flag_stack[--(obuf)->flag_sp]);               \
  }

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

/* flags for loadGeneralFile */
#define RG_NOCACHE 1

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


/*
 * Globals.
 */

global int Tabstop init(8);
global int IndentIncr init(4);

global char SearchHeader init(false);
global char *DefaultType init(NULL);
global char RenderFrame init(false);
global char TargetSelf init(false);
global char PermitSaveToPipe init(false);
global char DecodeCTE init(false);
global char AutoUncompress init(false);
global char PreserveTimestamp init(true);
global char ArgvIsURL init(true);
global char MetaRefresh init(false);
global char LocalhostOnly init(false);
global char *HostName init(NULL);

global char QuietMessage init(false);
global char TrapSignal init(true);

#define TRAP_ON                                                                \
  if (TrapSignal) {                                                            \
    prevtrap = mySignal(SIGINT, KeyAbort);                                     \
    term_cbreak();                                                             \
  }
#define TRAP_OFF                                                               \
  if (TrapSignal) {                                                            \
    term_raw();                                                                \
    if (prevtrap)                                                              \
      mySignal(SIGINT, prevtrap);                                              \
  }

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
global int Do_not_use_ti_te init(false);

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
global char *CurrentKeyData;
global char *CurrentCmdData;
extern char *w3m_version;
extern int enable_inline_image;

global int w3m_debug;
#define w3m_halfdump (w3m_dump & DUMP_HALFDUMP)
global int w3m_halfload init(false);
global int override_content_type init(false);
global int override_user_agent init(false);

global int confirm_on_quit init(true);
global int emacs_like_lineedit init(false);
global int space_autocomplete init(false);
global int vi_prec_num init(false);
global int label_topline init(false);
global int nextpage_topline init(false);
global char *displayTitleTerm init(NULL);
global int displayLink init(false);
global int displayLinkNumber init(false);
global int displayLineInfo init(false);
global int DecodeURL init(false);
global int retryAsHttp init(true);
global int show_srch_str init(true);
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
global int disable_secret_security_check init(false);
global char *passwd_file init(PASSWD_FILE);
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
global int IgnoreCase init(true);
global int WrapSearch init(false);
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


extern struct Hist *LoadHist;
extern struct Hist *SaveHist;
extern struct Hist *URLHist;
extern struct Hist *ShellHist;
extern struct Hist *TextHist;
global int UseHistory init(true);
global int URLHistSize init(100);
global int SaveURLHist init(true);
global int multicolList init(false);

#define Str_conv_from_system(x) (x)
#define Str_conv_to_system(x) (x)
#define Str_conv_to_halfdump(x) (x)
#define conv_from_system(x) (x)
#define conv_to_system(x) (x)
#define url_quote_conv(x, c) url_quote(x)
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
extern int symbol_width;
extern int symbol_width0;
#define N_GRAPH_SYMBOL 32
#define N_SYMBOL (N_GRAPH_SYMBOL + 14)
#define SYMBOL_BASE 0x20
global int no_rc_dir init(false);
global char *rc_dir init(NULL);
global char *config_file init(NULL);

global int view_unseenobject init(true);

global int ssl_verify_server init(true);
global char *ssl_cert_file init(NULL);
global char *ssl_key_file init(NULL);
global char *ssl_ca_path init(NULL);
global char *ssl_ca_file init(DEF_CAFILE);
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
global double pixel_per_char init(DEFAULT_PIXEL_PER_CHAR);
global int pixel_per_char_i init(DEFAULT_PIXEL_PER_CHAR);
global int set_pixel_per_char init(false);
global int use_lessopen init(false);

global char *keymap_file init(KEYMAP_FILE);

global int FollowRedirection init(10);

extern void deleteFiles(void);
void w3m_exit(int i);

/*
 * Externals
 */

#include "table.h"
#include "proto.h"

#endif /* not FM_H */
