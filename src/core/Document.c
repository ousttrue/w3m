#include "Document.h"
#include "Anchor.h"
#include "AnchorList.h"
#include "form.h"

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
