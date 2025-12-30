#pragma once
#include "ces.h"
#include "Str.h"

enum WC_OPT_DETECT_MODE : wc_uint8 {
    WC_OPT_DETECT_OFF = 0,
    WC_OPT_DETECT_ISO_2022 = 1,
    WC_OPT_DETECT_ON = 2,
};

struct wc_option {
    // automatically charset detection
    enum WC_OPT_DETECT_MODE auto_detect;
    // use combining characters
    wc_bool use_combining;
    // use language_tags
    wc_bool use_language_tag;
    // charset conversion using Unicode
    wc_bool ucs_conv;
    // previously charset conversion
    wc_bool pre_conv;
    // not allowed conversion between different width charsets
    wc_bool fix_width_conv;
    // use GB 12345 Unicode map instead of GB 2312 Unicode map
    wc_bool use_gb12345_map;
    // use JIS X 0201 Roman instead of US_ASCII
    wc_bool use_jisx0201;
    // use JIS C 6226:1978 instead of JIS X 0208
    wc_bool use_jisc6226;
    // use JIS X 0201 Katakana
    wc_bool use_jisx0201k;
    // use JIS X 0212
    wc_bool use_jisx0212;
    // use JIS X 0213
    wc_bool use_jisx0213;
    // strict ISO 2022
    wc_bool strict_iso2022;
    // treat 4 bytes char. of GB18030 as Unicode
    wc_bool gb18030_as_ucs;
    // don't output replace character
    wc_bool no_replace;
    // use wide characters
    wc_bool use_wide;
    // East Asian Ambiguous characters are wide
    wc_bool east_asian_width;
};
extern struct wc_option WcOption;

struct wc_status {
    struct wc_ces_info* ces_info;
    wc_uint8 gr;
    wc_uint8 gl;
    wc_uint8 ss;
    wc_ccs g0_ccs;
    wc_ccs g1_ccs;
    wc_ccs design[4];
    wc_table** tlist;
    wc_table** tlistw;
    int state;
    Str tag;
    int ntag;
    wc_uint32 base;
    int shift;
};

extern void wc_input_init(struct wc_status* st, enum wc_ces ces);
extern void wc_output_init(struct wc_status* st, enum wc_ces ces);
extern void wc_push_end(struct wc_status* st, Str os);
extern wc_bool wc_ces_has_ccs(struct wc_status* st, wc_ccs ccs);
