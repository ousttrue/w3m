/* $Id: map.c,v 1.30 2003/09/24 18:49:00 ukai Exp $ */
/*
 * client-side image maps
 */
#include "fm.h"
#include <math.h>

MapList *
searchMapList(Buffer *buf, char *name)
{
    MapList *ml;

    if (name == NULL)
	return NULL;
    for (ml = buf->maplist; ml != NULL; ml = ml->next) {
	if (!Strcmp_charp(ml->name, name))
	    break;
    }
    return ml;
}


Anchor *
retrieveCurrentMap(Buffer *buf)
{
    Anchor *a;
    FormItemList *fi;

    a = retrieveCurrentForm(buf);
    if (!a || !a->url)
	return NULL;
    fi = (FormItemList *)a->url;
    if (fi->parent->method == FORM_METHOD_INTERNAL &&
	!Strcmp_charp(fi->parent->action, "map"))
	return a;
    return NULL;
}

char *map1 = "<HTML><HEAD><TITLE>Image map links</TITLE></HEAD>\
<BODY><H1>Image map links</H1>\
<table>";

Buffer *
follow_map_panel(Buffer *buf, char *name)
{
    Str mappage;
    MapList *ml;
    ListItem *al;
    MapArea *a;
    ParsedURL pu;
    char *p, *q;
    Buffer *newbuf;

    ml = searchMapList(buf, name);
    if (ml == NULL)
	return NULL;

    mappage = Strnew_charp(map1);
    for (al = ml->area->first; al != NULL; al = al->next) {
	a = (MapArea *) al->ptr;
	if (!a)
	    continue;
	parseURL2(a->url, &pu, baseURL(buf));
	p = parsedURL2Str(&pu)->ptr;
	q = html_quote(p);
	if (DecodeURL)
	    p = html_quote(url_decode2(p, buf));
	else
	    p = q;
	Strcat_m_charp(mappage, "<tr valign=top><td><a href=\"", q, "\">",
		       html_quote(*a->alt ? a->alt : mybasename(a->url)),
		       "</a><td>", p, NULL);
    }
    Strcat_charp(mappage, "</table></body></html>");

    newbuf = loadHTMLString(mappage);
    return newbuf;
}

MapArea *
newMapArea(char *url, char *target, char *alt, char *shape, char *coords)
{
    MapArea *a = New(MapArea);

    a->url = url;
    a->target = target;
    a->alt = alt ? alt : "";
    return a;
}

/* append image map links */
static void
append_map_info(Buffer *buf, Str tmp, FormItemList *fi)
{
    MapList *ml;
    ListItem *al;
    MapArea *a;
    ParsedURL pu;
    char *p, *q;

    ml = searchMapList(buf, fi->value ? fi->value->ptr : NULL);
    if (ml == NULL)
	return;

    Strcat_m_charp(tmp,
		   "<tr valign=top><td colspan=2>Links of current image map",
		   "<tr valign=top><td colspan=2><table>", NULL);
    for (al = ml->area->first; al != NULL; al = al->next) {
	a = (MapArea *) al->ptr;
	if (!a)
	    continue;
	parseURL2(a->url, &pu, baseURL(buf));
	q = html_quote(parsedURL2Str(&pu)->ptr);
	p = html_quote(url_decode2(a->url, buf));
	Strcat_m_charp(tmp, "<tr valign=top><td>&nbsp;&nbsp;<td><a href=\"",
		       q, "\">",
		       html_quote(*a->alt ? a->alt : mybasename(a->url)),
		       "</a><td>", p, "\n", NULL);
    }
    Strcat_charp(tmp, "</table>");
}

/* append links */
static void
append_link_info(Buffer *buf, Str html, LinkList * link)
{
    LinkList *l;
    ParsedURL pu;
    char *url;

    if (!link)
	return;

    Strcat_charp(html, "<hr width=50%><h1>Link information</h1><table>\n");
    for (l = link; l; l = l->next) {
	if (l->url) {
	    parseURL2(l->url, &pu, baseURL(buf));
	    url = html_quote(parsedURL2Str(&pu)->ptr);
	}
	else
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
	    url = html_quote(url_decode2(l->url, buf));
	Strcat_m_charp(html, "<td>", url, NULL);
	if (l->ctype)
	    Strcat_m_charp(html, " (", html_quote(l->ctype), ")", NULL);
	Strcat_charp(html, "\n");
    }
    Strcat_charp(html, "</table>\n");
}

/* append frame URL */
static void
append_frame_info(Buffer *buf, Str html, struct frameset *set, int level)
{
    char *p, *q;
    int i, j;

    if (!set)
	return;

    for (i = 0; i < set->col * set->row; i++) {
	union frameset_element frame = set->frame[i];
	if (frame.element != NULL) {
	    switch (frame.element->attr) {
	    case F_UNLOADED:
	    case F_BODY:
		if (frame.body->url == NULL)
		    break;
		Strcat_charp(html, "<pre_int>");
		for (j = 0; j < level; j++)
		    Strcat_charp(html, "   ");
		q = html_quote(frame.body->url);
		Strcat_m_charp(html, "<a href=\"", q, "\">", NULL);
		if (frame.body->name) {
		    p = html_quote(url_unquote_conv(frame.body->name,
						    buf->document_charset));
		    Strcat_charp(html, p);
		}
		if (DecodeURL)
		    p = html_quote(url_decode2(frame.body->url, buf));
		else
		    p = q;
		Strcat_m_charp(html, " ", p, "</a></pre_int><br>\n", NULL);
		if (frame.body->ssl_certificate)
		    Strcat_m_charp(html,
				   "<blockquote><h2>SSL certificate</h2><pre>\n",
				   html_quote(frame.body->ssl_certificate),
				   "</pre></blockquote>\n", NULL);
		break;
	    case F_FRAMESET:
		append_frame_info(buf, html, frame.set, level + 1);
		break;
	    }
	}
    }
}

/* 
 * information of current page and link 
 */
Buffer *
page_info_panel(Buffer *buf)
{
    Str tmp = Strnew_size(1024);
    Anchor *a;
    ParsedURL pu;
    TextListItem *ti;
    struct frameset *f_set = NULL;
    int all;
    char *p, *q;
    Buffer *newbuf;

    Strcat_charp(tmp, "<html><head>\
<title>Information about current page</title>\
</head><body>\
<h1>Information about current page</h1>\n");
    if (buf == NULL)
	goto end;
    all = buf->allLine;
    if (all == 0 && buf->lastLine)
	all = buf->lastLine->linenumber;
    p = url_decode2(parsedURL2Str(&buf->currentURL)->ptr, NULL);
    Strcat_m_charp(tmp, "<table cellpadding=0>",
		   "<tr valign=top><td nowrap>Title<td>",
		   html_quote(buf->buffername),
		   "<tr valign=top><td nowrap>Current URL<td>",
		   html_quote(p),
		   "<tr valign=top><td nowrap>Document Type<td>",
		   buf->real_type ? html_quote(buf->real_type) : "unknown",
		   "<tr valign=top><td nowrap>Last Modified<td>",
		   html_quote(last_modified(buf)), NULL);
    Strcat_m_charp(tmp,
		   "<tr valign=top><td nowrap>Number of lines<td>",
		   Sprintf("%d", all)->ptr,
		   "<tr valign=top><td nowrap>Transferred bytes<td>",
		   Sprintf("%lu", (unsigned long)buf->trbyte)->ptr, NULL);

    a = retrieveCurrentAnchor(buf);
    if (a != NULL) {
	parseURL2(a->url, &pu, baseURL(buf));
	p = parsedURL2Str(&pu)->ptr;
	q = html_quote(p);
	if (DecodeURL)
	    p = html_quote(url_decode2(p, buf));
	else
	    p = q;
	Strcat_m_charp(tmp,
		       "<tr valign=top><td nowrap>URL of current anchor<td><a href=\"",
		       q, "\">", p, "</a>", NULL);
    }
    a = retrieveCurrentImg(buf);
    if (a != NULL) {
	parseURL2(a->url, &pu, baseURL(buf));
	p = parsedURL2Str(&pu)->ptr;
	q = html_quote(p);
	if (DecodeURL)
	    p = html_quote(url_decode2(p, buf));
	else
	    p = q;
	Strcat_m_charp(tmp,
		       "<tr valign=top><td nowrap>URL of current image<td><a href=\"",
		       q, "\">", p, "</a>", NULL);
    }
    a = retrieveCurrentForm(buf);
    if (a != NULL) {
	FormItemList *fi = (FormItemList *)a->url;
	p = form2str(fi);
	p = html_quote(url_decode2(p, buf));
	Strcat_m_charp(tmp,
		       "<tr valign=top><td nowrap>Method/type of current form&nbsp;<td>",
		       p, NULL);
	if (fi->parent->method == FORM_METHOD_INTERNAL
	    && !Strcmp_charp(fi->parent->action, "map"))
	    append_map_info(buf, tmp, fi->parent->item);
    }
    Strcat_charp(tmp, "</table>\n");

    append_link_info(buf, tmp, buf->linklist);

    if (buf->document_header != NULL) {
	Strcat_charp(tmp, "<hr width=50%><h1>Header information</h1><pre>\n");
	for (ti = buf->document_header->first; ti != NULL; ti = ti->next)
	    Strcat_m_charp(tmp, "<pre_int>", html_quote(ti->ptr),
			   "</pre_int>\n", NULL);
	Strcat_charp(tmp, "</pre>\n");
    }

    if (buf->frameset != NULL)
	f_set = buf->frameset;
    else if (buf->bufferprop & BP_FRAME &&
	     buf->nextBuffer != NULL && buf->nextBuffer->frameset != NULL)
	f_set = buf->nextBuffer->frameset;

    if (f_set) {
	Strcat_charp(tmp, "<hr width=50%><h1>Frame information</h1>\n");
	append_frame_info(buf, tmp, f_set, 0);
    }
    if (buf->ssl_certificate)
	Strcat_m_charp(tmp, "<h1>SSL certificate</h1><pre>\n",
		       html_quote(buf->ssl_certificate), "</pre>\n", NULL);
  end:
    Strcat_charp(tmp, "</body></html>");
    newbuf = loadHTMLString(tmp);
    return newbuf;
}
