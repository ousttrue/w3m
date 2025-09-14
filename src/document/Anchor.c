#include "Anchor.h"
#include <string.h>


int MarkAllPages = (false);

void initAnchor(struct Anchor* a, const char* url, const char* target,
    const char* referer, const char* title, unsigned char key)
{
    a->url = url;
    a->target = target;
    a->referer = referer;
    a->title = title;
    a->accesskey = key;
    a->slave = false;
}

static int bpcmp(struct BufferPoint a, struct BufferPoint b)
{
    return (((a).line - (b).line) ? ((a).line - (b).line) : ((a).pos - (b).pos));
}

// return 0 if on, -1 if left, 1 if right
int onAnchor(struct Anchor* a, struct BufferPoint bp)
{
    if (bpcmp(bp, a->start) < 0)
        return -1;
    if (bpcmp(a->end, bp) <= 0)
        return 1;
    return 0;
}
