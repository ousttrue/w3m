#pragma once
#include <stdint.h>
#include <stddef.h>

#define WC_FALSE 0
#define WC_TRUE 1

#define WC_OPT_DETECT_OFF 0
#define WC_OPT_DETECT_ISO_2022 1
#define WC_OPT_DETECT_ON 2

#define WC_LOCALE_JA_JP 1
#define WC_LOCALE_ZH_CN 2
#define WC_LOCALE_ZH_TW 3
#define WC_LOCALE_ZH_HK 4
#define WC_LOCALE_KO_KR 5

typedef unsigned char wc_uchar;
typedef uint8_t wc_uint8;
typedef uint16_t wc_uint16;
typedef uint32_t wc_uint32;
typedef wc_uint32 wc_ccs;
typedef wc_uint32 wc_ces;

typedef wc_uint32 wc_locale;
typedef wc_uchar wc_bool;

struct wc_wchar {
    wc_ccs ccs;
    wc_uint32 code;
};

struct wc_option {
    wc_uint8 auto_detect; /* automatically charset detection */
    wc_bool use_combining; /* use combining characters */
    wc_bool use_language_tag; /* use language_tags */
    wc_bool ucs_conv; /* charset conversion using Unicode */
    wc_bool pre_conv; /* previously charset conversion */
    wc_bool fix_width_conv; /* not allowed conversion between different
                               width charsets */
    wc_bool use_gb12345_map; /* use GB 12345 Unicode map instead of
                                GB 2312 Unicode map */
    wc_bool use_jisx0201; /* use JIS X 0201 Roman instead of US_ASCII */
    wc_bool use_jisc6226; /* use JIS C 6226:1978 instead of JIS X 0208 */
    wc_bool use_jisx0201k; /* use JIS X 0201 Katakana */
    wc_bool use_jisx0212; /* use JIS X 0212 */
    wc_bool use_jisx0213; /* use JIS X 0213 */
    wc_bool strict_iso2022; /* strict ISO 2022 */
    wc_bool gb18030_as_ucs; /* treat 4 bytes char. of GB18030 as Unicode */
    wc_bool no_replace; /* don't output replace character */
    wc_bool use_wide; /* use wide characters */
    wc_bool east_asian_width; /* East Asian Ambiguous characters are wide */
};

typedef char* (*wc_output_getPtr)(void* data);
typedef int (*wc_output_getLen)(void* data);
typedef void (*wc_output_putc)(void* data, uint8_t ch);
typedef void (*wc_output_clear)(void* data);
typedef void (*wc_output_free)(void* data);

struct wc_output {
    void* data;
    wc_output_getPtr getPtr;
    wc_output_getLen getLen;
    wc_output_putc putc;
    wc_output_clear clear;
    wc_output_free free;
};

struct wc_span {
    uint8_t* ptr;
    size_t len;
};
