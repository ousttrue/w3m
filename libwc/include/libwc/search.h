#pragma once
#include "wc_types.h"

struct wc_map {
    wc_uint16 code;
    wc_uint16 code2;
};

struct wc_map3 {
    wc_uint16 code;
    wc_uint16 code2;
    wc_uint16 code3;
};

struct wc_map* wc_map_search(wc_uint16 code, struct wc_map* map, size_t n);
struct wc_map3* wc_map3_search(wc_uint16 c1, wc_uint16 c2, struct wc_map3* map, size_t n);
struct wc_map* wc_map_range_search(wc_uint16 code, struct wc_map* map, size_t n);
struct wc_map* wc_map2_range_search(wc_uint16 code, struct wc_map* map, size_t n);
struct wc_map3* wc_map3_range_search(wc_uint16 code, struct wc_map3* map, size_t n);
