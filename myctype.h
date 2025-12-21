#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

enum MYCTYPE_TYPES : uint8_t {
    MYCTYPE_CNTRL = 1,
    MYCTYPE_SPACE = 2,
    MYCTYPE_ALPHA = 4,
    MYCTYPE_DIGIT = 8,
    MYCTYPE_PRINT = 16,
    MYCTYPE_HEX = 32,
    MYCTYPE_INTSPACE = 64,
    MYCTYPE_ASCII = (MYCTYPE_CNTRL | MYCTYPE_PRINT),
    MYCTYPE_ALNUM = (MYCTYPE_ALPHA | MYCTYPE_DIGIT),
    MYCTYPE_XDIGIT = (MYCTYPE_HEX | MYCTYPE_DIGIT),
};

enum MYCTYPE_TYPES GET_MYCTYPE(uint8_t x);
uint8_t GET_MYCDIGIT(uint8_t x);

inline static bool IS_CNTRL(uint8_t x) { return (GET_MYCTYPE(x) & MYCTYPE_CNTRL); }
inline static bool IS_SPACE(uint8_t x) { return (GET_MYCTYPE(x) & MYCTYPE_SPACE); }
inline static bool IS_ALPHA(uint8_t x) { return (GET_MYCTYPE(x) & MYCTYPE_ALPHA); }
inline static bool IS_DIGIT(uint8_t x) { return (GET_MYCTYPE(x) & MYCTYPE_DIGIT); }
inline static bool IS_PRINT(uint8_t x) { return (GET_MYCTYPE(x) & MYCTYPE_PRINT); }
inline static bool IS_ASCII(uint8_t x) { return (GET_MYCTYPE(x) & MYCTYPE_ASCII); }
inline static bool IS_ALNUM(uint8_t x) { return (GET_MYCTYPE(x) & MYCTYPE_ALNUM); }
inline static bool IS_XDIGIT(uint8_t x) { return (GET_MYCTYPE(x) & MYCTYPE_XDIGIT); }
inline static bool IS_INTSPACE(uint8_t x) { return (GET_MYCTYPE((unsigned char)(x)) & MYCTYPE_INTSPACE); }

inline static uint8_t TOLOWER(uint8_t x) { return (IS_ALPHA(x) ? ((x) | 0x20) : (x)); }
inline static uint8_t TOUPPER(uint8_t x) { return (IS_ALPHA(x) ? ((x) & ~0x20) : (x)); }

#define SKIP_BLANKS(p)                 \
    {                                  \
        while (*(p) && IS_SPACE(*(p))) \
            (p)++;                     \
    }
#define SKIP_NON_BLANKS(p)              \
    {                                   \
        while (*(p) && !IS_SPACE(*(p))) \
            (p)++;                      \
    }

inline static bool IS_ENDL(uint8_t c) { return ((c) == '\0' || (c) == '\r' || (c) == '\n'); }
inline static bool IS_ENDT(uint8_t c) { return (IS_ENDL(c) || (c) == ';'); }

int str_to_bool(char* value, int old);

#define SP_NORMAL 0
#define SP_PREC 1
#define SP_PREC2 2
int vscpf(const char* fmt, va_list ap);
