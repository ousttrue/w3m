#include "LinkList.h"
#include "runtime.h"
#include "AnchorList.h"
#include "Anchor.h"
#include "HtmlTagParsed.h"
#include "buffer.h"
#include "buffer_loader.h"
#include "html_quote.h"
#include "maparea.h"
#include "menu.h"
#include "html_form.h"
#include "quote.h"
#include "w3m.h"
#include "alloc.h"

struct LinkList*
link_menu(struct UI ui)
{
    struct Menu menu;
    struct LinkList* l;
    int i, nitem, len = 0, linkV = -1;
    Str str;

    if (!ui.current_buffer->document.linklist)
        return NULL;

    for (i = 0, l = ui.current_buffer->document.linklist; l; i++, l = l->next)
        ;
    nitem = i;

    const char** label;
    label = New_N(char*, nitem + 1);
    for (i = 0, l = ui.current_buffer->document.linklist; l; i++, l = l->next) {
        str = Strnew_charp(l->title ? l->title : "(empty)");
        if (l->type == LINK_TYPE_REL)
            Strcat_charp(str, " [Rel] ");
        else if (l->type == LINK_TYPE_REV)
            Strcat_charp(str, " [Rev] ");
        else
            Strcat_charp(str, " ");
        const char* p;
        if (!l->url)
            p = "";
        else
            p = url_decode2(l->url, ui.current_buffer ? ui.current_buffer->document.charset : 0);
        Strcat_charp(str, p);
        label[i] = str->ptr;
        if (len < str->length)
            len = str->length;
    }
    label[nitem] = NULL;

    set_menu_frame();
    new_option_menu(&menu, label, &linkV, NULL);

    menu.initial = 0;
    // menu.cursorX = ui.current_buffer->cursorX;
    // menu.cursorY = ui.current_buffer->cursorY;
    menu.x = /*menu.cursorX +*/ FRAME_WIDTH + 1;
    menu.y = /*menu.cursorY +*/ 2;

    popup_menu(ui, NULL, &menu);

    if (linkV < 0)
        return NULL;
    for (i = 0, l = ui.current_buffer->document.linklist; l; i++, l = l->next) {
        if (i == linkV)
            return l;
    }
    return NULL;
}

struct Content
link_list_panel(struct UI ui, struct Buffer* buf)
{
    // if (buf->bufferprop & BP_INTERNAL || (buf->document.linklist == 0 && buf->document.href == 0 && buf->document.img == 0)) {
    //     return (struct Content) {};
    // }

    Str tmp = Strnew_charp("<title>Link List</title>\
<h1 align=center>Link List</h1>\n");

    if (buf->document.linklist) {
        Strcat_charp(tmp, "<hr><h2>Links</h2>\n<ol>\n");
        for (struct LinkList* l = buf->document.linklist; l; l = l->next) {
            const char* p;
            const char* u;
            const char* t;
            if (l->url) {
                struct Url pu = parseUrl(l->url, baseURL(buf));
                p = parsedURL2Str(&pu)->ptr;
                const char* u = html_quote(p);
                if (DecodeURL)
                    p = html_quote(url_decode2(p, buf ? buf->document.charset : 0));
                else
                    p = u;
            } else
                u = p = "";
            if (l->type == LINK_TYPE_REL)
                t = " [Rel]";
            else if (l->type == LINK_TYPE_REV)
                t = " [Rev]";
            else
                t = "";
            t = Sprintf("%s%s\n", l->title ? l->title : "", t)->ptr;
            t = html_quote(t);
            Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p,
                "\n", 0);
        }
        Strcat_charp(tmp, "</ol>\n");
    }

    if (buf->document.href) {
        Strcat_charp(tmp, "<hr><h2>Anchors</h2>\n<ol>\n");
        struct AnchorList* al = buf->document.href;
        for (int i = 0; i < al->nanchor; i++) {
            struct Anchor* a = &al->anchors[i];
            if (a->hseq < 0 || a->slave)
                continue;
            struct Url pu = parseUrl(a->url, baseURL(buf));
            const char* p = parsedURL2Str(&pu)->ptr;
            const char* u = html_quote(p);
            if (DecodeURL)
                p = html_quote(url_decode2(p, buf ? buf->document.charset : 0));
            else
                p = u;
            const char* t = getAnchorText(buf, al, a);
            t = t ? html_quote(t) : "";
            Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p,
                "\n", 0);
        }
        Strcat_charp(tmp, "</ol>\n");
    }

    if (buf->document.img) {
        Strcat_charp(tmp, "<hr><h2>Images</h2>\n<ol>\n");
        struct AnchorList* al = buf->document.img;
        for (int i = 0; i < al->nanchor; i++) {
            struct Anchor* a = &al->anchors[i];
            if (a->slave)
                continue;
            struct Url pu = parseUrl(a->url, baseURL(buf));
            const char* p = parsedURL2Str(&pu)->ptr;
            const char* u = html_quote(p);
            if (DecodeURL)
                p = html_quote(url_decode2(p, buf ? buf->document.charset : 0));
            else
                p = u;
            const char* t;
            if (a->title && *a->title)
                t = html_quote(a->title);
            else
                t = html_quote(url_decode2(a->url, buf ? buf->document.charset : 0));
            Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p,
                "\n", 0);
            a = retrieveAnchor(buf->document.formitem, a->start);
            if (!a)
                continue;
            struct FormItem* fi = (struct FormItem*)a->url;
            fi = fi->parent->item;
            if (fi->parent->method == FORM_METHOD_INTERNAL && !Strcmp_charp(fi->parent->action, "map") && fi->value) {
                struct MapList* ml = searchMapList(&buf->document, fi->value->ptr);
                ListItem* mi;
                struct MapArea* m;
                if (!ml)
                    continue;
                Strcat_charp(tmp, "<br>\n<b>Image map</b>\n<ol>\n");
                for (mi = ml->area->first; mi != 0; mi = mi->next) {
                    m = (struct MapArea*)mi->ptr;
                    if (!m)
                        continue;
                    pu = parseUrl(m->url, baseURL(buf));
                    p = parsedURL2Str(&pu)->ptr;
                    u = html_quote(p);
                    if (DecodeURL)
                        p = html_quote(url_decode2(p, buf ? buf->document.charset : 0));
                    else
                        p = u;
                    if (m->alt && *m->alt)
                        t = html_quote(m->alt);
                    else
                        t = html_quote(url_decode2(m->url, buf ? buf->document.charset : 0));
                    Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t,
                        "</a><br>", p, "\n", 0);
                }
                Strcat_charp(tmp, "</ol>\n");
            }
        }
        Strcat_charp(tmp, "</ol>\n");
    }

    return (struct Content) {
        .url = {},
        .page = tmp,
        .cc = {
            .content_type = CONTENTTYPE_TEXT_HTML,
            .charset = WC_CES_UTF_8,
        },
    };
}

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
