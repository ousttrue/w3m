/* vi: set sw=4 ts=8 ai sm noet : */
#ifndef W3M_UTIL_H
#define W3M_UTIL_H

#include "config.h"

int exec_cmd(char *cmd);

#if defined(USE_M17N) && defined(USE_UNICODE)
#include "wc_types.h"
#define nextChar(s, l)	do { (s)++; } while ((s) < (l)->len && (l)->propBuf[s] & PC_WCHAR2)
#define prevChar(s, l)	do { (s)--; } while ((s) > 0 && (l)->propBuf[s] & PC_WCHAR2)
wc_uint32 getChar(const char *p);
int is_wordchar(wc_uint32 c);

#else		/* USE_M17N && USE_UNICODE */
#define nextChar(s, l)	(s)++
#define prevChar(s, l)	(s)--
#define getChar(p)	((int)*(p))
int is_wordchar(int c);

#endif		/* USE_M17N && USE_UNICODE */

#endif
