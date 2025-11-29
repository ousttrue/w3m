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
#define HELP_CGI "w3mhelp"
#define IMGDISPLAY "w3mimgdisplay"
#define XFACE2XPM "xface2xpm"

#define BOOKMARK "bookmark.html"
#define MOUSE_FILE "mouse"
#define COOKIE_FILE "cookie"
#define HISTORY_FILE "history"

#define USER_URIMETHODMAP RC_DIR "/urimethodmap"
#define SYS_URIMETHODMAP CONF_DIR "/urimethodmap"

/* User Configuration */
#define USE_M17N 1
#define USE_UNICODE 1
#define W3M_LANG EN
#define LANG W3M_LANG

/* Define to 1 if translation of program messages to the user's
   native language is requested. */
#define ENABLE_NLS 1

#define USE_COLOR 1
#define USE_ANSI_COLOR 1
#define USE_BG_COLOR 1
/* #undef USE_MIGEMO */
#define USE_MARK
#define USE_MOUSE 1
#define USE_GPM 1
/* #undef USE_SYSMOUSE */
#define USE_MENU 1
#define USE_COOKIE 1
#define USE_DIGEST_AUTH 1
#define USE_SSL 1
#define USE_SSL_VERIFY 1
#define USE_HELP_CGI 1
#define USE_EXTERNAL_URI_LOADER 1
#define USE_W3MMAILER 1
#define USE_NNTP 1
#define USE_GOPHER 1
#define USE_ALARM 1
#define USE_IMAGE 1
#define USE_W3MIMG_X11 1
#define USE_W3MIMG_FB 1
/* #undef USE_W3MIMG_WIN */
/* #undef W3MIMGDISPLAY_SETUID */
/* #undef USE_IMLIB */
/* #undef USE_GDKPIXBUF */
/* #undef USE_GTK2 */
/* #undef X_DISPLAY_MISSING */
#define USE_IMLIB2 1
#define USE_XFACE 1
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

#define HAVE_SIGSETJMP 1

#ifdef HAVE_SIGSETJMP
#define SETJMP(env) sigsetjmp(env, 1)
#define LONGJMP(env, val) siglongjmp(env, val)
#define JMP_BUF sigjmp_buf
#else
#define SETJMP(env) setjmp(env)
#define LONGJMP(env, val) longjmp(env, val)
#define JMP_BUF jmp_buf
#endif

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

#define GUNZIP_CMDNAME "gunzip"
#define BUNZIP2_CMDNAME "bunzip2"
#define INFLATE_CMDNAME "inflate"
#define W3MBOOKMARK_CMDNAME "w3mbookmark"
#define W3MHELPERPANEL_CMDNAME "w3mhelperpanel"
#define DEV_TTY_PATH "/dev/tty"
#define BROTLI_CMDNAME "brotli"

#define PATH_SEPARATOR ':'
#define GUNZIP_NAME "gunzip"
#define BUNZIP2_NAME "bunzip2"
#define INFLATE_NAME "inflate"
#define BROTLI_NAME "brotli"


#endif /* CONFIG_H_SEEN */
