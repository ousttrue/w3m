#include "Document.h"
#include "Anchor.h"
#include "AnchorList.h"
#include "form.h"
#include "image.h"

#define DOCUMENT_CHARSET WC_CES_UTF_8
wc_ces DocumentCharset = (DOCUMENT_CHARSET);

struct LineList* getLine(struct Document* doc, int i)
{
    for (struct LineList* l = doc->firstLine; l; l = l->next) {
        if (l->linenumber == i) {
            return l;
        }
    }
    return 0;
}

struct LineList* lastLine(struct Document* doc)
{
    struct LineList* l = doc->firstLine;
    if (!l) {
        return 0;
    }
    for (; l->next; l = l->next) {
    }
    return l;
}

struct Anchor*
registerImg(struct Document* doc, const char* url, const char* title, struct BufferPoint bp)
{
    struct Anchor* a;
    doc->img = putAnchor(doc->img, &a, bp);
    initAnchor(a, url, 0, 0, title, '\0');
    return a;
}

struct Anchor*
registerHref(struct Document* doc, const char* url, const char* target, const char* referer, const char* title,
    unsigned char key, struct BufferPoint bp)
{
    struct Anchor* a;
    doc->href = putAnchor(doc->href, &a, bp);
    initAnchor(a, url, target, referer, title, key);
    return a;
}

struct Anchor*
registerName(struct Document* doc, const char* url, struct BufferPoint bp)
{
    struct Anchor* a;
    doc->name = putAnchor(doc->name, &a, bp);
    initAnchor(a, url, 0, 0, 0, '\0');
    return a;
}

struct Anchor*
registerForm(struct Document* doc, struct Form* flist, struct HtmlTagParsed* tag, struct BufferPoint bp)
{
    struct FormItem* fi = formList_addInput(flist, tag);
    if (!fi)
        return 0;

    struct Anchor* a;
    doc->formitem = putAnchor(doc->formitem, &a, bp);
    initAnchor(a, (char*)fi, flist->target, 0, 0, '\0');
    return a;
}

void addMultirowsForm(struct Document* doc, struct AnchorList* al)
{
    int i, j, k, col, ecol, pos;
    struct Anchor a_form, *a;
    struct LineList *l, *ls;

    if (al == 0 || al->nanchor == 0)
        return;
    for (i = 0; i < al->nanchor; i++) {
        a_form = al->anchors[i];
        al->anchors[i].rows = 1;
        if (a_form.hseq < 0 || a_form.rows <= 1)
            continue;
        for (l = doc->firstLine; l != 0; l = l->next) {
            if (l->linenumber == a_form.y)
                break;
        }
        if (!l)
            continue;
        if (a_form.y == a_form.start.line)
            ls = l;
        else {
            for (ls = l; ls != 0;
                ls = (a_form.y < a_form.start.line) ? ls->next : ls->prev) {
                if (ls->linenumber == a_form.start.line)
                    break;
            }
            if (!ls)
                continue;
        }
        col = COLPOS(&ls->l, a_form.start.pos);
        ecol = COLPOS(&ls->l, a_form.end.pos);
        for (j = 0; l && j < a_form.rows; l = l->next, j++) {
            pos = columnPos(&l->l, col);
            if (j == 0) {
                doc->hmarklist->marks[a_form.hseq].line = l->linenumber;
                doc->hmarklist->marks[a_form.hseq].pos = pos;
            }
            if (a_form.start.line == l->linenumber)
                continue;
            doc->formitem = putAnchor(doc->formitem, &a,
                (struct BufferPoint) { .line = l->linenumber, .pos = pos });
            initAnchor(a, a_form.url, a_form.target, 0, 0, '\0');
            a->hseq = a_form.hseq;
            a->y = a_form.y;
            a->end.pos = pos + ecol - col;
            if (pos < 1 || a->end.pos >= l->l.size)
                continue;
            l->l.lineBuf[pos - 1] = '[';
            l->l.lineBuf[a->end.pos] = ']';
            for (k = pos; k < a->end.pos; k++)
                l->l.propBuf[k] |= PE_FORM;
        }
    }
}

void addMultirowsImg(struct Document* doc, struct AnchorList* al)
{
    int i, j, k, col, ecol, pos;
    struct Anchor a_img, a_href, a_form, *a;
    struct LineList *l, *ls;

    if (al == 0 || al->nanchor == 0)
        return;
    for (i = 0; i < al->nanchor; i++) {
        a_img = al->anchors[i];
        struct Image* img;
        img = a_img.image;
        if (a_img.hseq < 0 || !img || img->rows <= 1)
            continue;
        for (l = doc->firstLine; l != 0; l = l->next) {
            if (l->linenumber == img->y)
                break;
        }
        if (!l)
            continue;
        if (a_img.y == a_img.start.line)
            ls = l;
        else {
            for (ls = l; ls != 0;
                ls = (a_img.y < a_img.start.line) ? ls->next : ls->prev) {
                if (ls->linenumber == a_img.start.line)
                    break;
            }
            if (!ls)
                continue;
        }
        a = retrieveAnchor(doc->href, a_img.start);
        if (a)
            a_href = *a;
        else
            a_href.url = 0;
        a = retrieveAnchor(doc->formitem, a_img.start);
        if (a)
            a_form = *a;
        else
            a_form.url = 0;
        col = COLPOS(&ls->l, a_img.start.pos);
        ecol = COLPOS(&ls->l, a_img.end.pos);
        for (j = 0; l && j < img->rows; l = l->next, j++) {
            if (a_img.start.line == l->linenumber)
                continue;
            pos = columnPos(&l->l, col);
            a = registerImg(doc, a_img.url, a_img.title,
                (struct BufferPoint) { .line = l->linenumber, .pos = pos });
            a->hseq = -a_img.hseq;
            a->slave = true;
            a->image = img;
            a->end.pos = pos + ecol - col;
            for (k = pos; k < a->end.pos; k++)
                l->l.propBuf[k] |= PE_IMAGE;
            if (a_href.url) {
                a = registerHref(doc, a_href.url, a_href.target,
                    a_href.referer, a_href.title, a_href.accesskey,
                    (struct BufferPoint) { .line = l->linenumber, .pos = pos });
                a->hseq = a_href.hseq;
                a->slave = true;
                a->end.pos = pos + ecol - col;
                for (k = pos; k < a->end.pos; k++)
                    l->l.propBuf[k] |= PE_ANCHOR;
            }
            if (a_form.url) {
                doc->formitem = putAnchor(doc->formitem, &a,
                    (struct BufferPoint) { .line = l->linenumber, .pos = pos });
                initAnchor(a, a_form.url, a_form.target, 0, 0, '\0');
                a->hseq = a_form.hseq;
                a->end.pos = pos + ecol - col;
            }
        }
        img->rows = 0;
    }
}

struct Anchor* getNextHorizontalAnchor(struct Document* doc, struct Anchor* an, int searchkey_num, int d, int dy)
{
    struct LineList* l = currentLine(doc);
    int x = doc->pos;
    int y = l->linenumber;
    struct Anchor* pan = NULL;
    for (int i = 0; i < searchkey_num; i++) {
        if (an)
            x = (d > 0) ? an->end.pos : an->start.pos - 1;
        an = NULL;
        while (1) {
            for (; x >= 0 && x < l->l.len; x += d) {
                struct BufferPoint bp = { .line = y, .pos = x };
                an = retrieveAnchor(doc->href, bp);
                if (!an)
                    an = retrieveAnchor(doc->formitem, bp);
                if (an) {
                    pan = an;
                    break;
                }
            }
            if (!dy || an)
                break;
            l = (dy > 0) ? l->next : l->prev;
            if (!l)
                break;
            x = (d > 0) ? 0 : l->l.len - 1;
            y = l->linenumber;
        }
        if (!an)
            break;
    }
    return pan;
}

