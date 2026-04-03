#include "putc.h"
#include "conv.h"
#include "status.h"
#include "wtf.h"

static struct wc_status putc_st;
static wc_ces putc_f_ces, putc_t_ces;
static struct wc_output putc_str;

void wc_putc_init(struct wc_option opts, wc_ces f_ces, wc_ces t_ces)
{
    wc_output_init(opts, t_ces, &putc_st);
    putc_str = wc_output_from_size(8);
    putc_f_ces = f_ces;
    putc_t_ces = t_ces;
}

void wc_putc(struct wc_option opts, char* c, WriterFunc f)
{
    struct wc_output tmp;
    wc_uchar* p;
    if (putc_f_ces != WC_CES_WTF) {
        tmp = wc_conv(opts, c, putc_f_ces, WC_CES_WTF);
        p = (wc_uchar*)wc_output__ptr(tmp);
    } else
        p = (wc_uchar*)c;

    wc_output__clear(&putc_str);
    while (*p)
        (*putc_st.ces_info->push_to)(opts, &putc_str, wtf_parse(opts, &p), &putc_st);
    f((const wc_uchar*)wc_output__ptr(putc_str), wc_output__len(putc_str));
}

void wc_putc_end(WriterFunc f)
{
    wc_output__clear(&putc_str);
    wc_push_end(&putc_str, &putc_st);
    if (wc_output__len(putc_str))
        f((const wc_uchar*)wc_output__ptr(putc_str), wc_output__len(putc_str));
}

void wc_putc_clear_status(void)
{
    if (putc_st.ces_info->id & WC_CES_T_ISO_2022) {
        putc_st.gl = 0;
        putc_st.gr = 0;
        putc_st.ss = 0;
        putc_st.design[0] = 0;
        putc_st.design[1] = 0;
        putc_st.design[2] = 0;
        putc_st.design[3] = 0;
    }
}
