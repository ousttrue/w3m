#include "html_form.h"
#include <strings.h>

static const char* _formtypetbl[] = {
    "text", "password", "checkbox", "radio", "submit", "reset", "hidden",
    "image", "select", "textarea", "button", "file", NULL
};

static const char* _formmethodtbl[] = {
    "GET", "POST", "INTERNAL", "HEAD"
};

enum FormItemType formtype(const char* typestr)
{
    for (int i = 0; _formtypetbl[i]; i++) {
        if (strcasecmp(typestr, _formtypetbl[i]) == 0)
            return i;
    }
    return FORM_INPUT_TEXT;
}

const char* form2str(struct FormItem* fi)
{
    Str tmp = Strnew();
    if (fi->type != FORM_SELECT && fi->type != FORM_TEXTAREA)
        Strcat_charp(tmp, "input type=");
    Strcat_charp(tmp, _formtypetbl[fi->type]);
    if (fi->name && fi->name->length)
        Strcat_m_charp(tmp, " name=\"", fi->name->ptr, "\"", NULL);
    if ((fi->type == FORM_INPUT_RADIO || fi->type == FORM_INPUT_CHECKBOX || fi->type == FORM_SELECT) && fi->value)
        Strcat_m_charp(tmp, " value=\"", fi->value->ptr, "\"", NULL);
    Strcat_m_charp(tmp, " (", _formmethodtbl[fi->parent->method], " ",
        fi->parent->action->ptr, ")", NULL);
    return tmp->ptr;
}
