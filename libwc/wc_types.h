#pragma once
#include "../Str.h"
#include "../config.h"
#include <stdint.h>

typedef uint32_t wc_ccs;
typedef uint32_t wc_ces;
typedef uint32_t wc_locale;

typedef struct wc_status wc_status;

typedef struct {
    wc_ccs ccs;
    uint32_t code;
} wc_wchar_t;

typedef struct {
    uint16_t code;
    uint16_t code2;
} wc_map;

typedef struct {
    uint16_t code;
    uint16_t code2;
    uint16_t code3;
} wc_map3;

typedef struct {
    wc_ccs       ccs;
    size_t       n;
    wc_map      *map;
    wc_wchar_t (*conv)(wc_ccs, uint16_t);
} wc_table;

typedef struct {
    wc_ccs   ccs;
    uint8_t g;
    bool  init;
} wc_gset;

typedef struct {
    wc_ces    id;
    char     *name;
    char     *desc;
    wc_gset  *gset;
    uint8_t *gset_ext;
    pStr     (*conv_from)(pStr, wc_ces);
    void    (*push_to)(pStr, wc_wchar_t, wc_status *);
    pStr     (*char_conv)(uint8_t, wc_status *);
} wc_ces_info;

typedef struct {
    wc_ces   id;
    char    *name;
    char    *desc;
} wc_ces_list;

typedef struct {
    uint8_t auto_detect;	/* automatically charset detection */
    bool use_combining;	/* use combining characters */
    bool use_language_tag;	/* use language_tags */
    bool ucs_conv;		/* charset conversion using Unicode */
    bool pre_conv;		/* previously charset conversion */
    bool fix_width_conv;	/* not allowed conversion between different
				   width charsets */
    bool use_gb12345_map;	/* use GB 12345 Unicode map instead of
				   GB 2312 Unicode map */
    bool use_jisx0201;	/* use JIS X 0201 Roman instead of US_ASCII */
    bool use_jisc6226;	/* use JIS C 6226:1978 instead of JIS X 0208 */
    bool use_jisx0201k;	/* use JIS X 0201 Katakana */
    bool use_jisx0212;	/* use JIS X 0212 */
    bool use_jisx0213;	/* use JIS X 0213 */
    bool strict_iso2022;	/* strict ISO 2022 */
    bool gb18030_as_ucs;	/* treat 4 bytes char. of GB18030 as Unicode */
    bool no_replace;		/* don't output replace character */
    bool use_wide;		/* use wide characters */
    bool east_asian_width;	/* East Asian Ambiguous characters are wide */
} wc_option;

typedef struct wc_status {
    wc_ces_info *ces_info;
    uint8_t     gr;
    uint8_t     gl;
    uint8_t     ss;
    wc_ccs       g0_ccs;
    wc_ccs       g1_ccs;
    wc_ccs       design[4];
    wc_table   **tlist;
    wc_table   **tlistw;
    int          state;
    pStr          tag;
    int          ntag;
    uint32_t    base;
    int          shift;
} wc_status;

