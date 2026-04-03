
#include <stdio.h>
#include <stdlib.h>
#include <Str.h>
#include <libwc/wc_types.h>
#include <libwc/charset.h>
#include <libwc/conv.h>

int main(int argc, char** argv)
{
    if (argc < 3) {
        fprintf(stderr, "wctest <from> <to> [<file>]\n");
        exit(1);
    }

    wc_ces from = wc_guess_charset_short(argv[1], 0);
    wc_ces to = wc_guess_charset_short(argv[2], 0);

    FILE* f;
    if (argc > 3)
        f = fopen(argv[3], "r");
    else
        f = stdin;
    if (f == NULL)
        exit(2);

    fprintf(stderr, "%s -> %s\n", wc_ces_to_charset(from), wc_ces_to_charset(to));
    Str s = Strnew();
    wc_ces old;
    while (1) {
        s = Strfgets(f);
        if (!s->length)
            break;
        old = from;
        s = wc_Str_conv_with_detect(s, &from, from, to);
        if (from != old)
            fprintf(stderr, "%s ->\n", wc_ces_to_charset(from));
        printf("%s", s->ptr);
    }
    return 0;
}
