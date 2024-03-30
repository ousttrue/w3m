#pragma once
#include <string>
#include <string_view>
#include <list>

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
void make_domain_list(std::list<std::string> &list,
                      const std::string &domain_list);
std::string cleanupName(std::string_view name);

enum CleanupMode {
  RAW_MODE = 0,
  PAGER_MODE = 1,
  HTML_MODE = 2,
  HEADER_MODE = 3,
};
std::string cleanup_line(std::string_view s, CleanupMode mode);

std::string expandPath(std::string_view name);

unsigned int total_dot_number(std::string_view v, unsigned int max_count);
inline bool contain_no_dots(std::string_view v) {
  return total_dot_number(v, 1) == 0;
};

std::string html_quote(std::string_view str);
std::string html_unquote(std::string_view str);

std::string qstr_unquote(std::string_view s);

std::string mybasename(std::string_view s);

int stoi(std::string_view s);

std::string shell_quote(std::string_view str);

std::string unescape_spaces(std::string_view s);

std::string_view Strremovefirstspaces(std::string_view s);
std::string_view Strremovetrailingspaces(std::string_view s);
void Strshrink(std::string &s, int n);
