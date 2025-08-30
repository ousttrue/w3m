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
#include "linein.h"
#include "url.h"
#include "form.h"
#include "parsetag.h"
#include "parsetagx.h"
#include "func.h"
#include "menu.h"
#include "textlist.h"
#include "funcname1.h"
#include "istream.h"
#include "anchor.h"
#include <unistd.h>
#include <gc.h>
#include <Str.h>
#include <locale.h>

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

enum BufferProperty {
    BP_NORMAL = 0x0,
    BP_PIPE = 0x1,
    BP_INTERNAL = 0x8,
    BP_NO_URL = 0x10,
    BP_REDIRECTED = 0x20,
    BP_CLOSE = 0x40,
};

/* Link Buffer */
#define LB_NOLINK -1
#define LB_INFO 0 /* pginfo() */
#define LB_N_INFO 1
#define LB_SOURCE 2 /* vwSrc() */
#define LB_N_SOURCE LB_SOURCE
#define MAX_LB 3

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

#define IS_EMPTY_PARSED_URL(pu) ((pu)->scheme == SCM_UNKNOWN && !(pu)->file)
#define SCONF_RESERVED 0
#define SCONF_SUBSTITUTE_URL 1
#define SCONF_URL_CHARSET 2
#define SCONF_NO_REFERER_FROM 3
#define SCONF_NO_REFERER_TO 4
#define SCONF_USER_AGENT 5
#define SCONF_N_FIELD 6
#define query_SCONF_SUBSTITUTE_URL(pu) ((const char*)querySiteconf(pu, SCONF_SUBSTITUTE_URL))
#define query_SCONF_USER_AGENT(pu) ((const char*)querySiteconf(pu, SCONF_USER_AGENT))
#define query_SCONF_URL_CHARSET(pu) ((const wc_ces*)querySiteconf(pu, SCONF_URL_CHARSET))
#define query_SCONF_NO_REFERER_FROM(pu) ((const int*)querySiteconf(pu, SCONF_NO_REFERER_FROM))
#define query_SCONF_NO_REFERER_TO(pu) ((const int*)querySiteconf(pu, SCONF_NO_REFERER_TO))

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

typedef struct _MapArea {
    char* url;
    char* target;
    char* alt;
    char shape;
    short* coords;
    int ncoords;
    short center_x;
    short center_y;
} MapArea;

typedef struct _MapList {
    Str name;
    GeneralList* area;
    struct _MapList* next;
} MapList;

#define NO_REFERER ((char*)-1)

typedef struct {
    BufferPoint* marks;
    int nmark;
    int markmax;
    int prevhseq;
} HmarkerList;

#define LINK_TYPE_NONE 0
#define LINK_TYPE_REL 1
#define LINK_TYPE_REV 2
typedef struct _LinkList {
    char* url;
    char* title; /* Next, Contents, ... */
    char* ctype; /* Content-Type */
    char type; /* Rel, Rev */
    struct _LinkList* next;
} LinkList;

typedef struct _Buffer {
    char* filename;
    char* buffername;
    Line* firstLine;
    Line* topLine;
    Line* currentLine;
    Line* lastLine;
    struct _Buffer* nextBuffer;
    struct _Buffer* linkBuffer[MAX_LB];
    short width;
    short height;
    char* type;
    char* real_type;
    int allLine;
    enum BufferProperty bufferprop;
    int currentColumn;
    short cursorX;
    short cursorY;
    int pos;
    int visualpos;
    short rootX;
    short rootY;
    short COLS;
    short LINES;
    InputStream pagerSource;
    AnchorList* href;
    AnchorList* name;
    AnchorList* img;
    AnchorList* formitem;
    LinkList* linklist;
    FormList* formlist;
    MapList* maplist;
    HmarkerList* hmarklist;
    HmarkerList* imarklist;
    ParsedURL currentURL;
    ParsedURL* baseURL;
    char* baseTarget;
    int real_scheme;
    char* sourcefile;
    int* clone;
    size_t trbyte;
    char check_url;
    wc_ces document_charset;
    wc_uint8 auto_detect;
    TextList* document_header;
    FormItemList* form_submit;
    char* savecache;
    char* edit;
    struct mailcap* mailcap;
    char* mailcap_source;
    char* header_source;
    char search_header;
    char* ssl_certificate;
    char image_flag;
    char image_loaded;
    char need_reshape;
    Anchor* submit;
    struct _BufferPos* undo;
    struct _AlarmEvent* event;
} Buffer;

typedef struct _BufferPos {
    long top_linenumber;
    long cur_linenumber;
    int currentColumn;
    int pos;
    int bpos;
    struct _BufferPos* next;
    struct _BufferPos* prev;
} BufferPos;

typedef struct _DownloadList {
    pid_t pid;
    char* url;
    char* save;
    char* lock;
    long long size;
    time_t time;
    int running;
    int err;
    struct _DownloadList* next;
    struct _DownloadList* prev;
} DownloadList;
#define DOWNLOAD_LIST_TITLE "Download List Panel"

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

/* flags for loadGeneralFile */
#define RG_NOCACHE 1

struct html_feed_environ {
    struct readbuffer* obuf;
    TextLineList* buf;
    FILE* f;
    Str tagbuf;
    int limit;
    int maxlimit;
    struct environment* envs;
    int nenv;
    int envc;
    int envc_real;
    char* title;
    int blank_lines;
};

struct portlist {
    unsigned short port;
    struct portlist* next;
};

struct cookie {
    ParsedURL url;
    Str name;
    Str value;
    time_t expires;
    Str path;
    Str domain;
    Str comment;
    Str commentURL;
    struct portlist* portl;
    char version;
    char flag;
    struct cookie* next;
};
#define COO_USE 1
#define COO_SECURE 2
#define COO_DOMAIN 4
#define COO_PATH 8
#define COO_DISCARD 16
#define COO_OVERRIDE 32 /* user chose to override security checks */

#define COO_OVERRIDE_OK 32 /* flag to specify that an error is overridable */
/* version 0 refers to the original cookie_spec.html */
/* version 1 refers to RFC 2109 */
/* version 1' refers to the Internet draft to obsolete RFC 2109 */
#define COO_EINTERNAL (1) /* unknown error; probably forgot to convert "return 1" in cookie.c */
#define COO_ETAIL (2 | COO_OVERRIDE_OK) /* tail match failed (version 0) */
#define COO_ESPECIAL (3) /* special domain check failed (version 0) */
#define COO_EPATH (4) /* Path attribute mismatch (version 1 case 1) */
#define COO_ENODOT (5 | COO_OVERRIDE_OK) /* no embedded dots in Domain (version 1 case 2.1) */
#define COO_ENOTV1DOM (6 | COO_OVERRIDE_OK) /* Domain does not start with a dot (version 1 case 2.2) */
#define COO_EDOM (7 | COO_OVERRIDE_OK) /* domain-match failed (version 1 case 3) */
#define COO_EBADHOST (8 | COO_OVERRIDE_OK) /* dot in matched host name in FQDN (version 1 case 4) */
#define COO_EPORT (9) /* Port match failed (version 1' case 5) */
#define COO_EMAX COO_EPORT

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

typedef struct http_request {
    char command;
    char flag;
    char* referer;
    FormList* request;
} HRequest;

#define HR_COMMAND_GET 0
#define HR_COMMAND_POST 1
#define HR_COMMAND_CONNECT 2
#define HR_COMMAND_HEAD 3

#define HR_FLAG_LOCAL 1
#define HR_FLAG_PROXY 2

#define HTST_UNKNOWN 255
#define HTST_MISSING 254
#define HTST_NORMAL 0
#define HTST_CONNECT 1

#define TMPF_DFL 0
#define TMPF_SRC 1
#define TMPF_CACHE 2
#define TMPF_COOKIE 3
#define TMPF_HIST 4
#define MAX_TMPF_TYPE 5

#define set_no_proxy(domains) (NO_proxy_domains = make_domain_list(domains))

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
global char ArgvIsURL init(TRUE);
global char MetaRefresh init(FALSE);
global char LocalhostOnly init(FALSE);
global char* HostName init(NULL);

global char QuietMessage init(FALSE);
global char TrapSignal init(TRUE);
#define TRAP_ON                                \
    if (TrapSignal) {                          \
        prevtrap = mySignal(SIGINT, KeyAbort); \
            term_cbreak();                     \
    }
#define TRAP_OFF                        \
    if (TrapSignal) {                   \
            term_raw();                 \
        if (prevtrap)                   \
            mySignal(SIGINT, prevtrap); \
    }

extern unsigned char GlobalKeymap[];
extern unsigned char EscKeymap[];
extern unsigned char EscBKeymap[];
extern unsigned char EscDKeymap[];
extern FuncList w3mFuncList[];

global char* HTTP_proxy init(NULL);
global char* HTTPS_proxy init(NULL);
global ParsedURL HTTP_proxy_parsed;
global ParsedURL HTTPS_proxy_parsed;
global char* NO_proxy init(NULL);
global int NOproxy_netaddr init(TRUE);
#ifdef INET6
#define DNS_ORDER_UNSPEC 0
#define DNS_ORDER_INET_INET6 1
#define DNS_ORDER_INET6_INET 2
#define DNS_ORDER_INET_ONLY 4
#define DNS_ORDER_INET6_ONLY 6
global int DNS_order init(DNS_ORDER_UNSPEC);
extern int ai_family_order_table[7][3]; /* XXX */
#endif /* INET6 */
global TextList* NO_proxy_domains;
global char NoCache init(FALSE);
global char use_proxy init(TRUE);
#define Do_not_use_proxy (!use_proxy)

global char* document_root init(NULL);
global char* personal_document_root init(NULL);
global char* cgi_bin init(NULL);
global char* index_file init(NULL);

global char* CurrentDir;
global int CurrentPid;

global Buffer* Currentbuf;
global Buffer* Firstbuf;

global DownloadList* FirstDL init(NULL);
global DownloadList* LastDL init(NULL);
global int CurrentKey;
global char* CurrentKeyData;
global char* CurrentCmdData;
global char* w3m_reqlog;
extern char* w3m_version;
extern int enable_inline_image;

global Str header_string init(NULL);
global int override_content_type init(FALSE);
global int override_user_agent init(FALSE);

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
global int DecodeURL init(FALSE);
global int retryAsHttp init(TRUE);
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
global char* UserAgent init(NULL);
global int NoSendReferer init(FALSE);
global int CrossOriginReferer init(TRUE);
global char* AcceptLang init(NULL);
global char* AcceptEncoding init(NULL);
global char* AcceptMedia init(NULL);
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
global char* mimetypes_files init(USER_MIMETYPES ", " SYS_MIMETYPES);

global TextList* fileToDelete;

extern struct Hist* LoadHist;
extern struct Hist* SaveHist;
extern struct Hist* URLHist;
extern struct Hist* ShellHist;
extern struct Hist* TextHist;
global int UseHistory init(TRUE);
global int URLHistSize init(100);
global int SaveURLHist init(TRUE);
global int multicolList init(FALSE);

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
#define url_quote_conv(x, c) url_quote(wc_conv_strict((x), InnerCharset, (c))->ptr)
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
global int use_cookie init(TRUE);
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

/*
 * Externals
 */

#include "table.h"
#include "proto.h"

#endif /* not FM_H */
