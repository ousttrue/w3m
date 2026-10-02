#pragma once
#include "../Str.h"
#include "../config.h"
#include <stdint.h>

typedef uint32_t wc_ccs;
typedef uint32_t wc_ces;
typedef uint32_t wc_locale;

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

typedef struct wc_status wc_status;
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


