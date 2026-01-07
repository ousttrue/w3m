#include "maparea.h"
#include "menu.h"
#include "alloc.h"
#include "etc.h"
#include "indep.h"
#include "w3m_rc.h"
#include "url.h"
#include "document.h"
#include "html_form.h"
#include "myctype.h"
#include <libwc/charset.h>
#include <math.h>
#include <strings.h>

struct MapList* searchMapList(struct Document* doc, const char* name)
{
    if (name == NULL)
        return NULL;
    struct MapList* ml = doc->maplist;
    for (; ml != NULL; ml = ml->next) {
        if (!Strcmp_charp(ml->name, name))
            break;
    }
    return ml;
}

static int
inMapArea(struct MapArea* a, int x, int y)
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
nearestMapArea(struct MapList* ml, int x, int y)
{
    ListItem* al;
    struct MapArea* a;
    int i, l, n = -1, min = -1, limit = getRuntime()->pixel_per_char * getRuntime()->pixel_per_char + getRuntime()->pixel_per_line * getRuntime()->pixel_per_line;

    if (!ml || !ml->area)
        return n;
    for (i = 0, al = ml->area->first; al != NULL; i++, al = al->next) {
        a = (struct MapArea*)al->ptr;
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

int searchMapArea(struct Document* doc, struct MapList* ml, struct Anchor* a_img)
{
    if (!(ml && ml->area && ml->area->nitem))
        return -1;
    int px, py;
    if (!getMapXY(doc, a_img, &px, &py))
        return -1;

    int n = -ml->area->nitem;
    int i = 0;
    for (ListItem* al = ml->area->first; al != NULL; i++, al = al->next) {
        struct MapArea* a = (struct MapArea*)al->ptr;
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

int getMapXY(struct Document* doc, struct Anchor* a, int* x, int* y)
{
    if (!doc || !a || !a->image || !x || !y)
        return 0;
    *x = (int)((doc->currentColumn + doc->cursorX
                   - COLPOS(doc->currentLine, a->start.pos) + 0.5)
             * getRuntime()->pixel_per_char)
        - a->image->xoffset;
    *y = (int)((doc->currentLine->linenumber - a->image->y + 0.5)
             * getRuntime()->pixel_per_line)
        - a->image->yoffset;
    if (*x <= 0)
        *x = 1;
    if (*y <= 0)
        *y = 1;
    return 1;
}

struct MapArea*
follow_map_menu(struct Document* doc, const char* name, struct Anchor* a_img, int x, int y)
{
    struct MapList* ml = searchMapList(doc, name);
    if (ml == NULL || ml->area == NULL || ml->area->nitem == 0)
        return NULL;

    int selected = -1;
    int initial = searchMapArea(doc, ml, a_img);
    if (initial < 0)
        initial = 0;
    else if (!getRuntime()->image_map_list) {
        selected = initial;
        goto map_end;
    }

    const char** label = New_N(char*, ml->area->nitem + 1);
    int i = 0;
    for (ListItem* al = ml->area->first; al != NULL; i++, al = al->next) {
        struct MapArea* a = (struct MapArea*)al->ptr;
        if (a)
            label[i] = *a->alt ? a->alt : a->url;
        else
            label[i] = "";
    }
    label[ml->area->nitem] = NULL;

    optionMenu(x, y, label, &selected, initial, NULL);

map_end:
    if (selected >= 0) {
        i = 0;
        for (ListItem* al = ml->area->first; al != NULL; i++, al = al->next) {
            if (al->ptr && i == selected)
                return (struct MapArea*)al->ptr;
        }
    }
    return NULL;
}

struct MapArea*
newMapArea(const char* url, const char* target, const char* alt, const char* shape, const char* coords)
{
    struct MapArea* a = New(struct MapArea);
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

    const char* p;
    int i;
    int max = a->ncoords;
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
void append_map_info(struct Url* base_url, struct Document* doc, Str tmp, struct FormItemList* fi)
{
    struct MapList* ml = searchMapList(doc, fi->value ? fi->value->ptr : NULL);
    if (ml == NULL)
        return;

    Strcat_m_charp(tmp,
        "<tr valign=top><td colspan=2>Links of current image map",
        "<tr valign=top><td colspan=2><table>", NULL);

    for (ListItem* al = ml->area->first; al != NULL; al = al->next) {
        struct MapArea* a = (struct MapArea*)al->ptr;
        if (!a)
            continue;
        struct Url pu;
        parseURL2(a->url, &pu, base_url);
        const char* q = html_quote(parsedURL2Str(&pu)->ptr);
        const char* p = html_quote(url_decode2(base_url, doc, a->url));
        Strcat_m_charp(tmp, "<tr valign=top><td>&nbsp;&nbsp;<td><a href=\"",
            q, "\">",
            html_quote(*a->alt ? a->alt : mybasename(a->url)),
            "</a><td>", p, "\n", NULL);
    }
    Strcat_charp(tmp, "</table>");
}

/* append links */
void append_link_info(struct Url* base_url, struct Document* doc, Str html, struct LinkList* link)
{
    if (!link)
        return;

    Strcat_charp(html, "<hr width=50%><h1>Link information</h1><table>\n");
    for (struct LinkList* l = link; l; l = l->next) {
        char* url;
        if (l->url) {
            struct Url pu;
            parseURL2(l->url, &pu, base_url);
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
            url = html_quote(url_decode2(base_url, doc, l->url));
        Strcat_m_charp(html, "<td>", url, NULL);
        if (l->ctype)
            Strcat_m_charp(html, " (", html_quote(l->ctype), ")", NULL);
        Strcat_charp(html, "\n");
    }
    Strcat_charp(html, "</table>\n");
}


