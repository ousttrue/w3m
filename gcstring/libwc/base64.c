#include "base64.h"

static char Base64Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

Str base64_encode(const char* src, size_t len)
{
    Str dest;
    const unsigned char *in, *endw, *s;
    unsigned long j;
    size_t k;

    s = (unsigned char*)src;

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
    Strnulterm(dest);
    return dest;
}
