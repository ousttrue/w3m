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

pStr wc_char_conv(struct wc_option* WcOption, char c)
{
    return wc_Str_conv(WcOption,
        (*char_conv_st.ces_info->char_conv)(WcOption, (uint8_t)c, &char_conv_st),
        WC_CES_WTF, char_conv_t_ces);
}
