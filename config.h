#pragma once
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
#define HAVE_CHDIR 1
#define HAVE_MKDTEMP 1
#define HAVE_SETPGRP 1
#define HAVE_SETLOCALE 1
#define HAVE_LANGINFO_CODESET 1
#define HAVE_FLOAT_H 1
#define HAVE_SYS_SELECT_H 1
#ifndef HAVE_LSTAT
#define lstat stat
#endif
