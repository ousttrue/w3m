/*
 * w3m: WWW wo Miru utility
 *
 * by A.ITO  Feb. 1995
 *
 * You can use,copy,modify and distribute this program without any permission.
 */

#ifndef FM_H
#define FM_H

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
#define PAGER_MAX_LINE 10000 /* Maximum line kept as pager */

#define MAXIMUM_COLS 1024
#define DEFAULT_COLS 80

#define MAX_IMAGE 1000
#define MAX_IMAGE_SIZE 2048

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

/* Flags for displayBuffer() */
#define B_NORMAL 0
#define B_FORCE_REDRAW 1
#define B_REDRAW 2
#define B_SCROLL 3
#define B_REDRAW_IMAGE 4

/* Search Result */
#define SR_FOUND 0x1
#define SR_NOTFOUND 0x2
#define SR_WRAPPED 0x4

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

#define IS_EMPTY_PARSED_URL(pu) ((pu)->scheme == SCM_UNKNOWN && !(pu)->file)

/*
 * Macros.
 */

#define RELATIVE_WIDTH(w) (((w) >= 0) ? (int)((w) / pixel_per_char) : (w))
#define REAL_WIDTH(w, limit) (((w) >= 0) ? (int)((w) / pixel_per_char) : -(w) * (limit) / 100)

#define INLINE_IMG_NONE 0
#define INLINE_IMG_OSC5379 1
#define INLINE_IMG_SIXEL 2
#define INLINE_IMG_ITERM2 3
#define INLINE_IMG_KITTY 4

/*
 * Types.
 */

#define COPY_BUFROOT(dstbuf, srcbuf)       \
    {                                      \
        (dstbuf)->rootX = (srcbuf)->rootX; \
        (dstbuf)->rootY = (srcbuf)->rootY; \
        (dstbuf)->COLS = (srcbuf)->COLS;   \
        (dstbuf)->LINES = (srcbuf)->LINES; \
    }

#define COPY_BUFPOSITION(dstbuf, srcbuf)                   \
    {                                                      \
        (dstbuf)->topLine = (srcbuf)->topLine;             \
        (dstbuf)->currentLine = (srcbuf)->currentLine;     \
        (dstbuf)->pos = (srcbuf)->pos;                     \
        (dstbuf)->cursorX = (srcbuf)->cursorX;             \
        (dstbuf)->cursorY = (srcbuf)->cursorY;             \
        (dstbuf)->visualpos = (srcbuf)->visualpos;         \
        (dstbuf)->currentColumn = (srcbuf)->currentColumn; \
    }
#define SAVE_BUFPOSITION(sbufp) COPY_BUFPOSITION(sbufp, Currentbuf)
#define RESTORE_BUFPOSITION(sbufp) COPY_BUFPOSITION(Currentbuf, sbufp)
#define TOP_LINENUMBER(buf) ((buf)->topLine ? (buf)->topLine->linenumber : 1)
#define CUR_LINENUMBER(buf) ((buf)->currentLine ? (buf)->currentLine->linenumber : 1)

#define NO_BUFFER ((Buffer*)1)

#define _INIT_BUFFER_WIDTH (COLS - (showLineNum ? 6 : 1))
#define INIT_BUFFER_WIDTH ((_INIT_BUFFER_WIDTH > 0) ? _INIT_BUFFER_WIDTH : 0)
#define FOLD_BUFFER_WIDTH (FoldLine ? (INIT_BUFFER_WIDTH + 1) : -1)

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
#define RB_PREMODE (RB_PRE | RB_PRE_INT | RB_SCRIPT | RB_STYLE | RB_PLAIN | RB_INTXTA)
#define RB_SPECIAL (RB_PRE | RB_PRE_INT | RB_SCRIPT | RB_STYLE | RB_PLAIN | RB_NOBR)
#define RB_PLAIN_PRE 0x40000

#define RB_DEL 0x100000
#define RB_S 0x200000
#define RB_HTML5 0x400000

#define RB_GET_ALIGN(obuf) ((obuf)->flag & RB_ALIGN)
#define RB_SET_ALIGN(obuf, align)  \
    do {                           \
        (obuf)->flag &= ~RB_ALIGN; \
        (obuf)->flag |= (align);   \
    } while (0)
#define RB_SAVE_FLAG(obuf)                                              \
    {                                                                   \
        if ((obuf)->flag_sp < RB_STACK_SIZE)                            \
            (obuf)->flag_stack[(obuf)->flag_sp++] = RB_GET_ALIGN(obuf); \
    }
#define RB_RESTORE_FLAG(obuf)                                          \
    {                                                                  \
        if ((obuf)->flag_sp > 0)                                       \
            RB_SET_ALIGN(obuf, (obuf)->flag_stack[--(obuf)->flag_sp]); \
    }

/* state of token scanning finite state machine */
#define R_ST_NORMAL 0 /* normal */
#define R_ST_TAG0 1 /* within tag, just after < */
#define R_ST_TAG 2 /* within tag */
#define R_ST_QUOTE 3 /* within single quote */
#define R_ST_DQUOTE 4 /* within double quote */
#define R_ST_EQL 5 /* = */
#define R_ST_AMP 6 /* within ampersand quote */
#define R_ST_EOL 7 /* end of file */
#define R_ST_CMNT1 8 /* <!  */
#define R_ST_CMNT2 9 /* <!- */
#define R_ST_CMNT 10 /* within comment */
#define R_ST_NCMNT1 11 /* comment - */
#define R_ST_NCMNT2 12 /* comment -- */
#define R_ST_NCMNT3 13 /* comment -- space */
#define R_ST_IRRTAG 14 /* within irregular tag */
#define R_ST_VALUE 15 /* within tag attribule value */

#define ST_IS_REAL_TAG(s) ((s) == R_ST_TAG || (s) == R_ST_TAG0 || (s) == R_ST_EQL || (s) == R_ST_VALUE)

/* is this '<' really means the beginning of a tag? */
#define REALLY_THE_BEGINNING_OF_A_TAG(p) \
    (IS_ALPHA(p[1]) || p[1] == '/' || p[1] == '!' || p[1] == '?' || p[1] == '\0' || p[1] == '_')

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

global int IndentIncr init(4);
global int PagerMax init(PAGER_MAX_LINE);

global char SearchHeader init(FALSE);
global char* DefaultType init(0);
global char RenderFrame init(FALSE);
global char TargetSelf init(FALSE);
global char DecodeCTE init(FALSE);
global char AutoUncompress init(FALSE);
global char PreserveTimestamp init(TRUE);
global char ArgvIsURL init(TRUE);
global char MetaRefresh init(FALSE);
global char LocalhostOnly init(FALSE);
global char* HostName init(0);

global char fmInitialized init(FALSE);
global char TrapSignal init(TRUE);

#define TRAP_ON                                \
    if (TrapSignal) {                          \
        prevtrap = mySignal(SIGINT, KeyAbort); \
        if (fmInitialized)                     \
            term_cbreak();                     \
    }
#define TRAP_OFF                        \
    if (TrapSignal) {                   \
        if (fmInitialized)              \
            term_raw();                 \
        if (prevtrap)                   \
            mySignal(SIGINT, prevtrap); \
    }

global char* HTTP_proxy init(0);
global char* HTTPS_proxy init(0);
global char* GOPHER_proxy init(0);
global char* FTP_proxy init(0);
global struct Url HTTP_proxy_parsed;
global struct Url HTTPS_proxy_parsed;
global struct Url GOPHER_proxy_parsed;
global struct Url FTP_proxy_parsed;
global char* NO_proxy init(0);
global int NOproxy_netaddr init(TRUE);

global char use_proxy init(TRUE);
#define Do_not_use_proxy (!use_proxy)
global int Do_not_use_ti_te init(FALSE);
global char* NNTP_server init(0);
global char* NNTP_mode init(0);
global int MaxNewsMessage init(50);

global char* document_root init(0);
global char* personal_document_root init(0);
global char* cgi_bin init(0);
global char* index_file init(0);

global int CurrentKey;
global char* CurrentKeyData;
global char* CurrentCmdData;
global char* w3m_reqlog;
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

global int useColor init(TRUE);
global int highIntensityColors init(FALSE);
global int basic_color init(8); /* don't change */
global int anchor_color init(4); /* blue  */
global int image_color init(2); /* green */
global int form_color init(1); /* red   */
global int bg_color init(8); /* don't change */
global int mark_color init(6); /* cyan */
global int useActiveColor init(FALSE);
global int active_color init(6); /* cyan */
global int useVisitedColor init(FALSE);
global int visited_color init(5); /* magenta  */
global int confirm_on_quit init(TRUE);
global int use_mark init(FALSE);
global int emacs_like_lineedit init(FALSE);
global int space_autocomplete init(FALSE);
global int vi_prec_num init(FALSE);
global int label_topline init(FALSE);
global int nextpage_topline init(FALSE);
global char* displayTitleTerm init(0);
global int displayLink init(FALSE);
global int displayLinkNumber init(FALSE);
global int displayLineInfo init(FALSE);
global int DecodeURL init(FALSE);
global int retryAsHttp init(TRUE);
global int showLineNum init(FALSE);
global int show_srch_str init(TRUE);
global int activeImage init(FALSE);
global int displayImage init(TRUE);
global int autoImage init(TRUE);
global int useExtImageViewer init(TRUE);
global int maxLoadImage init(4);
global int image_map_list init(TRUE);
global int pseudoInlines init(TRUE);
#define DEF_EDITOR "/usr/bin/vi"
#define DEF_MAILER "/usr/bin/mail"
global char* Editor init(DEF_EDITOR);
global char* Mailer init(DEF_MAILER);
#define MAILTO_OPTIONS_IGNORE 1
#define MAILTO_OPTIONS_USE_MAILTO_URL 2
global int MailtoOptions init(MAILTO_OPTIONS_IGNORE);
#define DEF_EXT_BROWSER "/usr/bin/firefox"
global char* ExtBrowser init(DEF_EXT_BROWSER);
global char* ExtBrowser2 init(0);
global char* ExtBrowser3 init(0);
global char* ExtBrowser4 init(0);
global char* ExtBrowser5 init(0);
global char* ExtBrowser6 init(0);
global char* ExtBrowser7 init(0);
global char* ExtBrowser8 init(0);
global char* ExtBrowser9 init(0);
global int BackgroundExtViewer init(TRUE);
#define PRE_FORM_FILE RC_DIR "/pre_form"
global char* pre_form_file init(PRE_FORM_FILE);
global char* ftppasswd init(0);
global int ftppass_hostnamegen init(TRUE);
global int do_download init(FALSE);
global char* image_source init(0);
global int WrapDefault init(FALSE);
global int IgnoreCase init(TRUE);
global int WrapSearch init(FALSE);
global int squeezeBlankLine init(FALSE);
global char* BookmarkFile init(0);
global int UseExternalDirBuffer init(TRUE);
#define CGI_EXTENSION ".cgi"
global char* DirBufferCommand init("file:///$LIB/dirlist" CGI_EXTENSION);
global int UseDictCommand init(TRUE);
global char* DictCommand init("file:///$LIB/w3mdict" CGI_EXTENSION);
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

global struct auth_cookie* Auth_cookie init(0);

global char ExtHalfdump init(FALSE);
global char FollowLocale init(TRUE);
global char UseContentCharset init(TRUE);
global char SearchConv init(TRUE);
global char SimplePreserveSpace init(FALSE);
global char UseAltEntity init(FALSE);
global char DisplayBorders init(FALSE);
global char DisableCenter init(FALSE);
extern char* graph_symbol[];
extern char* graph2_symbol[];
extern int symbol_width;
extern int symbol_width0;
#define N_GRAPH_SYMBOL 32
#define N_SYMBOL (N_GRAPH_SYMBOL + 14)
#define SYMBOL_BASE 0x20
global char* rc_dir init(0);
global char* tmp_dir;
global char* param_tmp_dir init(0);
global char* mkd_tmp_dir init(0);
global char* config_file init(0);

global int relative_wheel_scroll init(FALSE);
global int fixed_wheel_scroll_count init(5);
global int relative_wheel_scroll_ratio init(30);

global int view_unseenobject init(FALSE);

global char* ssl_key_file init(0);
global char* ssl_ca_path init(0);
#define DEF_CAFILE ""
global char* ssl_ca_file init(DEF_CAFILE);
global int ssl_ca_default init(TRUE);
global char* ssl_forbid_method init("2, 3, t, 5");
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
global char* ssl_cipher init("DEFAULT:!LOW:!RC4:!EXP");
#else
global char* ssl_cipher init(0);
#endif

global int is_redisplay init(FALSE);
global int clear_buffer init(TRUE);

global int use_lessopen init(FALSE);

global int FollowRedirection init(10);

extern void deleteFiles(void);
void w3m_exit(int i);

#endif /* not FM_H */
