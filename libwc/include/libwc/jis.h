#pragma once
#include "wc_types.h"

extern wc_uchar* wc_jisx0212_jisx02132_map;

struct wc_wchar wc_jisx0201k_to_jisx0208(struct wc_wchar cc);
struct wc_wchar wc_jisx0212_to_jisx0213(struct wc_option opts, struct wc_wchar cc);
struct wc_wchar wc_jisx0213_to_jisx0212(struct wc_option opts, struct wc_wchar cc);
wc_ccs wc_jisx0208_or_jisx02131(wc_uint16 code);
wc_ccs wc_jisx0212_or_jisx02132(wc_uint16 code);
