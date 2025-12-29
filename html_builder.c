#include "html_builder.h"
#include "indep.h"
#include "line.h"

Str process_title(struct HtmlBuilder *hb, struct HtmlTag* tag)
{
    if (hb->pre_title)
        return NULL;
    hb->cur_title = Strnew();
    return NULL;
}

void feed_title(struct HtmlBuilder *hb, const char* str)
{
    if (hb->pre_title)
        return;
    if (!hb->cur_title)
        return;
    while (*str) {
        if (*str == '&')
            Strcat_charp(hb->cur_title, getescapecmd(&str));
        else if (*str == '\n' || *str == '\r') {
            Strcat_char(hb->cur_title, ' ');
            str++;
        } else
            Strcat_char(hb->cur_title, *(str++));
    }
}

Str process_n_title(struct HtmlBuilder *hb, struct HtmlTag* tag)
{
    if (hb->pre_title)
        return NULL;
    if (!hb->cur_title)
        return NULL;
    Strremovefirstspaces(hb->cur_title);
    Strremovetrailingspaces(hb->cur_title);
    Str tmp = Strnew_m_charp("<title_alt title=\"",
        html_quote(hb->cur_title->ptr), "\">", NULL);
    hb->pre_title = hb->cur_title;
    hb->cur_title = NULL;
    return tmp;
}
