/* vi: set sw=4 ts=8 ai sm noet : */

#ifndef _WC_TYPES_H
#define _WC_TYPES_H

#include "../Str.h"
#include "../config.h"
#include <stdint.h>

typedef unsigned char  wc_uchar;
typedef uint8_t wc_uint8;
typedef uint16_t wc_uint16;
typedef uint32_t wc_uint32;

typedef wc_uint32 wc_ccs;
typedef wc_uint32 wc_ces;
typedef wc_uint32 wc_locale;

typedef struct wc_status wc_status;

typedef struct {
    wc_ccs ccs;
    wc_uint32 code;
} wc_wchar_t;

typedef struct {
    wc_uint16 code;
    wc_uint16 code2;
} wc_map;

typedef struct {
    wc_uint16 code;
    wc_uint16 code2;
    wc_uint16 code3;
} wc_map3;

typedef struct {
    wc_ccs       ccs;
    size_t       n;
    wc_map      *map;
    wc_wchar_t (*conv)(wc_ccs, wc_uint16);
} wc_table;

typedef struct {
    wc_ccs   ccs;
    wc_uchar g;
    bool  init;
} wc_gset;

typedef struct {
    wc_ces    id;
    char     *name;
    char     *desc;
    wc_gset  *gset;
    wc_uchar *gset_ext;
    pStr     (*conv_from)(pStr, wc_ces);
    void    (*push_to)(pStr, wc_wchar_t, wc_status *);
    pStr     (*char_conv)(wc_uchar, wc_status *);
} wc_ces_info;

typedef struct {
    wc_ces   id;
    char    *name;
    char    *desc;
} wc_ces_list;

typedef struct {
    wc_uint8 auto_detect;	/* automatically charset detection */
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
    wc_uint8     gr;
    wc_uint8     gl;
    wc_uint8     ss;
    wc_ccs       g0_ccs;
    wc_ccs       g1_ccs;
    wc_ccs       design[4];
#ifdef USE_UNICODE
    wc_table   **tlist;
    wc_table   **tlistw;
#endif
    int          state;
#ifdef USE_UNICODE
    pStr          tag;
    int          ntag;
    wc_uint32    base;
    int          shift;
#endif
} wc_status;

#endif
