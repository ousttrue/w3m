#pragma once
#include <config.h>
#include <stdint.h>
#include <stddef.h>

#define WC_FALSE 0
#define WC_TRUE 1

typedef unsigned char wc_uchar;

typedef uint8_t wc_uint8;
typedef uint16_t wc_uint16;
typedef uint32_t wc_uint32;

typedef wc_uint32 wc_ccs;
typedef wc_uint32 wc_locale;
typedef wc_uchar wc_bool;

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
    wc_ccs ccs;
    size_t n;
    wc_map* map;
    wc_wchar_t (*conv)(wc_ccs, wc_uint16);
} wc_table;

typedef struct {
    wc_ccs ccs;
    wc_uchar g;
    wc_bool init;
} wc_gset;

typedef wc_uint32 wc_ces;

typedef struct {
    wc_ces id;
    char* name;
    char* desc;
} wc_ces_list;
