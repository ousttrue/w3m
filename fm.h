/* vi: set sw=4 ts=8 ai sm noet : */
/* 
 * w3m: WWW wo Miru utility
 * 
 * by A.ITO  Feb. 1995
 * 
 * You can use,copy,modify and distribute this program without any permission.
 */

#ifndef W3M_FM_H
#define W3M_FM_H

#include "config.h"	/* At top for defines below */

#ifndef USE_COLOR
#undef USE_ANSI_COLOR
#undef USE_BG_COLOR
#endif

#include "Str.h"

#include <gc/gc.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef USE_M17N
#include "libwc/wc.h"
#include "libwc/wtf.h"
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) gettext (String)
#define N_(String) (String)
#else
# define _(Text) Text
# define N_(Text) Text
#endif

#ifdef MAINPROGRAM
#define global
#define init(x) =(x)
#else				/* not MAINPROGRAM */
#define global extern
#define init(x)
#endif				/* not MAINPROGRAM */

#define DEFUN(funcname, macroname, docstring) void funcname(void)

/* 
 * Constants.
 */

#ifdef USE_IMAGE
#define MAX_IMAGE 1000
#define MAX_IMAGE_SIZE 2048
#endif

#define SHELLBUFFERNAME	"*Shellout*"
#define PIPEBUFFERNAME	"*stream*"
#define CPIPEBUFFERNAME	"*stream(closed)*"
#ifdef USE_DICT
#define DICTBUFFERNAME "*dictionary*"
#endif				/* USE_DICT */

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

#ifdef MAINPROGRAM
int REV_LB[MAX_LB] = {
    LB_N_FRAME, LB_FRAME, LB_N_INFO, LB_INFO, LB_N_SOURCE,
};
#else				/* not MAINPROGRAM */
extern int REV_LB[];
#endif				/* not MAINPROGRAM */

/* 
 * Macros.
 */

#ifdef __EMX__
#define strcasecmp	stricmp
#define strncasecmp	strnicmp
#endif				/* __EMX__ */


#define IS_ENDL(c) ((c)=='\0'||(c)=='\r'||(c)=='\n')
#define IS_ENDT(c) (IS_ENDL(c)||(c)==';')

#define bpcmp(a,b) \
  (((a).line - (b).line) ? ((a).line - (b).line) : ((a).pos - (b).pos))

#define RELATIVE_WIDTH(w)   (((w)>=0)?(int)((w)/pixel_per_char):(w))
#define REAL_WIDTH(w,limit) (((w)>=0)?(int)((w)/pixel_per_char):-(w)*(limit)/100)

#define EOL(l) (&(l)->ptr[(l)->length])
#define IS_EOL(p,l) ((p)==&(l)->ptr[(l)->length])

/* 
 * Types.
 */


#define COPY_BUFROOT(dstbuf, srcbuf) {\
 (dstbuf)->rootX = (srcbuf)->rootX; \
 (dstbuf)->rootY = (srcbuf)->rootY; \
 (dstbuf)->COLS = (srcbuf)->COLS; \
 (dstbuf)->LINES = (srcbuf)->LINES; \
}

#define COPY_BUFPOSITION(dstbuf, srcbuf) {\
 (dstbuf)->topLine = (srcbuf)->topLine; \
 (dstbuf)->currentLine = (srcbuf)->currentLine; \
 (dstbuf)->pos = (srcbuf)->pos; \
 (dstbuf)->cursorX = (srcbuf)->cursorX; \
 (dstbuf)->cursorY = (srcbuf)->cursorY; \
 (dstbuf)->visualpos = (srcbuf)->visualpos; \
 (dstbuf)->currentColumn = (srcbuf)->currentColumn; \
}
#define SAVE_BUFPOSITION(sbufp) COPY_BUFPOSITION(sbufp, Currentbuf)
#define RESTORE_BUFPOSITION(sbufp) COPY_BUFPOSITION(Currentbuf, sbufp)
#define TOP_LINENUMBER(buf) ((buf)->topLine ? (buf)->topLine->linenumber : 1)
#define CUR_LINENUMBER(buf) ((buf)->currentLine ? (buf)->currentLine->linenumber : 1)

#define NO_BUFFER ((Buffer*)1)

/* 
 * Globals.
 */


#if defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE)
global char *MyProgramName init("w3m");
#endif /* defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE) */
#ifdef USE_IMAGE
global int activeImage init(false);
#endif


global int is_redisplay init(false);
#ifdef USE_ALARM
enum {
    AL_UNSET,
    AL_EXPLICIT,
    AL_IMPLICIT,
    AL_IMPLICIT_ONCE,
};

typedef struct _AlarmEvent {
    int sec;
    short status;
    int cmd;
    void *data;
} AlarmEvent;
#endif

/* 
 * Externals
 */

#endif				/* not W3M_FM_H */
