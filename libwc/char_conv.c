#include "char_conv.h"
#include "ces.h"
#include "status.h"
#include "conv.h"

static enum wc_ces char_conv_f_ces = 0, char_conv_t_ces = WC_CES_WTF;
static struct wc_status char_conv_st;

void wc_char_conv_init(enum wc_ces f_ces, enum wc_ces t_ces)
{
    wc_input_init(&char_conv_st, f_ces);
    char_conv_st.state = -1;
    char_conv_f_ces = f_ces;
    char_conv_t_ces = t_ces;
}

Str wc_char_conv(char c)
{
    return wc_Str_conv((*char_conv_st.ces_info->char_conv)((wc_uchar)c, &char_conv_st),
        WC_CES_WTF, char_conv_t_ces);
}
