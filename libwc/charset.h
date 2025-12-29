#pragma once
#include "wc_types.h"

#define WC_LOCALE_JA_JP 1
#define WC_LOCALE_ZH_CN 2
#define WC_LOCALE_ZH_TW 3
#define WC_LOCALE_ZH_HK 4
#define WC_LOCALE_KO_KR 5

extern wc_locale WcLocale;

extern wc_ces wc_guess_charset(const char* charset, wc_ces orig);
extern wc_ces wc_guess_charset_short(const char* charset, wc_ces orig);
extern wc_ces wc_guess_locale_charset(char* locale, wc_ces orig);
extern wc_ces wc_charset_to_ces(char* charset);
extern wc_ces wc_charset_short_to_ces(char* charset);
extern wc_ces wc_locale_to_ces(char* locale);
extern wc_ces wc_guess_8bit_charset(wc_ces orig);
extern char* wc_ces_to_charset(wc_ces ces);
extern char* wc_ces_to_charset_desc(wc_ces ces);
extern wc_bool wc_check_ces(wc_ces ces);
extern wc_ces_list* wc_get_ces_list(void);

