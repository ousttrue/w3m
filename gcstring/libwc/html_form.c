#include "html_form.h"
#include "alloc.h"
#include <wc.h>
#include <strings.h>
#include <stdbool.h>

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

struct Form*
newFormList(const char* action, const char* method, const char* charset, const char* enctype,
    const char* target, const char* name, struct Form* _next)
{
    struct Form* l;
    Str a = Strnew_charp(action);
    int m = FORM_METHOD_GET;
    int e = FORM_ENCTYPE_URLENCODED;
    wc_ces c = 0;

    if (method == NULL || !strcasecmp(method, "get"))
        m = FORM_METHOD_GET;
    else if (!strcasecmp(method, "post"))
        m = FORM_METHOD_POST;
    else if (!strcasecmp(method, "internal"))
        m = FORM_METHOD_INTERNAL;
    /* unknown method is regarded as 'get' */

    if (m != FORM_METHOD_GET && enctype != NULL && !strcasecmp(enctype, "multipart/form-data")) {
        e = FORM_ENCTYPE_MULTIPART;
    }

    if (charset != NULL)
        c = wc_guess_charset(charset, 0);

    l = New(struct Form);
    l->item = l->lastitem = NULL;
    l->action = a;
    l->method = m;
    l->charset = c;
    l->enctype = e;
    l->target = target;
    l->name = name;
    l->next = _next;
    l->nitems = 0;
    l->body = NULL;
    l->length = 0;
    return l;
}

void chooseSelectOption(struct FormItem* fi, struct FormSelectOptionItem* item)
{
    fi->selected = 0;
    if (item == NULL) {
        fi->value = Strnew_size(0);
        fi->label = Strnew_size(0);
        return;
    }
    fi->value = item->value;
    fi->label = item->label;

    struct FormSelectOptionItem* opt = item;
    for (int i = 0; opt != NULL; i++, opt = opt->next) {
        if (opt->checked) {
            fi->value = opt->value;
            fi->label = opt->label;
            fi->selected = i;
            break;
        }
    }
    updateSelectOption(fi, item);
}

void updateSelectOption(struct FormItem* fi, struct FormSelectOptionItem* item)
{
    if (fi == NULL || item == NULL)
        return;
    for (int i = 0; item != NULL; i++, item = item->next) {
        if (i == fi->selected)
            item->checked = true;
        else
            item->checked = false;
    }
}

void addSelectOption(struct FormSelectOption* fso, Str value, Str label, int chk)
{
    struct FormSelectOptionItem* o = New(struct FormSelectOptionItem);
    if (value == NULL)
        value = label;
    o->value = value;
    Strremovefirstspaces(label);
    Strremovetrailingspaces(label);
    o->label = label;
    o->checked = chk;
    o->next = NULL;
    if (fso->first == NULL)
        fso->first = fso->last = o;
    else {
        fso->last->next = o;
        fso->last = o;
    }
}
