#include "LinkList.h"
#include "Document.h"
#include "HtmlTagParsed.h"
#include "quote.h"
#include "alloc.h"

void addLink(struct Document* doc, struct HtmlTagParsed* tag)
{
    char *title = NULL, *ctype = NULL, *rel = NULL, *rev = NULL;
    enum LinkType type = LINK_TYPE_NONE;

    const char* href;
    parsedtag_get_value(tag, ATTR_HREF, &href);
    if (href) {
        href = url_quote(remove_space(href));
    }
    parsedtag_get_value(tag, ATTR_TITLE, &title);
    parsedtag_get_value(tag, ATTR_TYPE, &ctype);
    parsedtag_get_value(tag, ATTR_REL, &rel);
    if (rel != NULL) {
        /* forward link type */
        type = LINK_TYPE_REL;
        if (title == NULL)
            title = rel;
    }
    parsedtag_get_value(tag, ATTR_REV, &rev);
    if (rev != NULL) {
        /* reverse link type */
        type = LINK_TYPE_REV;
        if (title == NULL)
            title = rev;
    }

    struct LinkList* l = New(struct LinkList);
    l->url = href;
    l->title = title;
    l->ctype = ctype;
    l->type = type;
    l->next = NULL;
    if (doc->linklist) {
        struct LinkList* i;
        for (i = doc->linklist; i->next; i = i->next)
            ;
        i->next = l;
    } else {
        doc->linklist = l;
    }
}
