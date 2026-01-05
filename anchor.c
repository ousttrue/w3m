#include "anchor.h"
#include "indep.h"
#include "alloc.h"
#include "file.h"
#include "html_form.h"
#include "buffer.h"
#include "w3m_rc.h"
#include "image.h"
#include "maparea.h"
#include "myctype.h"
#include "regex.h"
#include <string.h>

#define FIRST_ANCHOR_SIZE 30

#define bpcmp(a, b) \
    (((a).line - (b).line) ? ((a).line - (b).line) : ((a).pos - (b).pos))

typedef struct Anchor* (*AnchorProc)(struct Url* base_url, struct Document* doc, const char*, const char*, int, int);

static struct Anchor*
_put_anchor_all(struct Url* base_url, struct Document* doc, const char* p1, const char* p2, int line, int pos)
{
    Str tmp = Strnew_charp_n(p1, p2 - p1);
    return registerHref(doc, url_encode(tmp->ptr, base_url, doc->charset),
        NULL, NO_REFERER, NULL,
        '\0', line, pos);
}

struct AnchorList*
putAnchor(struct AnchorList* al, const char* url, const char* target, struct Anchor** anchor_return,
    const char* referer, const char* title, unsigned char key, int line, int pos)
{
    int n, i, j;
    struct Anchor* a;
    struct BufferPoint bp = { 0 };
    if (al == NULL) {
        al = New(struct AnchorList);
        al->anchors = NULL;
        al->nanchor = al->anchormax = 0;
        al->acache = -1;
    }
    if (al->anchormax == 0) {
        /* first time; allocate anchor buffer */
        al->anchors = New_N(struct Anchor, FIRST_ANCHOR_SIZE);
        al->anchormax = FIRST_ANCHOR_SIZE;
    }
    if (al->nanchor == al->anchormax) { /* need realloc */
        al->anchormax *= 2;
        al->anchors = New_Reuse(struct Anchor, al->anchors, al->anchormax);
    }
    bp.line = line;
    bp.pos = pos;
    n = al->nanchor;
    if (!n || bpcmp(al->anchors[n - 1].start, bp) < 0)
        i = n;
    else
        for (i = 0; i < n; i++) {
            if (bpcmp(al->anchors[i].start, bp) >= 0) {
                for (j = n; j > i; j--)
                    al->anchors[j] = al->anchors[j - 1];
                break;
            }
        }
    a = &al->anchors[i];
    a->url = url;
    a->target = target;
    a->referer = referer;
    a->title = title;
    a->accesskey = key;
    a->slave = FALSE;
    a->start = bp;
    a->end = bp;
    al->nanchor++;
    if (anchor_return)
        *anchor_return = a;
    return al;
}

struct Anchor*
registerHref(struct Document* doc, const char* url, const char* target, const char* referer, const char* title,
    unsigned char key, int line, int pos)
{
    struct Anchor* a;
    doc->href = putAnchor(doc->href, url, target, &a, referer, title, key,
        line, pos);
    return a;
}

struct Anchor*
registerName(struct Document* doc, const char* url, int line, int pos)
{
    struct Anchor* a;
    doc->name = putAnchor(doc->name, url, NULL, &a, NULL, NULL, '\0', line, pos);
    return a;
}

struct Anchor*
registerImg(struct Document* doc,
    const char* url, const char* title, int line, int pos)
{
    struct Anchor* a;
    doc->img = putAnchor(doc->img, url, NULL, &a, NULL, title, '\0', line,
        pos);
    return a;
}

struct Anchor*
registerForm(struct HtmlBuilder* hb, struct Document* doc,
    struct FormList* flist, struct HtmlTag* tag, int line,
    int pos)
{
    struct FormItemList* fi = formList_addInput(hb, flist, tag);
    if (fi == NULL)
        return NULL;

    struct Anchor* a;
    doc->formitem = putAnchor(doc->formitem, (char*)fi, flist->target, &a,
        NULL, NULL, '\0', line, pos);
    return a;
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

struct Anchor*
retrieveAnchor(struct AnchorList* al, int line, int pos)
{
    struct Anchor* a;
    size_t b, e;
    int cmp;

    if (al == NULL || al->nanchor == 0)
        return NULL;

    if (al->acache < 0 || al->acache >= al->nanchor)
        al->acache = 0;

    for (b = 0, e = al->nanchor - 1; b <= e; al->acache = (b + e) / 2) {
        a = &al->anchors[al->acache];
        cmp = onAnchor(a, line, pos);
        if (cmp == 0)
            return a;
        else if (cmp > 0)
            b = al->acache + 1;
        else if (al->acache == 0)
            return NULL;
        else
            e = al->acache - 1;
    }
    return NULL;
}

struct Anchor*
searchAnchor(struct AnchorList* al, const char* str)
{
    int i;
    struct Anchor* a;
    if (al == NULL)
        return NULL;
    for (i = 0; i < al->nanchor; i++) {
        a = &al->anchors[i];
        if (a->hseq < 0)
            continue;
        if (!strcmp(a->url, str))
            return a;
    }
    return NULL;
}

struct Anchor*
searchURLLabel(struct Document* doc, const char* url)
{
    return searchAnchor(doc->name, url);
}

static void
reseq_anchor0(struct AnchorList* al, short* seqmap)
{
    int i;
    struct Anchor* a;

    if (!al)
        return;

    for (i = 0; i < al->nanchor; i++) {
        a = &al->anchors[i];
        if (a->hseq >= 0) {
            a->hseq = seqmap[a->hseq];
        }
    }
}

/* renumber anchor */
static void
reseq_anchor(struct Document* doc)
{
    int i, j, n, nmark = (doc->hmarklist) ? doc->hmarklist->nmark : 0;
    short* seqmap;
    struct Anchor *a, *a1;
    struct HmarkerList* ml = NULL;

    if (!doc->href)
        return;

    n = nmark;
    for (i = 0; i < doc->href->nanchor; i++) {
        a = &doc->href->anchors[i];
        if (a->hseq == -2)
            n++;
    }

    if (n == nmark)
        return;

    seqmap = NewAtom_N(short, n);

    for (i = 0; i < n; i++)
        seqmap[i] = i;

    n = nmark;
    for (i = 0; i < doc->href->nanchor; i++) {
        a = &doc->href->anchors[i];
        if (a->hseq == -2) {
            a->hseq = n;
            a1 = closest_next_anchor(doc->href, NULL, a->start.pos,
                a->start.line);
            a1 = closest_next_anchor(doc->formitem, a1, a->start.pos,
                a->start.line);
            if (a1 && a1->hseq >= 0) {
                seqmap[n] = seqmap[a1->hseq];
                for (j = a1->hseq; j < nmark; j++)
                    seqmap[j]++;
            }
            ml = putHmarker(ml, a->start.line, a->start.pos, seqmap[n]);
            n++;
        }
    }

    for (i = 0; i < nmark; i++) {
        ml = putHmarker(ml, doc->hmarklist->marks[i].line,
            doc->hmarklist->marks[i].pos, seqmap[i]);
    }
    doc->hmarklist = ml;

    reseq_anchor0(doc->href, seqmap);
    reseq_anchor0(doc->formitem, seqmap);
}

static const char*
reAnchorPos(struct Url* base_url,
    struct Document* doc, struct Line* l, const char* p1, const char* p2, AnchorProc anchorproc)
{
    int spos = p1 - l->lineBuf;
    int epos = p2 - l->lineBuf;
    for (int i = spos; i < epos; i++) {
        if (l->propBuf[i] & (PE_ANCHOR | PE_FORM))
            return p2;
    }
    for (int i = spos; i < epos; i++)
        l->propBuf[i] |= PE_ANCHOR;
    while (spos > l->len && l->next && l->next->bpos) {
        spos -= l->len;
        epos -= l->len;
        l = l->next;
    }

    int hseq = -2;
    while (1) {
        struct Anchor* a = anchorproc(base_url, doc, p1, p2, l->linenumber, spos);
        a->hseq = hseq;
        if (hseq == -2) {
            reseq_anchor(doc);
            hseq = a->hseq;
        }
        a->end.line = l->linenumber;
        if (epos > l->len && l->next && l->next->bpos) {
            a->end.pos = l->len;
            spos = 0;
            epos -= l->len;
            l = l->next;
        } else {
            a->end.pos = epos;
            break;
        }
    }
    return p2;
}

void reAnchorWord(struct Url* base_url, struct Document* doc, struct Line* l, int spos, int epos)
{
    reAnchorPos(base_url, doc, l, &l->lineBuf[spos], &l->lineBuf[epos], _put_anchor_all);
}

/* search regexp and register them as anchors */
/* returns error message if any               */
static const char*
reAnchorAny(struct Url* base_url, struct Document* doc, const char* re, AnchorProc anchorproc)
{
    if (re == NULL || *re == '\0') {
        return NULL;
    }
    if ((re = regexCompile(re, 1)) != NULL) {
        return re;
    }

    const char* p = NULL;
    for (struct Line* l = getRuntime()->MarkAllPages ? doc->firstLine : doc->topLine;
        l != NULL && (getRuntime()->MarkAllPages || l->linenumber < doc->topLine->linenumber + LASTLINE());
        l = l->next) {
        if (p && l->bpos)
            break;
        p = l->lineBuf;
        for (;;) {
            if (regexMatch(p, &l->lineBuf[l->size] - p, p == l->lineBuf) == 1) {
                const char *p1, *p2;
                matchedPosition(&p1, &p2);
                p = reAnchorPos(base_url, doc, l, p1, p2, anchorproc);
            } else
                break;
        }
    }
    return NULL;
}

const char* reAnchor(struct Url* base_url, struct Document* doc, const char* re)
{
    return reAnchorAny(base_url, doc, re, _put_anchor_all);
}

#define FIRST_MARKER_SIZE 30
struct HmarkerList*
putHmarker(struct HmarkerList* ml, int line, int pos, int seq)
{
    if (ml == NULL) {
        ml = New(struct HmarkerList);
        ml->marks = NULL;
        ml->nmark = 0;
        ml->markmax = 0;
        ml->prevhseq = -1;
    }
    if (ml->markmax == 0) {
        ml->markmax = FIRST_MARKER_SIZE;
        ml->marks = NewAtom_N(struct BufferPoint, ml->markmax);
        bzero(ml->marks, sizeof(struct BufferPoint) * ml->markmax);
    }
    if (seq + 1 > ml->nmark)
        ml->nmark = seq + 1;
    if (ml->nmark >= ml->markmax) {
        ml->markmax = ml->nmark * 2;
        ml->marks = New_Reuse(struct BufferPoint, ml->marks, ml->markmax);
    }
    ml->marks[seq].line = line;
    ml->marks[seq].pos = pos;
    ml->marks[seq].invalid = 0;
    return ml;
}

struct Anchor*
closest_next_anchor(struct AnchorList* a, struct Anchor* an, int x, int y)
{
    int i;

    if (a == NULL || a->nanchor == 0)
        return an;
    for (i = 0; i < a->nanchor; i++) {
        if (a->anchors[i].hseq < 0)
            continue;
        if (a->anchors[i].start.line > y || (a->anchors[i].start.line == y && a->anchors[i].start.pos > x)) {
            if (an == NULL || an->start.line > a->anchors[i].start.line || (an->start.line == a->anchors[i].start.line && an->start.pos > a->anchors[i].start.pos))
                an = &a->anchors[i];
        }
    }
    return an;
}

struct Anchor*
closest_prev_anchor(struct AnchorList* a, struct Anchor* an, int x, int y)
{
    int i;

    if (a == NULL || a->nanchor == 0)
        return an;
    for (i = 0; i < a->nanchor; i++) {
        if (a->anchors[i].hseq < 0)
            continue;
        if (a->anchors[i].end.line < y || (a->anchors[i].end.line == y && a->anchors[i].end.pos <= x)) {
            if (an == NULL || an->end.line < a->anchors[i].end.line || (an->end.line == a->anchors[i].end.line && an->end.pos < a->anchors[i].end.pos))
                an = &a->anchors[i];
        }
    }
    return an;
}

void shiftAnchorPosition(struct AnchorList* al, struct HmarkerList* hl, int line, int pos,
    int shift)
{
    struct Anchor* a;
    size_t b, e, s = 0;
    int cmp;

    if (al == NULL || al->nanchor == 0)
        return;

    s = al->nanchor / 2;
    for (b = 0, e = al->nanchor - 1; b <= e; s = (b + e + 1) / 2) {
        a = &al->anchors[s];
        cmp = onAnchor(a, line, pos);
        if (cmp == 0)
            break;
        else if (cmp > 0)
            b = s + 1;
        else if (s == 0)
            break;
        else
            e = s - 1;
    }
    for (; s < al->nanchor; s++) {
        a = &al->anchors[s];
        if (a->start.line > line)
            break;
        if (a->start.pos > pos) {
            a->start.pos += shift;
            if (hl && hl->marks && a->hseq >= 0 && hl->marks[a->hseq].line == line)
                hl->marks[a->hseq].pos = a->start.pos;
        }
        if (a->end.pos >= pos)
            a->end.pos += shift;
    }
}

void addMultirowsImg(struct Document* doc, struct AnchorList* al)
{
    if (al == NULL || al->nanchor == 0)
        return;
    for (int i = 0; i < al->nanchor; i++) {
        struct Anchor a_img = al->anchors[i];
        struct Image* img = a_img.image;
        if (a_img.hseq < 0 || !img || img->rows <= 1)
            continue;
        struct Line* l = doc->firstLine;
        for (; l != NULL; l = l->next) {
            if (l->linenumber == img->y)
                break;
        }
        if (!l)
            continue;

        struct Line* ls;
        if (a_img.y == a_img.start.line)
            ls = l;
        else {
            for (ls = l; ls != NULL;
                ls = (a_img.y < a_img.start.line) ? ls->next : ls->prev) {
                if (ls->linenumber == a_img.start.line)
                    break;
            }
            if (!ls)
                continue;
        }
        struct Anchor* a = retrieveAnchor(doc->href, a_img.start.line, a_img.start.pos);
        struct Anchor a_href;
        if (a)
            a_href = *a;
        else
            a_href.url = NULL;
        a = retrieveAnchor(doc->formitem, a_img.start.line, a_img.start.pos);
        struct Anchor a_form;
        if (a)
            a_form = *a;
        else
            a_form.url = NULL;
        int col = COLPOS(ls, a_img.start.pos);
        int ecol = COLPOS(ls, a_img.end.pos);
        for (int j = 0; l && j < img->rows; l = l->next, j++) {
            if (a_img.start.line == l->linenumber)
                continue;
            int pos = columnPos(l, col);
            a = registerImg(doc, a_img.url, a_img.title, l->linenumber, pos);
            a->hseq = -a_img.hseq;
            a->slave = TRUE;
            a->image = img;
            a->end.pos = pos + ecol - col;
            for (int k = pos; k < a->end.pos; k++)
                l->propBuf[k] |= PE_IMAGE;
            if (a_href.url) {
                a = registerHref(doc, a_href.url, a_href.target,
                    a_href.referer, a_href.title,
                    a_href.accesskey, l->linenumber, pos);
                a->hseq = a_href.hseq;
                a->slave = TRUE;
                a->end.pos = pos + ecol - col;
                for (int k = pos; k < a->end.pos; k++)
                    l->propBuf[k] |= PE_ANCHOR;
            }
            if (a_form.url) {
                doc->formitem = putAnchor(doc->formitem, a_form.url,
                    a_form.target, &a, NULL, NULL, '\0',
                    l->linenumber, pos);
                a->hseq = a_form.hseq;
                a->end.pos = pos + ecol - col;
            }
        }
        img->rows = 0;
    }
}

void addMultirowsForm(struct Document* doc, struct AnchorList* al)
{
    if (al == NULL || al->nanchor == 0)
        return;

    for (int i = 0; i < al->nanchor; i++) {
        struct Anchor a_form = al->anchors[i];
        al->anchors[i].rows = 1;
        if (a_form.hseq < 0 || a_form.rows <= 1)
            continue;
        struct Line* l = doc->firstLine;
        for (; l != NULL; l = l->next) {
            if (l->linenumber == a_form.y)
                break;
        }
        if (!l)
            continue;
        struct Line* ls;
        if (a_form.y == a_form.start.line)
            ls = l;
        else {
            for (ls = l; ls != NULL;
                ls = (a_form.y < a_form.start.line) ? ls->next : ls->prev) {
                if (ls->linenumber == a_form.start.line)
                    break;
            }
            if (!ls)
                continue;
        }
        int col = COLPOS(ls, a_form.start.pos);
        int ecol = COLPOS(ls, a_form.end.pos);
        for (int j = 0; l && j < a_form.rows; l = l->next, j++) {
            int pos = columnPos(l, col);
            if (j == 0) {
                doc->hmarklist->marks[a_form.hseq].line = l->linenumber;
                doc->hmarklist->marks[a_form.hseq].pos = pos;
            }
            if (a_form.start.line == l->linenumber)
                continue;
            struct Anchor* a;
            doc->formitem = putAnchor(doc->formitem, a_form.url,
                a_form.target, &a, NULL, NULL, '\0',
                l->linenumber, pos);
            a->hseq = a_form.hseq;
            a->y = a_form.y;
            a->end.pos = pos + ecol - col;
            if (pos < 1 || a->end.pos >= l->size)
                continue;
            l->lineBuf[pos - 1] = '[';
            l->lineBuf[a->end.pos] = ']';
            for (int k = pos; k < a->end.pos; k++)
                l->propBuf[k] |= PE_FORM;
        }
    }
}

const char* getAnchorText(struct Document* doc, struct AnchorList* al, struct Anchor* a)
{
    Str tmp = NULL;
    if (!a || a->hseq < 0)
        return NULL;
    int hseq = a->hseq;
    struct Line* l = doc->firstLine;
    for (int i = 0; i < al->nanchor; i++) {
        a = &al->anchors[i];
        if (a->hseq != hseq)
            continue;
        for (; l; l = l->next) {
            if (l->linenumber == a->start.line)
                break;
        }
        if (!l)
            break;
        const char* p = l->lineBuf + a->start.pos;
        const char* ep = l->lineBuf + a->end.pos;
        for (; p < ep && IS_SPACE(*p); p++)
            ;
        if (p == ep)
            continue;
        if (!tmp)
            tmp = Strnew_size(ep - p);
        else
            Strcat_char(tmp, ' ');
        Strcat_charp_n(tmp, p, ep - p);
    }
    return tmp ? tmp->ptr : NULL;
}

Str link_list_panel(struct Url* base_url, struct Document* doc)
{
    // struct LinkList* l;
    // struct AnchorList* al;
    // struct Anchor* a;
    // struct FormItemList* fi;
    // int i;
    // const char *t, *u, *p;
    Str tmp = Strnew_charp("<title>Link List</title>\
<h1 align=center>Link List</h1>\n");

    if (
        // buf->bufferprop & BP_INTERNAL ||
        (doc->linklist == NULL && doc->href == NULL && doc->img == NULL)) {
        return NULL;
    }

    if (doc->linklist) {
        Strcat_charp(tmp, "<hr><h2>Links</h2>\n<ol>\n");
        for (struct LinkList* l = doc->linklist; l; l = l->next) {
            const char* p;
            const char* u;
            if (l->url) {
                struct Url pu;
                parseURL2(l->url, &pu, base_url);
                p = parsedURL2Str(&pu)->ptr;
                u = html_quote(p);
                if (getRuntime()->DecodeURL)
                    p = html_quote(url_decode2(base_url, doc, p));
                else
                    p = u;
            } else
                u = p = "";
            const char* t;
            if (l->type == LINK_TYPE_REL)
                t = " [Rel]";
            else if (l->type == LINK_TYPE_REV)
                t = " [Rev]";
            else
                t = "";
            t = Sprintf("%s%s\n", l->title ? l->title : "", t)->ptr;
            t = html_quote(t);
            Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p,
                "\n", NULL);
        }
        Strcat_charp(tmp, "</ol>\n");
    }

    if (doc->href) {
        Strcat_charp(tmp, "<hr><h2>Anchors</h2>\n<ol>\n");
        struct AnchorList* al = doc->href;
        for (int i = 0; i < al->nanchor; i++) {
            struct Anchor* a = &al->anchors[i];
            if (a->hseq < 0 || a->slave)
                continue;
            struct Url pu;
            parseURL2(a->url, &pu, base_url);
            const char* p = parsedURL2Str(&pu)->ptr;
            const char* u = html_quote(p);
            if (getRuntime()->DecodeURL)
                p = html_quote(url_decode2(base_url, doc, p));
            else
                p = u;
            const char* t = getAnchorText(doc, al, a);
            t = t ? html_quote(t) : "";
            Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p,
                "\n", NULL);
        }
        Strcat_charp(tmp, "</ol>\n");
    }

    if (doc->img) {
        Strcat_charp(tmp, "<hr><h2>Images</h2>\n<ol>\n");
        struct AnchorList* al = doc->img;
        for (int i = 0; i < al->nanchor; i++) {
            struct Anchor* a = &al->anchors[i];
            if (a->slave)
                continue;
            struct Url pu;
            parseURL2(a->url, &pu, base_url);
            const char* p = parsedURL2Str(&pu)->ptr;
            const char* u = html_quote(p);
            if (getRuntime()->DecodeURL)
                p = html_quote(url_decode2(base_url, doc, p));
            else
                p = u;
            const char* t;
            if (a->title && *a->title)
                t = html_quote(a->title);
            else
                t = html_quote(url_decode2(base_url, doc, a->url));
            Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p,
                "\n", NULL);
            a = retrieveAnchor(doc->formitem, a->start.line, a->start.pos);
            if (!a)
                continue;
            struct FormItemList* fi = (struct FormItemList*)a->url;
            fi = fi->parent->item;
            if (fi->parent->method == FORM_METHOD_INTERNAL && !Strcmp_charp(fi->parent->action, "map") && fi->value) {
                struct MapList* ml = searchMapList(doc, fi->value->ptr);
                ListItem* mi;
                struct MapArea* m;
                if (!ml)
                    continue;
                Strcat_charp(tmp, "<br>\n<b>Image map</b>\n<ol>\n");
                for (mi = ml->area->first; mi != NULL; mi = mi->next) {
                    m = (struct MapArea*)mi->ptr;
                    if (!m)
                        continue;
                    parseURL2(m->url, &pu, base_url);
                    p = parsedURL2Str(&pu)->ptr;
                    u = html_quote(p);
                    if (getRuntime()->DecodeURL)
                        p = html_quote(url_decode2(base_url, doc, p));
                    else
                        p = u;
                    if (m->alt && *m->alt)
                        t = html_quote(m->alt);
                    else
                        t = html_quote(url_decode2(base_url, doc, m->url));
                    Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t,
                        "</a><br>", p, "\n", NULL);
                }
                Strcat_charp(tmp, "</ol>\n");
            }
        }
        Strcat_charp(tmp, "</ol>\n");
    }
    return tmp;
}
