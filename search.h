#ifndef W3M_SEARCH_H
#define W3M_SEARCH_H

#include "buffer.h"
#include "config.h"

/* Search Result */
#define SR_FOUND       0x1
#define SR_NOTFOUND    0x2
#define SR_WRAPPED     0x4

extern const char *SearchString;

int backwardSearch(Buffer *buf, const char *str);
int forwardSearch(Buffer *buf, const char *str);

#ifdef USE_M17N
#include "libwc/wc_types.h"
const char *conv_search_string(const char *str, wc_ces f_ces);
#else
#define conv_search_string(str, f_ces)	str
#endif

#ifdef USE_MIGEMO
void init_migemo(void);
#endif

#endif
