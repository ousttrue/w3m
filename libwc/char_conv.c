#include "char_conv.h"
#include "detect.h"
#include "status.h"
#include "conv.h"

static wc_ces char_conv_f_ces = 0;
static wc_ces char_conv_t_ces = WC_CES_WTF;
static struct wc_status char_conv_st;

void wc_char_conv_init(wc_ces f_ces, wc_ces t_ces)
{
    wc_input_init(f_ces, &char_conv_st);
    char_conv_st.state = -1;
    char_conv_f_ces = f_ces;
    char_conv_t_ces = t_ces;
}

void wc_char_conv(struct wc_option* WcOption, pStr os, char c)
{
    pStr tmp = Strnew_size(8);
    (*char_conv_st.ces_info->char_conv)(WcOption, tmp, (uint8_t)c, &char_conv_st);
    wc_Str_conv(WcOption, os, tmp, WC_CES_WTF, char_conv_t_ces);
}
