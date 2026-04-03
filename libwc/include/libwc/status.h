#pragma once
#include "wc_types.h"
#include "ces.h"

typedef struct wc_wchar (*WcConv)(wc_ccs, wc_uint16);

struct wc_table {
    wc_ccs ccs;
    size_t n;
    struct wc_map* map;
    WcConv conv;
};

struct wc_status {
    struct wc_ces_info* ces_info;
    wc_uint8 gr;
    wc_uint8 gl;
    wc_uint8 ss;
    wc_ccs g0_ccs;
    wc_ccs g1_ccs;
    wc_ccs design[4];
    struct wc_table** tlist;
    struct wc_table** tlistw;
    int state;
    struct wc_output tag;
    int ntag;
    wc_uint32 base;
    int shift;
};

void wc_input_init(wc_ces ces, struct wc_status* st);
wc_bool wc_ces_has_ccs(wc_ccs ccs, struct wc_status* st);
