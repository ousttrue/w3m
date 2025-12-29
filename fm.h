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
#include "input_stream.h"

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

extern unsigned char GlobalKeymap[];
extern unsigned char EscKeymap[];
extern unsigned char EscBKeymap[];
extern unsigned char EscDKeymap[];
extern struct FuncList w3mFuncList[];

#define DNS_ORDER_UNSPEC 0
#define DNS_ORDER_INET_INET6 1
#define DNS_ORDER_INET6_INET 2
#define DNS_ORDER_INET_ONLY 4
#define DNS_ORDER_INET6_ONLY 6
extern int ai_family_order_table[7][3]; /* XXX */

global struct TextList* NO_proxy_domains;

global char* CurrentDir;
global int CurrentPid;

#define NO_TABBUFFER ((struct TabBuffer*)1)
extern char* w3m_version;

#define DUMP_BUFFER 0x01
#define DUMP_HEAD 0x02
#define DUMP_SOURCE 0x04
#define DUMP_EXTRA 0x08
#define DUMP_HALFDUMP 0x10
#define DUMP_FRAME 0x20
global int w3m_debug;
#define w3m_halfdump (getRuntime()->w3m_dump & DUMP_HALFDUMP)

#define MAILTO_OPTIONS_IGNORE 1
#define MAILTO_OPTIONS_USE_MAILTO_URL 2
#define DISPLAY_INS_DEL_SIMPLE 0
#define DISPLAY_INS_DEL_NORMAL 1
#define DISPLAY_INS_DEL_FONTIFY 2
#define DEFAULT_URL_EMPTY 0
#define DEFAULT_URL_CURRENT 1
#define DEFAULT_URL_LINK 2

global struct TextList* fileToDelete;
global char* tmp_dir;

#define ACCEPT_BAD_COOKIE_DISCARD 0
#define ACCEPT_BAD_COOKIE_ACCEPT 1
#define ACCEPT_BAD_COOKIE_ASK 2

global struct TextList* Cookie_reject_domains;
global struct TextList* Cookie_accept_domains;
global struct TextList* Cookie_avoid_wrong_number_of_dots_domains;

global struct TextList* backend_batch_commands init(NULL);
global TextLineList* backend_halfdump_buf;
int backend(void);
extern void deleteFiles(void);

/*
 * Externals
 */

#include "proto.h"

#endif /* not FM_H */
