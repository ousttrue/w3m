#include "graphicchar.h"
#include "term_entry.h"
#include <string.h>

enum GrahicCharType UseGraphicChar = GRAPHIC_CHAR_CHARSET;

static char gcmap[96];

char graphchar(char c)
{
    return (((unsigned)(c) >= ' ' && (unsigned)(c) < 128) ? gcmap[(c) - ' '] : (c));
}

void setgraphchar(const struct TermEntry* t)
{
    int c;
    for (c = 0; c < 96; c++)
        gcmap[c] = (char)(c + ' ');

    if (!t->ac)
        return;

    int n = strlen(t->ac);
    for (int i = 0; i < n - 1; i += 2) {
        c = (unsigned)t->ac[i] - ' ';
        if (c >= 0 && c < 96)
            gcmap[c] = t->ac[i + 1];
    }
}

bool graph_ok(const struct TermEntry* t)
{
    if (UseGraphicChar != GRAPHIC_CHAR_DEC)
        return 0;
    return t->as[0] != 0 && t->ae[0] != 0 && t->ac[0] != 0;
}
