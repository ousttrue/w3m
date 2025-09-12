#include "Anchor.h"
#include "alloc.h"
#include "HttpRequest.h"
#include "quote.h"
#include "w3m.h"
#include "buffer_loader.h"
#include "screen.h"
#include "html_quote.h"
#include "form.h"
#include "buffer.h"
#include "maparea.h"
#include "image.h"
#include "myctype.h"
#include "regex.h"
#include <string.h>

int MarkAllPages = (false);

typedef struct Anchor* (*AnchorFunc)(struct Buffer*, const char*, const char*, int, int);

#define bpcmp(a, b) \
    (((a).line - (b).line) ? ((a).line - (b).line) : ((a).pos - (b).pos))

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

int onAnchor(struct Anchor* a, int line, int pos)
{
    struct BufferPoint bp;
    bp.line = line;
    bp.pos = pos;

    if (bpcmp(bp, a->start) < 0)
        return -1;
    if (bpcmp(a->end, bp) <= 0)
        return 1;
    return 0;
}

static struct Anchor*
_put_anchor_all(struct Buffer* buf, const char* p1, const char* p2, int line, int pos)
{
    Str tmp = Strnew_charp_n(p1, p2 - p1);
    return registerHref(buf, url_quote(tmp->ptr),
        NULL, NO_REFERER, NULL,
        '\0', line, pos);
}

static char*
reAnchorPos(struct Buffer* buf, struct LineList* l, char* p1, char* p2, AnchorFunc anchorproc)
{
    struct Anchor* a;
    int spos, epos;
    int i, hseq = -2;

    spos = p1 - l->l.lineBuf;
    epos = p2 - l->l.lineBuf;
    for (i = spos; i < epos; i++) {
        if (l->l.propBuf[i] & (PE_ANCHOR | PE_FORM))
            return p2;
    }
    for (i = spos; i < epos; i++)
        l->l.propBuf[i] |= PE_ANCHOR;
    while (spos > l->l.len && l->next && l->next->bpos) {
        spos -= l->l.len;
        epos -= l->l.len;
        l = l->next;
    }
    while (1) {
        a = anchorproc(buf, p1, p2, l->linenumber, spos);
        a->hseq = hseq;
        if (hseq == -2) {
            reseq_anchor(buf);
            hseq = a->hseq;
        }
        a->end.line = l->linenumber;
        if (epos > l->l.len && l->next && l->next->bpos) {
            a->end.pos = l->l.len;
            spos = 0;
            epos -= l->l.len;
            l = l->next;
        } else {
            a->end.pos = epos;
            break;
        }
    }
    return p2;
}

void reAnchorWord(struct Buffer* buf, struct LineList* l, int spos, int epos)
{
    reAnchorPos(buf, l, &l->l.lineBuf[spos], &l->l.lineBuf[epos], _put_anchor_all);
}

/* search regexp and register them as anchors */
/* returns error message if any               */
static const char*
reAnchorAny(struct Buffer* buf, const char* re, AnchorFunc anchorproc)
{
    struct LineList* l;
    char *p = NULL, *p1, *p2;

    if (re == NULL || *re == '\0') {
        return NULL;
    }
    if ((re = regexCompile(re, 1)) != NULL) {
        return re;
    }
    for (l = MarkAllPages ? buf->firstLine : topLine(buf); l != NULL && (MarkAllPages || l->linenumber < topLine(buf)->linenumber + getScreen()->ROWS - 1);
        l = l->next) {
        if (p && l->bpos)
            continue;
        p = l->l.lineBuf;
        for (;;) {
            if (regexMatch(p, &l->l.lineBuf[l->l.size] - p, p == l->l.lineBuf) == 1) {
                matchedPosition(&p1, &p2);
                p = reAnchorPos(buf, l, p1, p2, anchorproc);
            } else
                break;
        }
    }
    return NULL;
}

const char* reAnchor(struct Buffer* buf, const char* re)
{
    return reAnchorAny(buf, re, _put_anchor_all);
}


