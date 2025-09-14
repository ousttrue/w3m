#include "page_info.h"
#include "Content.h"
#include "Document.h"
#include "AnchorList.h"
#include "Buffer.h"
#include "runtime.h"
#include "html_quote.h"
#include "ui.h"
#include "Anchor.h"
#include "form.h"
#include "maparea.h"
#include "str_util.h"
#include "LinkList.h"
#include <strings.h>

static void append_map_info(struct Document* doc, Str tmp, struct FormItem* fi)
{
    struct MapList* ml = searchMapList(doc, fi->value ? fi->value->ptr : NULL);
    if (ml == NULL)
        return;

    Strcat_m_charp(tmp,
        "<tr valign=top><td colspan=2>Links of current image map",
        "<tr valign=top><td colspan=2><table>", NULL);
    ListItem* al;
    for (al = ml->area->first; al != NULL; al = al->next) {
        struct MapArea* a = (struct MapArea*)al->ptr;
        if (!a)
            continue;
        struct Url pu = parseUrl(a->url, makeBaseUrl(doc));
        const char* q = html_quote(parsedURL2Str(&pu)->ptr);
        const char* p = html_quote(url_decode2(a->url, doc ? doc->charset : 0));
        Strcat_m_charp(tmp, "<tr valign=top><td>&nbsp;&nbsp;<td><a href=\"",
            q, "\">",
            html_quote(*a->alt ? a->alt : mybasename(a->url)),
            "</a><td>", p, "\n", NULL);
    }
    Strcat_charp(tmp, "</table>");
}

static void append_link_info(struct Document* doc, Str html, struct LinkList* link)
{
    if (!link)
        return;

    Strcat_charp(html, "<hr width=50%><h1>Link information</h1><table>\n");
    for (struct LinkList* l = link; l; l = l->next) {
        const char* url;
        if (l->url) {
            struct Url pu = parseUrl(l->url, makeBaseUrl(doc));
            url = html_quote(parsedURL2Str(&pu)->ptr);
        } else
            url = "(empty)";
        Strcat_m_charp(html, "<tr valign=top><td><a href=\"", url, "\">",
            l->title ? html_quote(l->title) : "(empty)", "</a><td>",
            NULL);
        if (l->type == LINK_TYPE_REL)
            Strcat_charp(html, "[Rel]");
        else if (l->type == LINK_TYPE_REV)
            Strcat_charp(html, "[Rev]");
        if (!l->url)
            url = "(empty)";
        else
            url = html_quote(url_decode2(l->url, doc ? doc->charset : 0));
        Strcat_m_charp(html, "<td>", url, NULL);
        if (l->ctype)
            Strcat_m_charp(html, " (", html_quote(l->ctype), ")", NULL);
        Strcat_charp(html, "\n");
    }
    Strcat_charp(html, "</table>\n");
}

struct Content
page_info_panel(struct Content* content, struct Document* doc, struct BufferPoint bp)
{
    Str tmp = Strnew_size(1024);
    Strcat_charp(tmp, "<html><head>\
<title>Information about current page</title>\
</head><body>\
<h1>Information about current page</h1>\n");

    if (!content || !doc)
        goto end;

    int all = doc->allLine;
    if (all == 0 && lastLine(doc))
        all = lastLine(doc)->linenumber;
    Strcat_charp(tmp, "<form method=internal action=charset>");

    const char* p = url_decode2(parsedURL2Str(&doc->url)->ptr, 0);
    Strcat_m_charp(tmp, "<table cellpadding=0>",
        "<tr valign=top><td nowrap>Title<td>",
        html_quote(doc->title),
        "<tr valign=top><td nowrap>Current URL<td>",
        html_quote(p),
        "<tr valign=top><td nowrap>Document Type<td>",
        contentTypeStr(content->cc.content_type),
        "<tr valign=top><td nowrap>Last Modified<td>",
        html_quote(last_modified(content)), NULL);

    if (doc->charset != InnerCharset) {
        wc_ces_list* list = wc_get_ces_list();
        Strcat_charp(tmp,
            "<tr><td nowrap>Document Charset<td><select name=charset>");
        for (; list->name != NULL; list++) {
            char charset[16];
            sprintf(charset, "%d", (unsigned int)list->id);
            Strcat_m_charp(tmp, "<option value=", charset,
                (doc->charset == list->id) ? " selected>"
                                           : ">",
                list->desc, NULL);
        }
        Strcat_charp(tmp, "</select>");
        Strcat_charp(tmp, "<tr><td><td><input type=submit value=Change>");
    }
    Strcat_m_charp(tmp,
        "<tr valign=top><td nowrap>Number of lines<td>",
        Sprintf("%d", all)->ptr,
        "<tr valign=top><td nowrap>Transferred bytes<td>",
        // Sprintf("%lu", (unsigned long)buf->trbyte)->ptr,
        NULL);

    struct Anchor* a = retrieveAnchor(doc->href, bp);
    if (a != NULL) {
        struct Url pu = parseUrl(a->url, makeBaseUrl(doc));
        p = parsedURL2Str(&pu)->ptr;
        const char* q = html_quote(p);
        if (DecodeURL)
            p = html_quote(url_decode2(p, doc ? doc->charset : 0));
        else
            p = q;
        Strcat_m_charp(tmp,
            "<tr valign=top><td nowrap>URL of current struct Anchor<td><a href=\"",
            q, "\">", p, "</a>", NULL);
    }
    a = retrieveAnchor(doc->img, bp);
    if (a != NULL) {
        struct Url pu = parseUrl(a->url, makeBaseUrl(doc));
        p = parsedURL2Str(&pu)->ptr;
        const char* q = html_quote(p);
        if (DecodeURL)
            p = html_quote(url_decode2(p, doc ? doc->charset : 0));
        else
            p = q;
        Strcat_m_charp(tmp,
            "<tr valign=top><td nowrap>URL of current image<td><a href=\"",
            q, "\">", p, "</a>", NULL);
    }
    a = retrieveAnchor(doc->formitem, bp);
    if (a != NULL) {
        struct FormItem* fi = (struct FormItem*)a->url;
        p = form2str(fi);
        p = html_quote(url_decode2(p, doc ? doc->charset : 0));
        Strcat_m_charp(tmp,
            "<tr valign=top><td nowrap>Method/type of current form&nbsp;<td>",
            p, NULL);
        if (fi->parent->method == FORM_METHOD_INTERNAL
            && !Strcmp_charp(fi->parent->action, "map"))
            append_map_info(doc, tmp, fi->parent->item);
    }
    Strcat_charp(tmp, "</table>\n");
    Strcat_charp(tmp, "</form>");

    append_link_info(doc, tmp, doc->linklist);

    if (content->document_header) {
        Strcat_charp(tmp, "<hr width=50%><h1>Header information</h1><pre>\n");
        for (TextListItem* ti = content->document_header->first; ti != NULL; ti = ti->next)
            Strcat_m_charp(tmp, "<pre_int>", html_quote(ti->ptr),
                "</pre_int>\n", NULL);
        Strcat_charp(tmp, "</pre>\n");
    }

    if (content->ssl_certificate)
        Strcat_m_charp(tmp, "<h1>SSL certificate</h1><pre>\n",
            html_quote(content->ssl_certificate), "</pre>\n", NULL);
end:
    Strcat_charp(tmp, "</body></html>");
    return (struct Content) {
        .url = {},
        .page = tmp,
        .cc = {
            .content_type = CONTENTTYPE_TEXT_HTML,
            .charset = WC_CES_UTF_8,
        },
    };
}
