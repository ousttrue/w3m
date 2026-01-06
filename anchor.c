#include "anchor.h"
#include "anchor_list.h"
#include "hmarker.h"
#include "document.h"
#include "indep.h"
#include "alloc.h"
#include "file.h"
#include "html_form.h"
#include "w3m_rc.h"
#include "maparea.h"
#include "myctype.h"
#include "regex.h"
#include <string.h>

typedef struct Anchor* (*AnchorProc)(struct Url* base_url, struct Document* doc,
    const char*, const char*, struct BufferPoint bp);

static struct Anchor*
_put_anchor_all(struct Url* base_url, struct Document* doc, const char* p1, const char* p2, struct BufferPoint bp)
{
    Str tmp = Strnew_charp_n(p1, p2 - p1);
    return doc_registerHref(doc, bp,
        url_encode(tmp->ptr, base_url, doc->charset), NULL, NO_REFERER, NULL, '\0');
}

struct Anchor*
registerForm(struct HtmlBuilder* hb, struct Document* doc, struct BufferPoint bp,
    struct FormList* flist, struct HtmlTag* tag)
{
    struct FormItemList* fi = formList_addInput(hb, flist, tag);
    if (fi == NULL)
        return NULL;

    return al_put(&doc->formitem, (char*)fi, flist->target, NULL, NULL, '\0', bp);
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
    int j, nmark = (doc->hmarklist) ? doc->hmarklist->nmark : 0;

    int n = nmark;
    for (int i = 0; i < doc->href.nanchor; i++) {
        struct Anchor* a = &doc->href.anchors[i];
        if (a->hseq == -2)
            n++;
    }

    if (n == nmark)
        return;

    short* seqmap = NewAtom_N(short, n);

    for (int i = 0; i < n; i++)
        seqmap[i] = i;

    n = nmark;
    struct HmarkerList* ml = NULL;
    for (int i = 0; i < doc->href.nanchor; i++) {
        struct Anchor* a = &doc->href.anchors[i];
        if (a->hseq == -2) {
            a->hseq = n;
            struct Anchor* a1 = al_closestNext(&doc->href, NULL, a->start);
            a1 = al_closestNext(&doc->formitem, a1, a->start);
            if (a1 && a1->hseq >= 0) {
                seqmap[n] = seqmap[a1->hseq];
                for (j = a1->hseq; j < nmark; j++)
                    seqmap[j]++;
            }
            ml = hm_put(ml, a->start, seqmap[n]);
            n++;
        }
    }

    for (int i = 0; i < nmark; i++) {
        ml = hm_put(ml, doc->hmarklist->marks[i], seqmap[i]);
    }
    doc->hmarklist = ml;

    reseq_anchor0(&doc->href, seqmap);
    reseq_anchor0(&doc->formitem, seqmap);
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
        struct Anchor* a = anchorproc(base_url, doc, p1, p2, (struct BufferPoint) { .line = l->linenumber, .pos = spos });
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

    if (doc->href.nanchor > 0) {
        Strcat_charp(tmp, "<hr><h2>Anchors</h2>\n<ol>\n");
        struct AnchorList* al = &doc->href;
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
            const char* t = doc_getAnchorText(doc, al, a);
            t = t ? html_quote(t) : "";
            Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p,
                "\n", NULL);
        }
        Strcat_charp(tmp, "</ol>\n");
    }

    if (doc->img.nanchor > 0) {
        Strcat_charp(tmp, "<hr><h2>Images</h2>\n<ol>\n");
        struct AnchorList* al = &doc->img;
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
            a = al_retrieve(&doc->formitem,
                (struct BufferPoint) { .line = a->start.line, .pos = a->start.pos });
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
