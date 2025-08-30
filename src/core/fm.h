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

/*
 * Constants.
 */
#define PAGER_MAX_LINE 10000 /* Maximum line kept as pager */

#define MAXIMUM_COLS 1024
#define DEFAULT_COLS 80

#define MAX_IMAGE 1000
#define MAX_IMAGE_SIZE 2048

#define DEFAULT_PIXEL_PER_CHAR 7.0 /* arbitrary */
#define DEFAULT_PIXEL_PER_LINE 14.0 /* arbitrary */
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

/* Search Result */
#define SR_FOUND 0x1
#define SR_NOTFOUND 0x2
#define SR_WRAPPED 0x4

#ifdef MAINPROGRAM
int REV_LB[MAX_LB] = {
    LB_N_INFO,
    LB_INFO,
    LB_N_SOURCE,
};
#else /* not MAINPROGRAM */
extern int REV_LB[];
#endif /* not MAINPROGRAM */

/* mark URL, Message-ID */
#define CHK_URL 1
#define CHK_NMID 2

#define IMG_FLAG_SKIP 1
#define IMG_FLAG_AUTO 2

#define IMG_FLAG_START 0
#define IMG_FLAG_STOP 1
#define IMG_FLAG_NEXT 2

#define IMG_FLAG_UNLOADED 0
#define IMG_FLAG_LOADED 1
#define IMG_FLAG_ERROR 2
#define IMG_FLAG_DONT_REMOVE 4


/*
 * Macros.
 */

#define bpcmp(a, b) \
    (((a).line - (b).line) ? ((a).line - (b).line) : ((a).pos - (b).pos))

#define RELATIVE_WIDTH(w) (((w) >= 0) ? (int)((w) / pixel_per_char) : (w))
#define REAL_WIDTH(w, limit) (((w) >= 0) ? (int)((w) / pixel_per_char) : -(w) * (limit) / 100)

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

#ifdef FORMAT_NICE
#define RB_FILL 0x80000
#endif /* FORMAT_NICE */
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

/*
 * Globals.
 */

global int IndentIncr init(4);
global int ShowEffect init(TRUE);
global int PagerMax init(PAGER_MAX_LINE);

global char SearchHeader init(FALSE);
global char* DefaultType init(NULL);
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


global int CurrentKey;
global char* CurrentKeyData;
global char* CurrentCmdData;
extern int enable_inline_image;

global int useColor init(TRUE);
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
global int label_topline init(FALSE);
global int nextpage_topline init(FALSE);
global int displayLink init(FALSE);
global int displayLinkNumber init(FALSE);
global int displayLineInfo init(FALSE);
global int showLineNum init(FALSE);
global int show_srch_str init(TRUE);
global char* Imgdisplay init(IMGDISPLAY);
global int activeImage init(FALSE);
global int displayImage init(TRUE);
global int autoImage init(TRUE);
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
global int do_download init(FALSE);
global char* image_source init(NULL);
global int WrapDefault init(FALSE);
global int IgnoreCase init(TRUE);
global int WrapSearch init(FALSE);
global int squeezeBlankLine init(FALSE);
global char* BookmarkFile init(NULL);
global int UseExternalDirBuffer init(TRUE);
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

global struct auth_cookie* Auth_cookie init(NULL);
global struct cookie* First_cookie init(NULL);

global char* mailcap_files init(USER_MAILCAP ", " SYS_MAILCAP);

global TextList* fileToDelete;

extern struct Hist* LoadHist;
extern struct Hist* SaveHist;
extern struct Hist* URLHist;
extern struct Hist* ShellHist;
extern struct Hist* TextHist;
global int UseHistory init(TRUE);
global int URLHistSize init(100);
global int SaveURLHist init(TRUE);

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
global int no_rc_dir init(FALSE);
global char* rc_dir init(NULL);
global char* param_tmp_dir init(NULL);
#ifdef HAVE_MKDTEMP
global char* mkd_tmp_dir init(NULL);
#endif
global char* config_file init(NULL);

global int default_use_cookie init(TRUE);
global int show_cookie init(FALSE);
global int accept_cookie init(TRUE);
#define ACCEPT_BAD_COOKIE_DISCARD 0
#define ACCEPT_BAD_COOKIE_ACCEPT 1
#define ACCEPT_BAD_COOKIE_ASK 2
global int accept_bad_cookie init(ACCEPT_BAD_COOKIE_DISCARD);
global char* cookie_reject_domains init(NULL);
global char* cookie_accept_domains init(NULL);
global char* cookie_avoid_wrong_number_of_dots init(NULL);
global TextList* Cookie_reject_domains;
global TextList* Cookie_accept_domains;
global TextList* Cookie_avoid_wrong_number_of_dots_domains;

global int view_unseenobject init(FALSE);

global int is_redisplay init(FALSE);
global int clear_buffer init(TRUE);
global double pixel_per_char init(DEFAULT_PIXEL_PER_CHAR);
global int pixel_per_char_i init(DEFAULT_PIXEL_PER_CHAR);
global int set_pixel_per_char init(FALSE);
global double pixel_per_line init(DEFAULT_PIXEL_PER_LINE);
global int pixel_per_line_i init(DEFAULT_PIXEL_PER_LINE);
global int set_pixel_per_line init(FALSE);
global double image_scale init(100);
global int use_lessopen init(FALSE);

global char* keymap_file init(KEYMAP_FILE);

global int FollowRedirection init(10);

extern void deleteFiles(void);
void w3m_exit(int i);

#define AL_UNSET 0
#define AL_EXPLICIT 1
#define AL_IMPLICIT 2
#define AL_IMPLICIT_ONCE 3

typedef struct _AlarmEvent {
    int sec;
    short status;
    int cmd;
    void* data;
} AlarmEvent;

#endif /* not FM_H */
