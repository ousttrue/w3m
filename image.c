/* $Id: image.c,v 1.37 2010/12/21 10:13:55 htrb Exp $ */

#include "fm.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#ifdef HAVE_WAITPID
#include <sys/wait.h>
#endif

