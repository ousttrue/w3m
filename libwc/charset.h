#pragma once
#include "ces.h"

#define WC_LOCALE_JA_JP 1
#define WC_LOCALE_ZH_CN 2
#define WC_LOCALE_ZH_TW 3
#define WC_LOCALE_ZH_HK 4
#define WC_LOCALE_KO_KR 5

extern wc_locale WcLocale;

extern enum wc_ces wc_guess_charset(const char* charset, enum wc_ces orig);
extern enum wc_ces wc_guess_charset_short(const char* charset, enum wc_ces orig);
extern enum wc_ces wc_guess_locale_charset(const char* locale, enum wc_ces orig);
extern enum wc_ces wc_charset_to_ces(const char* charset);
extern enum wc_ces wc_charset_short_to_ces(const char* charset);
extern enum wc_ces wc_locale_to_ces(const char* locale);
extern enum wc_ces wc_guess_8bit_charset(enum wc_ces orig);
extern char* wc_ces_to_charset(enum wc_ces ces);
extern char* wc_ces_to_charset_desc(enum wc_ces ces);
extern wc_bool wc_check_ces(enum wc_ces ces);
extern wc_ces_list* wc_get_ces_list(void);

