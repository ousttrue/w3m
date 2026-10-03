#pragma once
#include <stdint.h>
#include <stddef.h>
// #include "search.h"

typedef uint32_t wc_ccs;

typedef struct {
    wc_ccs ccs;
    uint32_t code;
} wc_wchar_t;

typedef struct {
    wc_ccs ccs;
    size_t n;
    struct wc_map* map;
    wc_wchar_t (*conv)(wc_ccs, uint16_t);
} wc_table;

typedef struct {
    wc_ccs ccs;
    uint8_t g;
    bool init;
} wc_gset;
