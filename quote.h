#pragma once
#include <stdint.h>
#include <stdbool.h>

enum QuoteMask {
    // &, <, >, ", ' */
    HTML_QUOTE_MASK = 0x07,
    // [^A-Za-z0-9_./:\200-\377] */
    SHELL_UNSAFE_MASK = 0x08,
    // [\0- \177-\377] */
    URL_QUOTE_MASK = 0x10,
    // [\0- #%&+:?\177-\377] */
    FILE_QUOTE_MASK = 0x30,
    // [^A-Za-z0-9_$\-.] */
    URL_UNSAFE_MASK = 0x70,
};

bool is_html_quote(int c);
bool is_shell_unsafe(int c);
bool is_url_quote(int c);
bool is_file_quote(int c);
bool is_url_unsafe(int c);

const char* html_quote_char(int c);
