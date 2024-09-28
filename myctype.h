#pragma once

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

#define GET_MYCTYPE(x) (MYCTYPE_MAP[(int)(unsigned char)(x)])
#define GET_MYCDIGIT(x) (MYCTYPE_DIGITMAP[(int)(unsigned char)(x)])

#define IS_CNTRL(x) (GET_MYCTYPE(x) & MYCTYPE_CNTRL)
#define IS_SPACE(x) (GET_MYCTYPE(x) & MYCTYPE_SPACE)
#define IS_ALPHA(x) (GET_MYCTYPE(x) & MYCTYPE_ALPHA)
#define IS_DIGIT(x) (GET_MYCTYPE(x) & MYCTYPE_DIGIT)
#define IS_PRINT(x) (GET_MYCTYPE(x) & MYCTYPE_PRINT)
#define IS_ASCII(x) (GET_MYCTYPE(x) & MYCTYPE_ASCII)
#define IS_ALNUM(x) (GET_MYCTYPE(x) & MYCTYPE_ALNUM)
#define IS_XDIGIT(x) (GET_MYCTYPE(x) & MYCTYPE_XDIGIT)
#define IS_INTSPACE(x) (MYCTYPE_MAP[(unsigned char)(x)] & MYCTYPE_INTSPACE)

extern unsigned char MYCTYPE_MAP[];
extern unsigned char MYCTYPE_DIGITMAP[];

#define TOLOWER(x) (IS_ALPHA(x) ? ((x) | 0x20) : (x))
#define TOUPPER(x) (IS_ALPHA(x) ? ((x) & ~0x20) : (x))

#define SKIP_BLANKS(p)                                                         \
  {                                                                            \
    while (*(p) && IS_SPACE(*(p)))                                             \
      (p)++;                                                                   \
  }
#define SKIP_NON_BLANKS(p)                                                     \
  {                                                                            \
    while (*(p) && !IS_SPACE(*(p)))                                            \
      (p)++;                                                                   \
  }
#define IS_ENDL(c) ((c) == '\0' || (c) == '\r' || (c) == '\n')
#define IS_ENDT(c) (IS_ENDL(c) || (c) == ';')

extern unsigned char QUOTE_MAP[];
extern char *HTML_QUOTE_MAP[];
#define HTML_QUOTE_MASK 0x07   /* &, <, >, ", ' */
#define SHELL_UNSAFE_MASK 0x08 /* [^A-Za-z0-9_./:\200-\377] */
#define URL_QUOTE_MASK 0x10    /* [\0- \177-\377] */
#define FILE_QUOTE_MASK 0x30   /* [\0- #%&+:?\177-\377] */
#define URL_UNSAFE_MASK 0x70   /* [^A-Za-z0-9_$\-.] */
#define GET_QUOTE_TYPE(c) QUOTE_MAP[(int)(unsigned char)(c)]
#define is_html_quote(c) (GET_QUOTE_TYPE(c) & HTML_QUOTE_MASK)
#define is_shell_unsafe(c) (GET_QUOTE_TYPE(c) & SHELL_UNSAFE_MASK)
#define is_url_quote(c) (GET_QUOTE_TYPE(c) & URL_QUOTE_MASK)
#define is_file_quote(c) (GET_QUOTE_TYPE(c) & FILE_QUOTE_MASK)
#define is_url_unsafe(c) (GET_QUOTE_TYPE(c) & URL_UNSAFE_MASK)
#define html_quote_char(c) HTML_QUOTE_MAP[(int)is_html_quote(c)]


