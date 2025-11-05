#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MYCTYPE_CNTRL 1
#define MYCTYPE_SPACE 2
#define MYCTYPE_ALPHA 4
#define MYCTYPE_DIGIT 8
#define MYCTYPE_PRINT 16
#define MYCTYPE_HEX 32
#define MYCTYPE_INTSPACE 64
#define MYCTYPE_ASCII (MYCTYPE_CNTRL | MYCTYPE_PRINT)
#define MYCTYPE_ALNUM (MYCTYPE_ALPHA | MYCTYPE_DIGIT)
#define MYCTYPE_XDIGIT (MYCTYPE_HEX | MYCTYPE_DIGIT)

extern unsigned char MYCTYPE_MAP[];
extern unsigned char MYCTYPE_DIGITMAP[];

static inline unsigned char GET_MYCTYPE(uint8_t x) { return (MYCTYPE_MAP[(int)(unsigned char)(x)]); }
#define GET_MYCDIGIT(x) (MYCTYPE_DIGITMAP[(int)(unsigned char)(x)])

#define IS_CNTRL(x) (GET_MYCTYPE(x) & MYCTYPE_CNTRL)
static inline bool IS_SPACE(uint8_t x) { return (GET_MYCTYPE(x) & MYCTYPE_SPACE); }
#define IS_ALPHA(x) (GET_MYCTYPE(x) & MYCTYPE_ALPHA)
#define IS_DIGIT(x) (GET_MYCTYPE(x) & MYCTYPE_DIGIT)
#define IS_PRINT(x) (GET_MYCTYPE(x) & MYCTYPE_PRINT)
#define IS_ASCII(x) (GET_MYCTYPE(x) & MYCTYPE_ASCII)
#define IS_ALNUM(x) (GET_MYCTYPE(x) & MYCTYPE_ALNUM)
#define IS_XDIGIT(x) (GET_MYCTYPE(x) & MYCTYPE_XDIGIT)
#define IS_INTSPACE(x) (MYCTYPE_MAP[(unsigned char)(x)] & MYCTYPE_INTSPACE)

static inline uint8_t TOLOWER(uint8_t x) { return (IS_ALPHA(x) ? ((x) | 0x20) : (x)); }
static inline uint8_t TOUPPER(uint8_t x) { return (IS_ALPHA(x) ? ((x) & ~0x20) : (x)); }

static inline void SKIP_BLANKS(char** p)
{
    while (*(*p) && IS_SPACE(*(*p)))
        (*p)++;
}

static inline void SKIP_NON_BLANKS(char** p)
{
    while (*(*p) && !IS_SPACE(*(*p)))
        (*p)++;
}

#define IS_ENDL(c) ((c) == '\0' || (c) == '\r' || (c) == '\n')
#define IS_ENDT(c) (IS_ENDL(c) || (c) == ';')

bool non_null(const char* s);
