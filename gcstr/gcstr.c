#include "gcstr.h"
#include <string.h>

#define SP_NORMAL 0
#define SP_PREC 1
#define SP_PREC2 2

int vscpf(const char* fmt, va_list ap)
{
    int len = 0;
    int status = SP_NORMAL;
    int p = 0;
    for (const char* f = fmt; *f; f++) {
    redo:
        switch (status) {
        case SP_NORMAL:
            if (*f == '%') {
                status = SP_PREC;
                p = 0;
            } else
                len++;
            break;
        case SP_PREC:
            if (IS_ALPHA(*f)) {
                /* conversion char. */
                int vi;
                char* vs;

                switch (*f) {
                case 'l':
                case 'h':
                case 'L':
                case 'w':
                    continue;
                case 'd':
                case 'i':
                case 'o':
                case 'x':
                case 'X':
                case 'u':
                    vi = va_arg(ap, int);
                    len += (p > 0) ? p : 10;
                    break;
                case 'f':
                case 'g':
                case 'e':
                case 'G':
                case 'E':
                    va_arg(ap, double);
                    len += (p > 0) ? p : 15;
                    break;
                case 'c':
                    len += 1;
                    vi = va_arg(ap, int);
                    break;
                case 's':
                    vs = va_arg(ap, char*);
                    vi = strlen(vs);
                    len += (p > vi) ? p : vi;
                    break;
                case 'p':
                    va_arg(ap, void*);
                    len += 10;
                    break;
                case 'n':
                    va_arg(ap, void*);
                    break;
                }
                status = SP_NORMAL;
            } else if (IS_DIGIT(*f))
                p = p * 10 + *f - '0';
            else if (*f == '.')
                status = SP_PREC2;
            else if (*f == '%') {
                status = SP_NORMAL;
                len++;
            }
            break;
        case SP_PREC2:
            if (IS_ALPHA(*f)) {
                status = SP_PREC;
                goto redo;
            }
            break;
        }
    }

    return len;
}

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
