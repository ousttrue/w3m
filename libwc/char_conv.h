#pragma once
#include "ces.h"

extern void wc_char_conv_init(wc_ces f_ces, wc_ces t_ces);
struct wc_option;
extern void wc_char_conv(struct wc_option* WcOption, pStr os, char c);
