#ifndef W3M_SEARCH_H_
#define W3M_SEARCH_H_

#include "config.h"
#include "fm.h"

extern const char *SearchString;

int backwardSearch(Buffer *buf, const char *str);
int forwardSearch(Buffer *buf, const char *str);

#ifdef USE_M17N
#include "wc_types.h"
const char *conv_search_string(const char *str, wc_ces f_ces);
#else
#define conv_search_string(str, f_ces)	str
#endif

#ifdef USE_MIGEMO
void init_migemo(void);
#endif

#endif
