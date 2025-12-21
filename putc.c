#include "putc.h"
#include "ces.h"
#include "conv.h"
#include "status.h"
#include "wtf.h"
#include <unistd.h>

struct PutcStatus wc_putc_init(wc_ces f_ces, wc_ces t_ces)
{
    struct PutcStatus status = {
        .putc_str = Strnew_size(8),
        .putc_f_ces = f_ces,
        .putc_t_ces = t_ces,
    };
    wc_output_init(t_ces, &status.putc_st);
    return status;
}

void wc_putc(struct PutcStatus* status, char* c, int fd)
{
    wc_uchar* p;
    if (status->putc_f_ces != WC_CES_WTF)
        p = (wc_uchar*)wc_conv(c, status->putc_f_ces, WC_CES_WTF)->ptr;
    else
        p = (wc_uchar*)c;

    Strclear(status->putc_str);
    while (*p)
        (*status->putc_st.ces_info->push_to)(status->putc_str, wtf_parse(&p), &status->putc_st);
    write(fd, status->putc_str->ptr, status->putc_str->length);
}

void wc_putc_end(struct PutcStatus* status, int fd)
{
    Strclear(status->putc_str);
    wc_push_end(status->putc_str, &status->putc_st);
    if (status->putc_str->length)
        write(fd, status->putc_str->ptr, status->putc_str->length);
}

void wc_putc_clear_status(struct PutcStatus* status)
{
    if (status->putc_st.ces_info->id & WC_CES_T_ISO_2022) {
        status->putc_st.gl = 0;
        status->putc_st.gr = 0;
        status->putc_st.ss = 0;
        status->putc_st.design[0] = 0;
        status->putc_st.design[1] = 0;
        status->putc_st.design[2] = 0;
        status->putc_st.design[3] = 0;
    }
}
