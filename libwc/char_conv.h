#pragma once
#include "ces.h"

extern void wc_char_conv_init(wc_ces f_ces, wc_ces t_ces);
struct wc_option;
extern pStr wc_char_conv(struct wc_option* WcOption, char c);
