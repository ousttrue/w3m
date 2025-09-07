#pragma once

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

#define SETPGRP_VOID 1
#ifdef HAVE_SETPGRP
#ifdef SETPGRP_VOID
#define SETPGRP() setpgrp()
#else
#define SETPGRP() setpgrp(0, 0)
#endif
#else /* no HAVE_SETPGRP; OS/2 EMX */
#define SETPGRP() setpgid(0, 0)
#endif
#define HAVE_FLOAT_H 1
#define HAVE_SYS_SELECT_H 1

#define HAVE_SIGSETJMP 1

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

#define PATH_SEPARATOR ':'
