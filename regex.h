#pragma once
#include <libwc/wc_types.h>
#define REGEX_MAX 64
#define STORAGE_MAX 256

struct longchar {
    char type;
    wc_wchar_t wch;
    unsigned char ch;
};

enum RegexMode : uint8_t {
    RE_MATCHMODE = 0x07,
    RE_NORMAL = 0x00,
    RE_ANY = 0x01,
    RE_WHICH = 0x02,
    RE_EXCEPT = 0x03,
    RE_SUBREGEX = 0x04,
    RE_BEGIN = 0x05,
    RE_END = 0x06,
    RE_ENDMARK = 0x07,

    RE_OPT = 0x08,
    RE_ANYTIME = 0x10,
    RE_IGNCASE = 0x40,
};

struct regexchar {
    union {
        struct longchar* pattern;
        struct Regex* sub;
    } p;
    enum RegexMode mode;
};

struct Regex {
    struct regexchar re[REGEX_MAX];
    struct longchar storage[STORAGE_MAX];
    const char* position;
    const char* lposition;
    struct Regex* alt_regex;
};

struct Regex* newRegex(const char* ex, int igncase, struct Regex* regex, const char** error_msg);

int RegexMatch(struct Regex* re, const char* str, int len, int firstp);

void MatchedPosition(struct Regex* re, const char** first, const char** last);

/* backward compatibility */
const char* regexCompile(const char* ex, int igncase);

int regexMatch(const char* str, int len, int firstp);

void matchedPosition(const char** first, const char** last);
