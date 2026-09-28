/* vi: set sw=4 ts=8 ai sm noet : */
#ifndef W3M_MYCTYPE_H
#define W3M_MYCTYPE_H
#include "config.h"

#define MYCTYPE_CNTRL 1
#define MYCTYPE_SPACE 2
#define MYCTYPE_ALPHA 4
#define MYCTYPE_DIGIT 8
#define MYCTYPE_PRINT 16
#define MYCTYPE_HEX   32
#define MYCTYPE_INTSPACE 64
#define MYCTYPE_ASCII (MYCTYPE_CNTRL|MYCTYPE_PRINT)
#define MYCTYPE_ALNUM (MYCTYPE_ALPHA|MYCTYPE_DIGIT)
#define MYCTYPE_XDIGIT (MYCTYPE_HEX|MYCTYPE_DIGIT)

#define GET_MYCTYPE(x) (MYCTYPE_MAP[(int)(unsigned char)(x)])
#define GET_MYCDIGIT(x) (MYCTYPE_DIGITMAP[(int)(unsigned char)(x)])

#define IS_CNTRL(x) (GET_MYCTYPE(x) & MYCTYPE_CNTRL)
#define IS_SPACE(x) (GET_MYCTYPE(x) & MYCTYPE_SPACE)
#define SKIP_BLANKS(p) do{while(*(p)&&IS_SPACE(*(p)))(p)++;}while(0)
#define IS_ALPHA(x) (GET_MYCTYPE(x) & MYCTYPE_ALPHA)
#define IS_DIGIT(x) (GET_MYCTYPE(x) & MYCTYPE_DIGIT)
#define IS_PRINT(x) (GET_MYCTYPE(x) & MYCTYPE_PRINT)
#define IS_ASCII(x) (GET_MYCTYPE(x) & MYCTYPE_ASCII)
#define IS_ALNUM(x) (GET_MYCTYPE(x) & MYCTYPE_ALNUM)
#define IS_XDIGIT(x) (GET_MYCTYPE(x) & MYCTYPE_XDIGIT)
#define IS_INTSPACE(x) (MYCTYPE_MAP[(unsigned char)(x)] & MYCTYPE_INTSPACE)

extern unsigned char MYCTYPE_MAP[];
extern unsigned char MYCTYPE_DIGITMAP[];

#define	TOLOWER(x)	(IS_ALPHA(x) ? ((x)|0x20) : (x))
#define	TOUPPER(x)	(IS_ALPHA(x) ? ((x)&~0x20) : (x))

#ifdef USE_M17N
#define get_mctype(c) ((Lineprop)wtf_type((const wc_uchar *)(c)) << 8)
#define get_mclen(c) wtf_len1((const wc_uchar *)(c))
#define get_mcwidth(c) wtf_width((const wc_uchar *)(c))
#define get_strwidth(c) wtf_strwidth((const wc_uchar *)(c))
#define get_Str_strwidth(c) wtf_strwidth((wc_uchar *)((c)->ptr))
#else
#define get_mctype(c) (IS_CNTRL(*(c)) ? PC_CTRL : PC_ASCII)
#define get_mclen(c) 1
#define get_mcwidth(c) 1
#define get_strwidth(c) strlen(c)
#define get_Str_strwidth(c) ((c)->length)
#endif
#endif
