/* config.h.  Generated from config.h.in by configure.  */
#ifndef CONFIG_H_SEEN
#define CONFIG_H_SEEN
/*
 * Configuration for w3m
 */
#define JA 0
#define EN 1

/* Name of package */
#define PACKAGE "w3m"

#define HELP_FILE "w3mhelp-w3m_en.html"
#define HELP_CGI     "w3mhelp"
#define W3MCONFIG    "w3mconfig"
#define IMGDISPLAY   "w3mimgdisplay"
#define XFACE2XPM    "xface2xpm"

#define BOOKMARK     "bookmark.html"
#define CONFIG_FILE  "config"
#define KEYMAP_FILE  "keymap"
#define MENU_FILE    "menu"
#define MOUSE_FILE   "mouse"
#define COOKIE_FILE  "cookie"
#define HISTORY_FILE "history"

#define PASSWD_FILE	RC_DIR "/passwd"
#define PRE_FORM_FILE	RC_DIR "/pre_form"
#define SITECONF_FILE	RC_DIR "/siteconf"
#define USER_MAILCAP	RC_DIR "/mailcap"
#define SYS_MAILCAP	CONF_DIR "/mailcap"
#define USER_MIMETYPES	"~/.mime.types"
#define SYS_MIMETYPES	ETC_DIR "/mime.types"
#define USER_URIMETHODMAP	RC_DIR "/urimethodmap"
#define SYS_URIMETHODMAP	CONF_DIR "/urimethodmap"

#define DEF_SAVE_FILE "index.html"

/* User Configuration */
#define DISPLAY_CHARSET WC_CES_UTF_8
#define SYSTEM_CHARSET WC_CES_UTF_8
#define DOCUMENT_CHARSET WC_CES_UTF_8
// #define USE_M17N 1
// #define USE_UNICODE 1
#define W3M_LANG EN
#define LANG W3M_LANG

/* Define to 1 if translation of program messages to the user's
   native language is requested. */
// #define ENABLE_NLS 1

// #define USE_COLOR 1
// #define USE_ANSI_COLOR 1
// #define USE_BG_COLOR 1
/* #undef USE_MIGEMO */
#define USE_MARK
#define USE_MOUSE 1
/* #undef USE_GPM */
/* #undef USE_SYSMOUSE */
// #define USE_MENU 1
#define USE_COOKIE 1
#define USE_DIGEST_AUTH 1
#define USE_SSL 1
#define USE_SSL_VERIFY 1
#define DEF_CAFILE ""
#define USE_HELP_CGI 1
#define USE_EXTERNAL_URI_LOADER 1
#define USE_W3MMAILER 1
#define USE_NNTP 1
#define USE_GOPHER 1
#define USE_ALARM 1
// #define USE_IMAGE 1
/* #undef USE_W3MIMG_X11 */
/* #undef USE_W3MIMG_FB */
/* #undef USE_W3MIMG_WIN */
/* #undef W3MIMGDISPLAY_SETUID */
/* #undef USE_IMLIB */
/* #undef USE_GDKPIXBUF */
/* #undef USE_GTK2 */
#define X_DISPLAY_MISSING 1
/* #undef USE_IMLIB2 */
// #define USE_XFACE 1
#define USE_DICT 1
#define USE_HISTORY 1
/* #undef FORMAT_NICE */
#define ID_EXT
/* #undef CLEAR_BUF */
#define INET6 1
#define HAVE_SOCKLEN_T 1
/* #undef HAVE_OLD_SS_FAMILY */
/* #undef USE_EGD */
#define ENABLE_REMOVE_TRAILINGSPACES
/* #undef MENU_THIN_FRAME */
/* #undef USE_RAW_SCROLL */
/* #undef TABLE_EXPAND */
/* #undef TABLE_NO_COMPACT */
#define NOWRAP
#define MATRIX

#define DEF_EDITOR "/usr/bin/vi"
#define DEF_MAILER "/usr/bin/mail"
#define DEF_EXT_BROWSER "/usr/bin/firefox"

/* fallback viewer. mailcap override these configuration */
#define DEF_IMAGE_VIEWER	"display"
#define DEF_AUDIO_PLAYER	"showaudio"

/* for USE_MIGEMO */
#define DEF_MIGEMO_COMMAND ""

/* #undef USE_BINMODE_STREAM */
#define HAVE_TERMIOS_H 1
/* #undef HAVE_TERMIO_H */
/* #undef HAVE_SGTTY_H */
#define HAVE_DIRENT_H 1
#define HAVE_LOCALE_H 1
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define SIZEOF_LONG_LONG 8

#define HAVE_STRTOLL 1
/* #undef HAVE_STROQ */
#define HAVE_ATOLL 1
/* #undef HAVE_ATOQ */
#define HAVE_STRCASECMP 1
#define HAVE_STRCASESTR 1
#define HAVE_STRCHR 1
#define HAVE_STRERROR 1
#define HAVE_BCOPY 1
#define HAVE_WAITPID 1
#define HAVE_WAIT3 1
#define HAVE_STRFTIME 1
#define HAVE_GETCWD 1
#define HAVE_GETWD 1
#define HAVE_SYMLINK 1
#define HAVE_READLINK 1
#define HAVE_LSTAT 1
#define HAVE_SETENV 1
#define HAVE_PUTENV 1
#define HAVE_SRAND48 1
#define HAVE_SRANDOM 1
/* #undef HAVE_GETPASSPHRASE */
#define HAVE_CHDIR 1
#define HAVE_MKDTEMP 1
#define HAVE_FACCESSAT 1
#define HAVE_SETPGRP 1
#define HAVE_SETLOCALE 1
#define HAVE_LANGINFO_CODESET 1

#define HAVE_FLOAT_H 1
#define HAVE_SYS_SELECT_H 1

#ifndef HAVE_SRAND48
#ifdef HAVE_SRANDOM
#define srand48 srandom
#define lrand48 random
#else /* HAVE_SRANDOM */
#define USE_INCLUDED_SRAND48
#endif /* HAVE_SRANDOM */
#endif

#ifndef HAVE_LSTAT
#define lstat stat
#endif

#define DEFAULT_TERM	0	/* XXX */

#define GUNZIP_CMDNAME  "gunzip"
#define BUNZIP2_CMDNAME "bunzip2"
#define INFLATE_CMDNAME	"inflate"
#define W3MBOOKMARK_CMDNAME	"w3mbookmark"
#define W3MHELPERPANEL_CMDNAME	"w3mhelperpanel"
#define DEV_NULL_PATH	"/dev/null"
#define DEV_TTY_PATH	"/dev/tty"
#define CGI_EXTENSION	".cgi"

#define BROTLI_CMDNAME "brotli"

#define PATH_SEPARATOR	':'
#define GUNZIP_NAME  "gunzip"
#define BUNZIP2_NAME "bunzip2"
#define INFLATE_NAME "inflate"
#define BROTLI_NAME "brotli"


#endif /* CONFIG_H_SEEN */
