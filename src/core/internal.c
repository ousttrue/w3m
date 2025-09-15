#include "internal.h"
#include "MapArea.h"
#include "alloc.h"
#include "html_quote.h"
#include "rc.h"
#include "cookie.h"
#include "downloadlist.h"
#include "KeyValue.h"
#include "AnchorList.h"
#include "Anchor.h"
#include "html_form.h"
#include "LinkList.h"
#include "runtime.h"
#include "menu.h"
#include <stdlib.h>
#include <strings.h>

typedef void (*FormActionFunc)(struct UI ui, struct KeyValue*);

struct FormAction {
    const char* action;
    FormActionFunc rout;
};

static void change_charset(struct UI ui, struct KeyValue* arg)
{
    abort();
    // struct Buffer* buf = ui.current_buffer->linkBuffer[LB_N_INFO];
    // if (buf == NULL)
    //     return;
    // delBuffer(ui, ui.current_buffer);
    // ui.current_buffer = buf;
    // if (ui.current_buffer->bufferprop & BP_INTERNAL)
    //     return;
    // wc_ces charset;
    // charset = ui.current_buffer->document.charset;
    // for (; arg; arg = arg->next) {
    //     if (!strcmp(arg->arg, "charset"))
    //         charset = atoi(arg->value);
    // }
    // _docCSet(ui, charset);
}

struct FormAction internal_action[] = {
    { "map", follow_map },
    { "option", panel_set_option },
    { "cookie", set_cookie_flag },
    { "download", download_action },
    { "charset", change_charset },
    { "none", NULL },
    { NULL, NULL },
};

void do_internal(struct UI ui, const char* action, const char* data)
{
    for (int i = 0; internal_action[i].action; i++) {
        if (strcasecmp(internal_action[i].action, action) == 0) {
            if (internal_action[i].rout)
                internal_action[i].rout(ui, cgistr2tagarg(data));
            return;
        }
    }
}

struct Content link_list_panel(struct Document* doc)
{
    Str tmp = Strnew_charp("<title>Link List</title>\
<h1 align=center>Link List</h1>\n");

    if (doc) {
        if (doc->linklist) {
            Strcat_charp(tmp, "<hr><h2>Links</h2>\n<ol>\n");
            for (struct LinkList* l = doc->linklist; l; l = l->next) {
                const char* p;
                const char* u;
                const char* t;
                if (l->url) {
                    struct Url pu = parseUrl(l->url, makeBaseUrl(doc));
                    p = parsedURL2Str(&pu)->ptr;
                    const char* u = html_quote(p);
                    if (DecodeURL)
                        p = html_quote(url_decode2(p, doc->charset));
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

        if (doc->href) {
            Strcat_charp(tmp, "<hr><h2>Anchors</h2>\n<ol>\n");
            struct AnchorList* al = doc->href;
            for (int i = 0; i < al->nanchor; i++) {
                struct Anchor* a = &al->anchors[i];
                if (a->hseq < 0 || a->slave)
                    continue;
                struct Url pu = parseUrl(a->url, makeBaseUrl(doc));
                const char* p = parsedURL2Str(&pu)->ptr;
                const char* u = html_quote(p);
                if (DecodeURL)
                    p = html_quote(url_decode2(p, doc->charset));
                else
                    p = u;
                const char* t = getAnchorText(doc, al, a);
                t = t ? html_quote(t) : "";
                Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p,
                    "\n", 0);
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
                struct Url pu = parseUrl(a->url, makeBaseUrl(doc));
                const char* p = parsedURL2Str(&pu)->ptr;
                const char* u = html_quote(p);
                if (DecodeURL)
                    p = html_quote(url_decode2(p, doc->charset));
                else
                    p = u;
                const char* t;
                if (a->title && *a->title)
                    t = html_quote(a->title);
                else
                    t = html_quote(url_decode2(a->url, doc->charset));
                Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p,
                    "\n", 0);
                a = retrieveAnchor(doc->formitem, a->start);
                if (!a)
                    continue;
                struct FormItem* fi = (struct FormItem*)a->url;
                fi = fi->parent->item;
                if (fi->parent->method == FORM_METHOD_INTERNAL && !Strcmp_charp(fi->parent->action, "map") && fi->value) {
                    struct MapList* ml = searchMapList(doc, fi->value->ptr);
                    ListItem* mi;
                    struct MapArea* m;
                    if (!ml)
                        continue;
                    Strcat_charp(tmp, "<br>\n<b>Image map</b>\n<ol>\n");
                    for (mi = ml->area->first; mi != 0; mi = mi->next) {
                        m = (struct MapArea*)mi->ptr;
                        if (!m)
                            continue;
                        pu = parseUrl(m->url, makeBaseUrl(doc));
                        p = parsedURL2Str(&pu)->ptr;
                        u = html_quote(p);
                        if (DecodeURL)
                            p = html_quote(url_decode2(p, doc->charset));
                        else
                            p = u;
                        if (m->alt && *m->alt)
                            t = html_quote(m->alt);
                        else
                            t = html_quote(url_decode2(m->url, doc->charset));
                        Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t,
                            "</a><br>", p, "\n", 0);
                    }
                    Strcat_charp(tmp, "</ol>\n");
                }
            }
            Strcat_charp(tmp, "</ol>\n");
        }
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

struct LinkList* link_menu(struct UI ui, struct Document* doc)
{
    if (!doc->linklist)
        return NULL;

    int i = 0;
    struct LinkList* l = doc->linklist;
    for (; l; i++, l = l->next)
        ;
    int nitem = i;

    struct Menu menu;
    int len = 0, linkV = -1;
    Str str;

    const char** label = New_N(char*, nitem + 1);
    for (i = 0, l = doc->linklist; l; i++, l = l->next) {
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
            p = url_decode2(l->url, doc->charset);
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
    for (i = 0, l = doc->linklist; l; i++, l = l->next) {
        if (i == linkV)
            return l;
    }
    return NULL;
}
