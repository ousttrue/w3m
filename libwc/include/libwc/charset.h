#pragma once
#include "ces.h"

struct wc_ces_list* wc_get_ces_list(void);
wc_ces wc_guess_charset(const char* charset, wc_ces orig);
wc_ces wc_guess_charset_short(const char* charset, wc_ces orig);
wc_ces wc_guess_locale_charset(const char* locale, wc_ces orig);
wc_ces wc_charset_to_ces(const char* charset);
wc_ces wc_charset_short_to_ces(const char* charset);
wc_ces wc_locale_to_ces(const char* locale);
wc_ces wc_guess_8bit_charset(wc_ces orig);
const char* wc_ces_to_charset(wc_ces ces);
const char* wc_ces_to_charset_desc(wc_ces ces);
wc_bool wc_check_ces(wc_ces ces);
