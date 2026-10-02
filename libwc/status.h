#pragma once
#include "../Str.h"
#include "ces.h"

enum WC_OPT_DETECT {
    WC_OPT_DETECT_OFF = 0,
    WC_OPT_DETECT_ISO_2022 = 1,
    WC_OPT_DETECT_ON = 2,
};

struct wc_option {
    enum WC_OPT_DETECT auto_detect; /* automatically charset detection */
    bool use_combining; /* use combining characters */
    bool use_language_tag; /* use language_tags */
    bool ucs_conv; /* charset conversion using Unicode */
    bool pre_conv; /* previously charset conversion */
    bool fix_width_conv; /* not allowed conversion between different
                            width charsets */
    bool use_gb12345_map; /* use GB 12345 Unicode map instead of
                             GB 2312 Unicode map */
    bool use_jisx0201; /* use JIS X 0201 Roman instead of US_ASCII */
    bool use_jisc6226; /* use JIS C 6226:1978 instead of JIS X 0208 */
    bool use_jisx0201k; /* use JIS X 0201 Katakana */
    bool use_jisx0212; /* use JIS X 0212 */
    bool use_jisx0213; /* use JIS X 0213 */
    bool strict_iso2022; /* strict ISO 2022 */
    bool gb18030_as_ucs; /* treat 4 bytes char. of GB18030 as Unicode */
    bool no_replace; /* don't output replace character */
    bool use_wide; /* use wide characters */
    bool east_asian_width; /* East Asian Ambiguous characters are wide */
};

extern struct wc_option WcOption;

struct wc_status {
    struct wc_ces_info* ces_info;
    uint8_t gr;
    uint8_t gl;
    uint8_t ss;
    wc_ccs g0_ccs;
    wc_ccs g1_ccs;
    wc_ccs design[4];
    wc_table** tlist;
    wc_table** tlistw;
    int state;
    pStr tag;
    int ntag;
    uint32_t base;
    int shift;
};

extern void wc_input_init(wc_ces ces, struct wc_status* st);
extern void wc_output_init(wc_ces ces, struct wc_status* st);
extern bool wc_ces_has_ccs(wc_ccs ccs, struct wc_status* st);
