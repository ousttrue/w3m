#ifndef W3M_REGEX_H
#define W3M_REGEX_H

#define REGEX_MAX	64
#define STORAGE_MAX	256

#include "config.h"
#include "libwc/ccs.h"

typedef struct {
    char type;
    wc_wchar_t wch;
    unsigned char ch;
} longchar;

typedef struct regexchar {
    union {
	longchar *pattern;
	struct regex *sub;
    } p;
    unsigned char mode;
} regexchar;


typedef struct regex {
    regexchar re[REGEX_MAX];
    longchar storage[STORAGE_MAX];
    const char *position;
    const char *lposition;
    struct regex *alt_regex;
} Regex;


Regex *newRegex(const char *ex, int igncase, Regex *regex, const char **error_msg);

int RegexMatch(Regex *re, const char *str, int len, int firstp);

void MatchedPosition(Regex *re, const char **first, const char **last);


/* backward compatibility */
const char *regexCompile(const char *ex, int igncase);

int regexMatch(const char *str, int len, int firstp);

void matchedPosition(const char **first, const char **last);

#endif
