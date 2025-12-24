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
#include "history.h"

#define MENU_SELECT
#define MENU_MAP

#if !HAVE_SETLOCALE
#define setlocale(category, locale) /* empty */
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) gettext(String)
#define N_(String) (String)
#else
#undef bindtextdomain
#define bindtextdomain(Domain, Directory) /* empty */
#undef textdomain
#define textdomain(Domain) /* empty */
#define _(Text) Text
#define N_(Text) Text
#define gettext(Text) Text
#endif

#include "frame.h"
#include "parsetag.h"
#include "parsetagx.h"
#include "func.h"
#include "menu.h"
#include "textlist.h"
#include "funcname1.h"
#include "terms.h"
#include "istream.h"

#ifndef HAVE_BCOPY
void bcopy(const void*, void*, int);
void bzero(void*, int);
#endif /* HAVE_BCOPY */

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

#define DEFAULT_COLS 80

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
 * Line Property
 */

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

#define IMG_FLAG_UNLOADED 0
#define IMG_FLAG_LOADED 1
#define IMG_FLAG_ERROR 2
#define IMG_FLAG_DONT_REMOVE 4


/*
 * Macros.
 */

#define bpcmp(a, b) \
    (((a).line - (b).line) ? ((a).line - (b).line) : ((a).pos - (b).pos))

#define RELATIVE_WIDTH(w) (((w) >= 0) ? (int)((w) / getRuntime()->pixel_per_char) : (w))
#define REAL_WIDTH(w, limit) (((w) >= 0) ? (int)((w) / getRuntime()->pixel_per_char) : -(w) * (limit) / 100)

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

#include "line.h"
#include "url.h"

#define COPY_BUFROOT(dstbuf, srcbuf)       \
    {                                      \
        (dstbuf)->rootX = (srcbuf)->rootX; \
        (dstbuf)->rootY = (srcbuf)->rootY; \
        (dstbuf)->COLS = (srcbuf)->COLS;   \
        (dstbuf)->LINES = (srcbuf)->LINES; \
    }

#define COPY_BUFPOSITION(dstbuf, srcbuf)                   \
    {                                                      \
        (dstbuf)->doc.topLine = (srcbuf)->doc.topLine;             \
        (dstbuf)->doc.currentLine = (srcbuf)->doc.currentLine;     \
        (dstbuf)->pos = (srcbuf)->pos;                     \
        (dstbuf)->cursorX = (srcbuf)->cursorX;             \
        (dstbuf)->cursorY = (srcbuf)->cursorY;             \
        (dstbuf)->visualpos = (srcbuf)->visualpos;         \
        (dstbuf)->currentColumn = (srcbuf)->currentColumn; \
    }
#define SAVE_BUFPOSITION(sbufp) COPY_BUFPOSITION(sbufp, Currentbuf)
#define RESTORE_BUFPOSITION(sbufp) COPY_BUFPOSITION(Currentbuf, sbufp)
#define TOP_LINENUMBER(buf) ((buf)->doc.topLine ? (buf)->doc.topLine->linenumber : 1)
#define CUR_LINENUMBER(buf) ((buf)->doc.currentLine ? (buf)->doc.currentLine->linenumber : 1)

#ifdef USE_COOKIE
struct portlist {
    unsigned short port;
    struct portlist* next;
};

struct cookie {
    struct Url url;
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
#endif /* USE_COOKIE */

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

#define set_no_proxy(domains) (NO_proxy_domains = make_domain_list(domains))

/*
 * Globals.
 */

global int IndentIncr init(4);
global int PagerMax init(PAGER_MAX_LINE);

global char SearchHeader init(FALSE);
global char* DefaultType init(NULL);
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

global char TrapSignal init(TRUE);

extern unsigned char GlobalKeymap[];
extern unsigned char EscKeymap[];
extern unsigned char EscBKeymap[];
extern unsigned char EscDKeymap[];
#ifdef __EMX__
extern unsigned char PcKeymap[];
#endif
extern struct FuncList w3mFuncList[];

global char* HTTP_proxy init(NULL);
#ifdef USE_SSL
global char* HTTPS_proxy init(NULL);
#endif /* USE_SSL */
#ifdef USE_GOPHER
global char* GOPHER_proxy init(NULL);
#endif /* USE_GOPHER */
global char* FTP_proxy init(NULL);
global struct Url HTTP_proxy_parsed;
#ifdef USE_SSL
global struct Url HTTPS_proxy_parsed;
#endif /* USE_SSL */
#ifdef USE_GOPHER
global struct Url GOPHER_proxy_parsed;
#endif /* USE_GOPHER */
global struct Url FTP_proxy_parsed;
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
#ifdef USE_NNTP
global char* NNTP_server init(NULL);
global char* NNTP_mode init(NULL);
global int MaxNewsMessage init(50);
#endif

global char* document_root init(NULL);
global char* personal_document_root init(NULL);
global char* cgi_bin init(NULL);
global char* index_file init(NULL);

global char* CurrentDir;
global int CurrentPid;
#if defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE)
global char* MyProgramName init("w3m");
#endif /* defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE) */
/*
 * global Buffer *Currentbuf;
 * global Buffer *Firstbuf;
 */
global int open_tab_blank init(FALSE);
global int open_tab_dl_list init(FALSE);
global int close_tab_back init(FALSE);
global int TabCols init(10);
#define NO_TABBUFFER ((struct TabBuffer*)1)
global struct DownloadList* FirstDL init(NULL);
global struct DownloadList* LastDL init(NULL);
global char* w3m_reqlog;
extern char* w3m_version;

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
global Str header_string init(NULL);
global int override_content_type init(FALSE);
global int override_user_agent init(FALSE);

global int confirm_on_quit init(TRUE);
#ifdef USE_MARK
global int use_mark init(FALSE);
#endif
global int emacs_like_lineedit init(FALSE);
global int space_autocomplete init(FALSE);
global int vi_prec_num init(FALSE);
global int label_topline init(FALSE);
global int nextpage_topline init(FALSE);
global char* displayTitleTerm init(NULL);
global int displayLinkNumber init(FALSE);
global int retryAsHttp init(TRUE);
global int show_srch_str init(TRUE);
#ifdef USE_IMAGE
global char* Imgdisplay init(IMGDISPLAY);
global int autoImage init(TRUE);
global int useExtImageViewer init(TRUE);
global int maxLoadImage init(4);
global int image_map_list init(TRUE);
#else
global int displayImage init(FALSE); /* XXX: emacs-w3m use display_image=off */
#endif
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
global char* ftppasswd init(NULL);
global int ftppass_hostnamegen init(TRUE);
#ifdef USE_IMAGE
global char* image_source init(NULL);
#endif
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
#ifdef USE_DICT
global int UseDictCommand init(TRUE);
global char* DictCommand init("file:///$LIB/w3mdict" CGI_EXTENSION);
#endif /* USE_DICT */
global int ignore_null_img_alt init(TRUE);
#define DISPLAY_INS_DEL_SIMPLE 0
#define DISPLAY_INS_DEL_NORMAL 1
#define DISPLAY_INS_DEL_FONTIFY 2
global int displayInsDel init(DISPLAY_INS_DEL_NORMAL);
global int FoldTextarea init(FALSE);
#define DEFAULT_URL_EMPTY 0
#define DEFAULT_URL_CURRENT 1
#define DEFAULT_URL_LINK 2
global int DefaultURLString init(DEFAULT_URL_CURRENT);
global int MarkAllPages init(FALSE);

#ifdef USE_MIGEMO
global int use_migemo init(FALSE);
global int migemo_active init(0);
global char* migemo_command init(DEF_MIGEMO_COMMAND);
#endif /* USE_MIGEMO */

global struct auth_cookie* Auth_cookie init(NULL);
#ifdef USE_COOKIE
global struct cookie* First_cookie init(NULL);
#endif /* USE_COOKIE */

global char* mailcap_files init(USER_MAILCAP ", " SYS_MAILCAP);
global char* mimetypes_files init(USER_MIMETYPES ", " SYS_MIMETYPES);
#ifdef USE_EXTERNAL_URI_LOADER
global char* urimethodmap_files init(USER_URIMETHODMAP ", " SYS_URIMETHODMAP);
#endif

global TextList* fileToDelete;

global int multicolList init(FALSE);

global char FollowLocale init(TRUE);
global char UseContentCharset init(TRUE);
global char SearchConv init(TRUE);
global char SimplePreserveSpace init(FALSE);

global char UseAltEntity init(FALSE);
global char DisplayBorders init(FALSE);
global char DisableCenter init(FALSE);

global int no_rc_dir init(FALSE);
global char* rc_dir init(NULL);
global char* tmp_dir;
global char* param_tmp_dir init(NULL);
#ifdef HAVE_MKDTEMP
global char* mkd_tmp_dir init(NULL);
#endif
global char* config_file init(NULL);

#ifdef USE_COOKIE
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
#endif /* USE_COOKIE */

#ifdef USE_IMAGE
global int view_unseenobject init(FALSE);
#else
global int view_unseenobject init(TRUE);
#endif

#if defined(USE_SSL) && defined(USE_SSL_VERIFY)
global int ssl_verify_server init(TRUE);
global char* ssl_cert_file init(NULL);
global char* ssl_key_file init(NULL);
global char* ssl_ca_path init(NULL);
global char* ssl_ca_file init(DEF_CAFILE);
global int ssl_ca_default init(TRUE);
global int ssl_path_modified init(FALSE);
#endif /* defined(USE_SSL) && \
        * defined(USE_SSL_VERIFY) */
#ifdef USE_SSL
global char* ssl_forbid_method init("2, 3, t, 5");
#ifdef SSL_CTX_set_min_proto_version
global char* ssl_min_version init(NULL);
#endif
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
global char* ssl_cipher init("DEFAULT:!LOW:!RC4:!EXP");
#else
global char* ssl_cipher init(NULL);
#endif
#endif /* USE_SSL */

global int is_redisplay init(FALSE);
global int clear_buffer init(TRUE);
global double image_scale init(100);
global int use_lessopen init(FALSE);

global char* keymap_file init(KEYMAP_FILE);

global int FollowRedirection init(10);

global int w3m_backend init(FALSE);
global TextLineList* backend_halfdump_buf;
global TextList* backend_batch_commands init(NULL);
int backend(void);
extern void deleteFiles(void);

#ifdef USE_ALARM
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
#endif

/*
 * Externals
 */

#include "proto.h"

#endif /* not FM_H */
