#include "html_title.h"
#include "html_quote.h"
#include "entity.h"

static Str cur_title;
static Str pre_title;

void init_title()
{
    cur_title = 0;
    pre_title = 0;
}

Str process_title(struct HtmlTagParsed* tag)
{
    if (pre_title)
        return 0;
    cur_title = Strnew();
    return 0;
}

Str process_n_title(struct HtmlTagParsed* tag)
{
    Str tmp;

    if (pre_title)
        return 0;
    if (!cur_title)
        return 0;
    Strremovefirstspaces(cur_title);
    Strremovetrailingspaces(cur_title);
    tmp = Strnew_m_charp("<title_alt title=\"",
        html_quote(cur_title->ptr), "\">", 0);
    pre_title = cur_title;
    cur_title = 0;
    return tmp;
}

void feed_title(const char* str)
{
    if (pre_title)
        return;
    if (!cur_title)
        return;
    while (*str) {
        if (*str == '&')
            Strcat_charp(cur_title, getescapecmd(&str));
        else if (*str == '\n' || *str == '\r') {
            Strcat_char(cur_title, ' ');
            str++;
        } else
            Strcat_char(cur_title, *(str++));
    }
}
