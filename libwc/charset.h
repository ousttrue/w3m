#pragma once
#include "ces.h"

typedef uint32_t wc_locale;

extern wc_locale WcLocale;

typedef struct {
    wc_ces id;
    const char* name;
    const char* desc;
} wc_ces_list;

extern wc_ces wc_guess_charset(const char* charset, wc_ces orig);
extern wc_ces wc_guess_charset_short(const char* charset, wc_ces orig);
extern wc_ces wc_guess_locale_charset(char* locale, wc_ces orig);
extern wc_ces wc_charset_to_ces(const char* charset);
extern wc_ces wc_charset_short_to_ces(const char* charset);
extern wc_ces wc_locale_to_ces(char* locale);
extern char* wc_ces_to_charset(wc_ces ces);
extern char* wc_ces_to_charset_desc(wc_ces ces);
extern bool wc_check_ces(wc_ces ces);
extern wc_ces_list* wc_get_ces_list(void);
