#pragma once
#include <string>
#include <string_view>
#include <list>

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define strcasecmp _stricmp

char *strcasestr(const char *str, const char *pattern);
#else
#include <string.h>
#endif

extern const char *HTML_QUOTE_MAP[];
extern unsigned char QUOTE_MAP[];
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

std::string_view remove_space(std::string_view str);
void make_domain_list(std::list<std::string> &list, const char *domain_list);
std::string cleanupName(std::string_view name);
