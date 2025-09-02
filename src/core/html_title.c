#include "html_title.h"
#include "indep.h"

static Str cur_title;
static Str pre_title;

void init_title()
{
    cur_title = NULL;
    pre_title = NULL;
}

Str process_title(struct parsed_tag* tag)
{
    if (pre_title)
        return 0;
    cur_title = Strnew();
    return NULL;
}

Str process_n_title(struct parsed_tag* tag)
{
    Str tmp;

    if (pre_title)
        return NULL;
    if (!cur_title)
        return NULL;
    Strremovefirstspaces(cur_title);
    Strremovetrailingspaces(cur_title);
    tmp = Strnew_m_charp("<title_alt title=\"",
        html_quote(cur_title->ptr), "\">", NULL);
    pre_title = cur_title;
    cur_title = NULL;
    return tmp;
}

void feed_title(char* str)
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
