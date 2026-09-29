#include "etc.h"
#include "str_gc.h"
#include "charset.h"
#include "config.h"
#include "proto.h"
#include "myctype.h"
#include "rc.h"
#include "symbol.h"

#include <fcntl.h>
#include <libgen.h>
#include <pwd.h>
#include <time.h>
#include <strings.h>

TextList* fileToDelete;

char* url_unquote_conv(const char* url, wc_ces charset)
{
    wc_uint8 old_auto_detect = WcOption.auto_detect;
    Str tmp;
    tmp = Str_url_unquote(Strnew_charp(url), FALSE, TRUE);
    if (!charset || charset == WC_CES_US_ASCII)
        charset = SystemCharset;
    WcOption.auto_detect = WC_OPT_DETECT_ON;
    tmp = convertLine(NULL, tmp, RAW_MODE, &charset, charset);
    WcOption.auto_detect = old_auto_detect;
    return tmp->ptr;
}

static const char* tmpf_base[MAX_TMPF_TYPE] = {
    "tmp",
    "src",
    "frame",
    "cache",
    "cookie",
    "hist",
};
static unsigned int tmpf_seq[MAX_TMPF_TYPE];

Str tmpfname(int type, const char* ext)
{
    Str tmpf;

    tmpf = Sprintf("%s/w3m%s%d-%d%s",
        type == TMPF_HIST ? rc_dir : tmp_dir,
        tmpf_base[type],
        CurrentPid, tmpf_seq[type]++, (ext) ? ext : "");
    pushText(fileToDelete, tmpf->ptr);
    return tmpf;
}

static const char Base64Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

Str base64_encode(const char* src, size_t len)
{
#define Strcatc(x, y) ((x)->ptr[(x)->length++] = (y))
    Str dest;
    const unsigned char *in, *endw, *s;
    unsigned long j;
    size_t k;

    s = (const unsigned char*)src;

    k = len;
    if (k % 3)
        k += 3 - (k % 3);

    k = k / 3 * 4;

    if (!len || k + 1 < len)
        return Strnew();

    dest = Strnew_size(k);
    if (dest->area_size <= k) {
        Strfree(dest);
        return Strnew();
    }

    in = s;

    endw = s + len - 2;

    while (in < endw) {
        j = *in++;
        j = j << 8 | *in++;
        j = j << 8 | *in++;

        Strcatc(dest, Base64Table[(j >> 18) & 0x3f]);
        Strcatc(dest, Base64Table[(j >> 12) & 0x3f]);
        Strcatc(dest, Base64Table[(j >> 6) & 0x3f]);
        Strcatc(dest, Base64Table[j & 0x3f]);
    }

    if (s + len - in) {
        j = *in++;
        if (s + len - in) {
            j = j << 8 | *in++;
            j = j << 8;
            Strcatc(dest, Base64Table[(j >> 18) & 0x3f]);
            Strcatc(dest, Base64Table[(j >> 12) & 0x3f]);
            Strcatc(dest, Base64Table[(j >> 6) & 0x3f]);
        } else {
            j = j << 8;
            j = j << 8;
            Strcatc(dest, Base64Table[(j >> 18) & 0x3f]);
            Strcatc(dest, Base64Table[(j >> 12) & 0x3f]);
            Strcatc(dest, '=');
        }
        Strcatc(dest, '=');
    }
    dest->ptr[dest->length] = '\0';
    return dest;
#undef Strcatc
}
