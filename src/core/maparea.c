#include "maparea.h"
#include "runtime.h"
#include "Anchor.h"
#include "LinkList.h"
#include "str_util.h"
#include "html_quote.h"
#include "w3m.h"
#include "buffer_loader.h"
#include "display.h"
#include "alloc.h"
#include "form.h"
#include "menu.h"
#include "image.h"
#include "ctrlcode.h"
#include "buffer.h"
#include <math.h>
#include <stdlib.h>
#include <strings.h>

MapList*
searchMapList(struct Buffer* buf, const char* name)
{
    MapList* ml;

    if (name == NULL)
        return NULL;
    for (ml = buf->document.maplist; ml != NULL; ml = ml->next) {
        if (!Strcmp_charp(ml->name, name))
            break;
    }
    return ml;
}

#define SHAPE_UNKNOWN 0
#define SHAPE_DEFAULT 1
#define SHAPE_RECT 2
#define SHAPE_CIRCLE 3
#define SHAPE_POLY 4

static int
inMapArea(MapArea* a, int x, int y)
{
    int i;
    double r1, r2, s, c, t;

    if (!a)
        return false;
    switch (a->shape) {
    case SHAPE_RECT:
        if (x >= a->coords[0] && y >= a->coords[1] && x <= a->coords[2] && y <= a->coords[3])
            return true;
        break;
    case SHAPE_CIRCLE:
        if ((x - a->coords[0]) * (x - a->coords[0])
                + (y - a->coords[1]) * (y - a->coords[1])
            <= a->coords[2] * a->coords[2])
            return true;
        break;
    case SHAPE_POLY:
        for (t = 0, i = 0; i < a->ncoords; i += 2) {
            r1 = sqrt((double)(x - a->coords[i]) * (x - a->coords[i])
                + (double)(y - a->coords[i + 1]) * (y - a->coords[i + 1]));
            r2 = sqrt((double)(x - a->coords[i + 2]) * (x - a->coords[i + 2])
                + (double)(y - a->coords[i + 3]) * (y - a->coords[i + 3]));
            if (r1 == 0 || r2 == 0)
                return true;
            s = ((double)(x - a->coords[i]) * (y - a->coords[i + 3])
                    - (double)(x - a->coords[i + 2]) * (y - a->coords[i + 1]))
                / r1 / r2;
            c = ((double)(x - a->coords[i]) * (x - a->coords[i + 2])
                    + (double)(y - a->coords[i + 1]) * (y - a->coords[i + 3]))
                / r1 / r2;
            t += atan2(s, c);
        }
        if (fabs(t) > 2 * 3.14)
            return true;
        break;
    case SHAPE_DEFAULT:
        return true;
    default:
        break;
    }
    return false;
}

static int
nearestMapArea(MapList* ml, int x, int y)
{
    ListItem* al;
    MapArea* a;
    int i, l, n = -1, min = -1, limit = pixel_per_char * pixel_per_char + pixel_per_line * pixel_per_line;

    if (!ml || !ml->area)
        return n;
    for (i = 0, al = ml->area->first; al != NULL; i++, al = al->next) {
        a = (MapArea*)al->ptr;
        if (a) {
            l = (a->center_x - x) * (a->center_x - x)
                + (a->center_y - y) * (a->center_y - y);
            if ((min < 0 || l < min) && l < limit) {
                n = i;
                min = l;
            }
        }
    }
    return n;
}

int searchMapArea(struct Buffer* buf, MapList* ml, struct Anchor* a_img)
{
    ListItem* al;
    MapArea* a;
    int i, n;
    int px, py;

    if (!(ml && ml->area && ml->area->nitem))
        return -1;
    if (!getMapXY(buf, a_img, &px, &py))
        return -1;
    n = -ml->area->nitem;
    for (i = 0, al = ml->area->first; al != NULL; i++, al = al->next) {
        a = (MapArea*)al->ptr;
        if (!a)
            continue;
        if (n < 0 && inMapArea(a, px, py)) {
            if (a->shape == SHAPE_DEFAULT) {
                if (n == -ml->area->nitem)
                    n = -i;
            } else
                n = i;
        }
    }
    if (n == -ml->area->nitem)
        return nearestMapArea(ml, px, py);
    else if (n < 0)
        return -n;
    return n;
}

int getMapXY(struct Buffer* buf, struct Anchor* a, int* x, int* y)
{
    if (!buf || !a || !a->image || !x || !y)
        return 0;
    *x = (int)((buf->document.currentColumn /*+ buf->cursorX*/
                   - COLPOS(&currentLine(&buf->document)->l, a->start.pos) + 0.5)
             * pixel_per_char)
        - a->image->xoffset;
    *y = (int)((currentLine(&buf->document)->linenumber - a->image->y + 0.5)
             * pixel_per_line)
        - a->image->yoffset;
    if (*x <= 0)
        *x = 1;
    if (*y <= 0)
        *y = 1;
    return 1;
}

struct Anchor*
retrieveCurrentMap(struct Buffer* buf)
{
    struct Anchor* a;
    struct FormItem* fi;

    a = retrieveCurrentForm(buf);
    if (!a || !a->url)
        return NULL;
    fi = (struct FormItem*)a->url;
    if (fi->parent->method == FORM_METHOD_INTERNAL && !Strcmp_charp(fi->parent->action, "map"))
        return a;
    return NULL;
}

MapArea*
follow_map_menu(struct UI ui, struct Buffer* buf, const char* name, struct Anchor* a_img, int x, int y)
{
    MapList* ml;
    ListItem* al;
    int i, selected = -1;
    int initial = 0;
    MapArea* a;

    ml = searchMapList(buf, name);
    if (ml == NULL || ml->area == NULL || ml->area->nitem == 0)
        return NULL;

    initial = searchMapArea(buf, ml, a_img);
    if (initial < 0)
        initial = 0;
    else if (!image_map_list) {
        selected = initial;
        goto map_end;
    }

    const char** label;
    label = New_N(char*, ml->area->nitem + 1);
    for (i = 0, al = ml->area->first; al != NULL; i++, al = al->next) {
        a = (MapArea*)al->ptr;
        if (a)
            label[i] = *a->alt ? a->alt : a->url;
        else
            label[i] = "";
    }
    label[ml->area->nitem] = NULL;

    optionMenu(ui, x, y, label, &selected, initial, NULL);

map_end:
    if (selected >= 0) {
        for (i = 0, al = ml->area->first; al != NULL; i++, al = al->next) {
            if (al->ptr && i == selected)
                return (MapArea*)al->ptr;
        }
    }
    return NULL;
}

MapArea*
newMapArea(const char* url, const char* target, const char* alt, const char* shape, const char* coords)
{
    MapArea* a = New(MapArea);
    int i, max;

    a->url = url;
    a->target = target;
    a->alt = alt ? alt : "";
    a->shape = SHAPE_RECT;
    if (shape) {
        if (!strcasecmp(shape, "default"))
            a->shape = SHAPE_DEFAULT;
        else if (!strncasecmp(shape, "rect", 4))
            a->shape = SHAPE_RECT;
        else if (!strncasecmp(shape, "circ", 4))
            a->shape = SHAPE_CIRCLE;
        else if (!strncasecmp(shape, "poly", 4))
            a->shape = SHAPE_POLY;
        else
            a->shape = SHAPE_UNKNOWN;
    }
    a->coords = NULL;
    a->ncoords = 0;
    a->center_x = 0;
    a->center_y = 0;
    if (a->shape == SHAPE_UNKNOWN || a->shape == SHAPE_DEFAULT)
        return a;
    if (!coords) {
        a->shape = SHAPE_UNKNOWN;
        return a;
    }
    if (a->shape == SHAPE_RECT) {
        a->coords = New_N(short, 4);
        a->ncoords = 4;
    } else if (a->shape == SHAPE_CIRCLE) {
        a->coords = New_N(short, 3);
        a->ncoords = 3;
    }
    max = a->ncoords;
    const char* p;
    for (i = 0, p = coords; (a->shape == SHAPE_POLY || i < a->ncoords) && *p;) {
        while (IS_SPACE(*p))
            p++;
        if (!IS_DIGIT(*p) && *p != '-' && *p != '+')
            break;
        if (a->shape == SHAPE_POLY) {
            if (max <= i) {
                max = i ? i * 2 : 6;
                a->coords = New_Reuse(short, a->coords, max + 2);
            }
            a->ncoords++;
        }
        a->coords[i] = (short)atoi(p);
        i++;
        if (*p == '-' || *p == '+')
            p++;
        while (IS_DIGIT(*p))
            p++;
        if (*p != ',' && !IS_SPACE(*p))
            break;
        while (IS_SPACE(*p))
            p++;
        if (*p == ',')
            p++;
    }
    if (i != a->ncoords || (a->shape == SHAPE_POLY && a->ncoords < 6)) {
        a->shape = SHAPE_UNKNOWN;
        a->coords = NULL;
        a->ncoords = 0;
        return a;
    }
    if (a->shape == SHAPE_POLY) {
        a->ncoords = a->ncoords / 2 * 2;
        a->coords[a->ncoords] = a->coords[0];
        a->coords[a->ncoords + 1] = a->coords[1];
    }
    if (a->shape == SHAPE_CIRCLE) {
        a->center_x = a->coords[0];
        a->center_y = a->coords[1];
    } else {
        for (i = 0; i < a->ncoords / 2; i++) {
            a->center_x += a->coords[2 * i];
            a->center_y += a->coords[2 * i + 1];
        }
        a->center_x /= a->ncoords / 2;
        a->center_y /= a->ncoords / 2;
    }
    return a;
}

/* append image map links */
static void
append_map_info(struct Buffer* buf, Str tmp, struct FormItem* fi)
{
    MapList* ml = searchMapList(buf, fi->value ? fi->value->ptr : NULL);
    if (ml == NULL)
        return;

    Strcat_m_charp(tmp,
        "<tr valign=top><td colspan=2>Links of current image map",
        "<tr valign=top><td colspan=2><table>", NULL);
    ListItem* al;
    for (al = ml->area->first; al != NULL; al = al->next) {
        MapArea* a = (MapArea*)al->ptr;
        if (!a)
            continue;
        struct Url pu = parseUrl(a->url, baseURL(buf));
        const char* q = html_quote(parsedURL2Str(&pu)->ptr);
        const char* p = html_quote(url_decode2(a->url, buf ? buf->document.charset : 0));
        Strcat_m_charp(tmp, "<tr valign=top><td>&nbsp;&nbsp;<td><a href=\"",
            q, "\">",
            html_quote(*a->alt ? a->alt : mybasename(a->url)),
            "</a><td>", p, "\n", NULL);
    }
    Strcat_charp(tmp, "</table>");
}

/*
 * information of current page and link
 */
struct Content
page_info_panel(struct UI ui, struct Buffer* buf)
{
    Str tmp = Strnew_size(1024);
    Strcat_charp(tmp, "<html><head>\
<title>Information about current page</title>\
</head><body>\
<h1>Information about current page</h1>\n");
    if (buf == NULL)
        goto end;

    int all = buf->document.allLine;
    if (all == 0 && lastLine(&buf->document))
        all = lastLine(&buf->document)->linenumber;
    Strcat_charp(tmp, "<form method=internal action=charset>");

    const char* p = url_decode2(parsedURL2Str(&buf->content.url)->ptr, 0);
    Strcat_m_charp(tmp, "<table cellpadding=0>",
        "<tr valign=top><td nowrap>Title<td>",
        html_quote(buf->document.title),
        "<tr valign=top><td nowrap>Current URL<td>",
        html_quote(p),
        "<tr valign=top><td nowrap>Document Type<td>",
        contentTypeStr(buf->content.cc.content_type),
        "<tr valign=top><td nowrap>Last Modified<td>",
        html_quote(last_modified(buf)), NULL);

    if (buf->document.charset != InnerCharset) {
        wc_ces_list* list = wc_get_ces_list();
        Strcat_charp(tmp,
            "<tr><td nowrap>Document Charset<td><select name=charset>");
        for (; list->name != NULL; list++) {
            char charset[16];
            sprintf(charset, "%d", (unsigned int)list->id);
            Strcat_m_charp(tmp, "<option value=", charset,
                (buf->document.charset == list->id) ? " selected>"
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

    struct Anchor* a = retrieveCurrentAnchor(buf);
    if (a != NULL) {
        struct Url pu = parseUrl(a->url, baseURL(buf));
        p = parsedURL2Str(&pu)->ptr;
        const char* q = html_quote(p);
        if (DecodeURL)
            p = html_quote(url_decode2(p, buf ? buf->document.charset : 0));
        else
            p = q;
        Strcat_m_charp(tmp,
            "<tr valign=top><td nowrap>URL of current struct Anchor<td><a href=\"",
            q, "\">", p, "</a>", NULL);
    }
    a = retrieveCurrentImg(buf);
    if (a != NULL) {
        struct Url pu = parseUrl(a->url, baseURL(buf));
        p = parsedURL2Str(&pu)->ptr;
        const char* q = html_quote(p);
        if (DecodeURL)
            p = html_quote(url_decode2(p, buf ? buf->document.charset : 0));
        else
            p = q;
        Strcat_m_charp(tmp,
            "<tr valign=top><td nowrap>URL of current image<td><a href=\"",
            q, "\">", p, "</a>", NULL);
    }
    a = retrieveCurrentForm(buf);
    if (a != NULL) {
        struct FormItem* fi = (struct FormItem*)a->url;
        p = form2str(fi);
        p = html_quote(url_decode2(p, buf ? buf->document.charset : 0));
        Strcat_m_charp(tmp,
            "<tr valign=top><td nowrap>Method/type of current form&nbsp;<td>",
            p, NULL);
        if (fi->parent->method == FORM_METHOD_INTERNAL
            && !Strcmp_charp(fi->parent->action, "map"))
            append_map_info(buf, tmp, fi->parent->item);
    }
    Strcat_charp(tmp, "</table>\n");
    Strcat_charp(tmp, "</form>");

    append_link_info(buf, tmp, buf->document.linklist);

    if (buf->document_header != NULL) {
        Strcat_charp(tmp, "<hr width=50%><h1>Header information</h1><pre>\n");
        TextListItem* ti;
        for (ti = buf->document_header->first; ti != NULL; ti = ti->next)
            Strcat_m_charp(tmp, "<pre_int>", html_quote(ti->ptr),
                "</pre_int>\n", NULL);
        Strcat_charp(tmp, "</pre>\n");
    }

    if (buf->ssl_certificate)
        Strcat_m_charp(tmp, "<h1>SSL certificate</h1><pre>\n",
            html_quote(buf->ssl_certificate), "</pre>\n", NULL);
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
