#pragma once

extern const char *HTML_QUOTE_MAP[];

bool is_html_quote(char c);

#define html_quote_char(c) HTML_QUOTE_MAP[(int)is_html_quote(c)]

const char *html_quote(const char *str);
