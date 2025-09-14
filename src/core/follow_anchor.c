#include "follow_anchor.h"
#include "AnchorList.h"
#include "alloc.h"
#include "buffer_list.h"
#include "history.h"
#include "linein.h"
#include "quote.h"
#include "runtime.h"
#include "url.h"
#include "buffer.h"
#include "Anchor.h"
#include "image.h"
#include "html_form.h"
#include "form.h"
#include "maparea.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int label_topline = (false);

static Str
conv_form_encoding(Str val, struct FormItem* fi, wc_ces document_charset)
{
    wc_ces charset = SystemCharset;
    if (fi->parent->charset)
        charset = fi->parent->charset;
    else if (document_charset && document_charset != WC_CES_US_ASCII)
        charset = document_charset;
    return wc_Str_conv_strict(val, InnerCharset, charset);
}

void query_from_followform(struct UI ui, Str* query, struct FormItem* fi, int multipart)
{
    FILE* body = NULL;
    if (multipart) {
        *query = tmpfname(TMPF_DFL, NULL);
        body = fopen((*query)->ptr, "w");
        if (body == NULL) {
            return;
        }
        fi->parent->body = (*query)->ptr;
        fi->parent->boundary = Sprintf("------------------------------%d%ld%ld%ld", CurrentPid,
            fi->parent, fi->parent->body, fi->parent->boundary)
                                   ->ptr;
    }

    *query = Strnew();
    struct FormItem* f2;
    for (f2 = fi->parent->item; f2; f2 = f2->next) {
        if (f2->name == NULL)
            continue;
        /* <ISINDEX> is translated into single text form */
        if (f2->name->length == 0 && (multipart || f2->type != FORM_INPUT_TEXT))
            continue;
        switch (f2->type) {
        case FORM_INPUT_RESET:
            /* do nothing */
            continue;
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_IMAGE:
            if (f2 != fi || f2->value == NULL)
                continue;
            break;
        case FORM_INPUT_RADIO:
        case FORM_INPUT_CHECKBOX:
            if (!f2->checked)
                continue;
        default:
            break;
        }
        if (multipart) {
            if (f2->type == FORM_INPUT_IMAGE) {
                int x = 0, y = 0;
                getMapXY(ui.document,
                    retrieveAnchor(ui.document->img, getBufferPosition(ui)), &x, &y);
                *query = Strdup(conv_form_encoding(f2->name, fi, ui.document->charset));
                Strcat_charp(*query, ".x");
                form_write_data(body, fi->parent->boundary, (*query)->ptr,
                    Sprintf("%d", x)->ptr);
                *query = Strdup(conv_form_encoding(f2->name, fi, ui.document->charset));
                Strcat_charp(*query, ".y");
                form_write_data(body, fi->parent->boundary, (*query)->ptr,
                    Sprintf("%d", y)->ptr);
            } else if (f2->name && f2->name->length > 0 && f2->value != NULL) {
                /* not IMAGE */
                *query = conv_form_encoding(f2->value, fi, ui.document->charset);
                if (f2->type == FORM_INPUT_FILE)
                    form_write_from_file(body, fi->parent->boundary,
                        conv_form_encoding(f2->name, fi,
                            ui.document->charset)
                            ->ptr,
                        (*query)->ptr,
                        Str_conv_to_system(f2->value)->ptr);
                else
                    form_write_data(body, fi->parent->boundary,
                        conv_form_encoding(f2->name, fi,
                            ui.document->charset)
                            ->ptr,
                        (*query)->ptr);
            }
        } else {
            /* not multipart */
            if (f2->type == FORM_INPUT_IMAGE) {
                int x = 0, y = 0;
                getMapXY(ui.document,
                    retrieveAnchor(ui.document->img, getBufferPosition(ui)), &x, &y);
                Strcat(*query,
                    Str_form_quote(conv_form_encoding(f2->name, fi, ui.document->charset)));
                Strcat(*query, Sprintf(".x=%d&", x));
                Strcat(*query,
                    Str_form_quote(conv_form_encoding(f2->name, fi, ui.document->charset)));
                Strcat(*query, Sprintf(".y=%d", y));
            } else {
                /* not IMAGE */
                if (f2->name && f2->name->length > 0) {
                    Strcat(*query,
                        Str_form_quote(conv_form_encoding(f2->name, fi, ui.document->charset)));
                    Strcat_char(*query, '=');
                }
                if (f2->value != NULL) {
                    if (fi->parent->method == FORM_METHOD_INTERNAL)
                        Strcat(*query, Str_form_quote(f2->value));
                    else {
                        Strcat(*query,
                            Str_form_quote(conv_form_encoding(f2->value, fi, ui.document->charset)));
                    }
                }
            }
            if (f2->next)
                Strcat_char(*query, '&');
        }
    }
    if (multipart) {
        fprintf(body, "--%s--\r\n", fi->parent->boundary);
        fclose(body);
    } else {
        /* remove trailing & */
        while (Strlastchar(*query) == '&')
            Strshrink(*query, 1);
    }
}

static struct Buffer*
loadLink(struct UI ui, const char* url, const char* target, const char* referer, struct Form* post, bool do_download)
{
    // message(ui, MSG_INFO, Sprintf("loading %s", url)->ptr);
    // refresh(ttyWriter());

    struct Url* base = makeBaseUrl(ui.document);
    // const int* no_referer_ptr;
    // if ((no_referer_ptr && *no_referer_ptr)
    //     || base == NULL
    //     || base->scheme == SCM_LOCAL
    //     || base->scheme == SCM_LOCAL_CGI
    //     || base->scheme == SCM_DATA)
    //     referer = NO_REFERER;
    // if (referer == NULL)
    //     referer = parsedURL2RefererStr(&ui.current_buffer->content.url)->ptr;

    struct Content c = loadGeneralFile(url, makeBaseUrl(ui.document), post, referer, UI_TTY);
    if (do_download) {
        if (!c.page)
            return NULL;
        const char* file = guessFileName(c.url.file);
        // doFileMove(tmp->ptr, file);
        abort();
        return NULL;
    }

    return pushContent(ui, c);
}

static struct FormItem* save_submit_formlist(struct FormItem* src)
{
    struct Form* list;
    struct Form* srclist;
    struct FormItem* srcitem;
    struct FormItem* item;
    struct FormItem* ret = NULL;
    struct FormSelectOptionItem* opt;
    struct FormSelectOptionItem* curopt;
    struct FormSelectOptionItem* srcopt;

    if (src == NULL)
        return NULL;
    srclist = src->parent;
    list = New(struct Form);
    list->method = srclist->method;
    list->action = Strdup(srclist->action);
    list->charset = srclist->charset;
    list->enctype = srclist->enctype;
    list->nitems = srclist->nitems;
    list->body = srclist->body;
    list->boundary = srclist->boundary;
    list->length = srclist->length;

    for (srcitem = srclist->item; srcitem; srcitem = srcitem->next) {
        item = New(struct FormItem);
        item->type = srcitem->type;
        item->name = Strdup(srcitem->name);
        item->value = Strdup(srcitem->value);
        item->checked = srcitem->checked;
        item->accept = srcitem->accept;
        item->size = srcitem->size;
        item->rows = srcitem->rows;
        item->maxlength = srcitem->maxlength;
        item->readonly = srcitem->readonly;
        opt = curopt = NULL;
        for (srcopt = srcitem->select_option; srcopt; srcopt = srcopt->next) {
            if (!srcopt->checked)
                continue;
            opt = New(struct FormSelectOptionItem);
            opt->value = Strdup(srcopt->value);
            opt->label = Strdup(srcopt->label);
            opt->checked = srcopt->checked;
            if (item->select_option == NULL) {
                item->select_option = curopt = opt;
            } else {
                curopt->next = opt;
                curopt = curopt->next;
            }
        }
        item->select_option = opt;
        if (srcitem->label)
            item->label = Strdup(srcitem->label);
        item->parent = list;
        item->next = NULL;

        if (list->lastitem == NULL) {
            list->item = list->lastitem = item;
        } else {
            list->lastitem->next = item;
            list->lastitem = item;
        }

        if (srcitem == src)
            ret = item;
    }

    return ret;
}

static void do_submit(struct UI ui, struct Anchor* a, struct FormItem* fi, bool do_download)
{
    Str tmp = Strnew();
    int multipart = (fi->parent->method == FORM_METHOD_POST && fi->parent->enctype == FORM_ENCTYPE_MULTIPART);
    query_from_followform(ui, &tmp, fi, multipart);

    Str tmp2 = Strdup(fi->parent->action);
    if (!Strcmp_charp(tmp2, "!CURRENT_URL!")) {
        /* It means "current URL" */
        tmp2 = parsedURL2Str(&ui.current_buffer->content.url);
        char* p;
        if ((p = strchr(tmp2->ptr, '?')) != NULL)
            Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
    }

    if (fi->parent->method == FORM_METHOD_GET) {
        char* p;
        if ((p = strchr(tmp2->ptr, '?')) != NULL)
            Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
        Strcat_charp(tmp2, "?");
        Strcat(tmp2, tmp);
        loadLink(ui, tmp2->ptr, (char*)a->target, NULL, NULL, do_download);
    } else if (fi->parent->method == FORM_METHOD_POST) {
        struct Buffer* buf;
        if (multipart) {
            struct stat st;
            stat(fi->parent->body, &st);
            fi->parent->length = st.st_size;
        } else {
            fi->parent->body = tmp->ptr;
            fi->parent->length = tmp->length;
        }
        buf = loadLink(ui, tmp2->ptr, (char*)a->target, NULL, fi->parent, do_download);
        if (multipart) {
            unlink(fi->parent->body);
        }
        // if (buf && !(buf->bufferprop & BP_REDIRECTED)) { /* buf must be ui.current_buffer */
        /* BP_REDIRECTED means that the buffer is obtained through
         * Location: header. In this case, buf->form_submit must not be set
         * because the page is not loaded by POST method but GET method.
         */
        buf->form_submit = save_submit_formlist(fi);
        // }
    } else if ((fi->parent->method == FORM_METHOD_INTERNAL && (!Strcmp_charp(fi->parent->action, "map") || !Strcmp_charp(fi->parent->action, "none")))
        // || ui.current_buffer->bufferprop & BP_INTERNAL
    ) { /* internal */
        do_internal(ui, tmp2->ptr, tmp->ptr);
    } else {
        message(ui, MSG_ERR, "Can't send form because of illegal method.");
    }
}

void _followForm(struct UI ui, bool submit, bool do_download)
{
    if (ui.document->firstLine == NULL)
        return;

    struct Anchor* a = retrieveAnchor(ui.document->formitem, getBufferPosition(ui));
    if (a == NULL)
        return;

    struct FormItem* fi = (struct FormItem*)a->url;
    switch (fi->type) {
    case FORM_INPUT_TEXT: {
        if (submit) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        if (fi->readonly)
            message(ui, MSG_INFO, "Read only field!");
        const char* p = inputStrHist(ui, "TEXT:", fi->value ? fi->value->ptr : NULL, TextHist);
        if (p == NULL || fi->readonly)
            break;
        fi->value = Strnew_charp(p);
        formUpdateBuffer(a, ui.current_buffer, fi);
        if (fi->accept || fi->parent->nitems == 1) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        break;
    }
    case FORM_INPUT_FILE: {
        if (submit) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        if (fi->readonly)
            message(ui, MSG_INFO, "Read only field!");
        const char* p = inputFilenameHist(ui, "Filename:", fi->value ? fi->value->ptr : NULL, NULL);
        if (p == NULL || fi->readonly)
            break;
        fi->value = Strnew_charp(p);
        formUpdateBuffer(a, ui.current_buffer, fi);
        if (fi->accept || fi->parent->nitems == 1) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        break;
    }
    case FORM_INPUT_PASSWORD: {
        if (submit) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        if (fi->readonly) {
            message(ui, MSG_INFO, "Read only field!");
            break;
        }
        const char* p = inputLine(ui, "Password:", fi->value ? fi->value->ptr : NULL,
            IN_PASSWORD);
        if (p == NULL)
            break;
        fi->value = Strnew_charp(p);
        formUpdateBuffer(a, ui.current_buffer, fi);
        if (fi->accept) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        break;
    }
    case FORM_TEXTAREA: {
        if (submit) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        if (fi->readonly) {
            message(ui, MSG_INFO, "Read only field!");
        }
        input_textarea(fi);
        formUpdateBuffer(a, ui.current_buffer, fi);
        break;
    }
    case FORM_INPUT_RADIO: {
        if (submit) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        if (fi->readonly) {
            message(ui, MSG_INFO, "Read only field!");
            break;
        }
        formRecheckRadio(ui, a, ui.current_buffer, fi);
        break;
    }
    case FORM_INPUT_CHECKBOX: {
        if (submit) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        if (fi->readonly) {
            /* FIXME: gettextize? */
            message(ui, MSG_INFO, "Read only field!");
            break;
        }
        fi->checked = !fi->checked;
        formUpdateBuffer(a, ui.current_buffer, fi);
        break;
    }
    case FORM_SELECT: {
        if (submit) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        if (!formChooseOptionByMenu(ui, fi,
                ui.viewport_cursor.x - ui.document->pos + a->start.pos,
                ui.viewport_cursor.y))
            break;
        formUpdateBuffer(a, ui.current_buffer, fi);
        if (fi->parent->nitems == 1) {
            do_submit(ui, a, fi, do_download);
            return;
        }
        break;
    }
    case FORM_INPUT_IMAGE:
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON: {
        do_submit(ui, a, fi, do_download);
        break;
    }
    case FORM_INPUT_RESET: {
        for (int i = 0; i < ui.document->formitem->nanchor; i++) {
            struct Anchor* a2 = &ui.document->formitem->anchors[i];
            struct FormItem* f2 = (struct FormItem*)a2->url;
            if (f2->parent == fi->parent && f2->name && f2->value && f2->type != FORM_INPUT_SUBMIT && f2->type != FORM_INPUT_HIDDEN && f2->type != FORM_INPUT_RESET) {
                f2->value = f2->init_value;
                f2->checked = f2->init_checked;
                f2->label = f2->init_label;
                f2->selected = f2->init_selected;
                formUpdateBuffer(a2, ui.current_buffer, f2);
            }
        }
        break;
    }
    case FORM_INPUT_HIDDEN:
    default:
        break;
    }
}

void gotoLabel(struct UI ui, const char* label)
{
    struct Anchor* al = searchURLLabel(ui.current_buffer, label);
    if (al == NULL) {
        /* FIXME: gettextize? */
        message(getUI(), MSG_INFO, Sprintf("%s is not found", label)->ptr);
        return;
    }

    struct Buffer* buf = newBuffer();
    buf->content = ui.current_buffer->content;
    buf->document = ui.current_buffer->document;

    pushHashHist(URLHist, parsedURL2Str(&buf->content.url)->ptr);
    (*buf->clone)++;
    pushBuffer(ui, buf);
    gotoLine(ui.document, al->start.line);
    if (label_topline)
        ui.document->topLineIndex = ui.current_buffer->document.currentLineIndex
            - topLine(&ui.current_buffer->document)->linenumber;
    ui.document->pos = al->start.pos;

    return;
}

void followAnchor(struct UI ui, bool do_download)
{
    if (ui.document->firstLine == NULL)
        return;

    struct Anchor* a = retrieveAnchor(ui.document->img, getBufferPosition(ui));
    if (a && a->image && a->image->map) {
        _followForm(ui, false, do_download);
        return;
    }
    int x = 0, y = 0;
    int map = 0;
    if (a && a->image && a->image->ismap) {
        getMapXY(ui.document, a, &x, &y);
        map = 1;
    }
    a = retrieveAnchor(ui.document->href, getBufferPosition(ui));
    if (a == NULL) {
        _followForm(getUI(), false, do_download);
        return;
    }
    if (*a->url == '#') { /* index within this buffer */
        gotoLabel(ui, (char*)a->url + 1);
        return;
    }

    struct Url u = parseUrl(a->url, makeBaseUrl(&ui.current_buffer->document));
    if (Strcmp(parsedURL2Str(&u), parsedURL2Str(&ui.current_buffer->content.url)) == 0) {
        /* index within this buffer */
        if (u.label) {
            gotoLabel(ui, u.label);
            return;
        }
    }

    const char* url;
    url = a->url;
    if (map)
        url = Sprintf("%s?%d,%d", a->url, x, y)->ptr;

    loadLink(ui, url, (char*)a->target, a->referer, NULL, do_download);
}

void followImage(struct UI ui, bool do_download)
{
    if (ui.document->firstLine == NULL)
        return;

    struct Anchor* a = retrieveAnchor(ui.document->img, getBufferPosition(ui));
    if (a == NULL)
        return;
    /* FIXME: gettextize? */
    message(getUI(), MSG_INFO, Sprintf("loading %s", a->url)->ptr);
    // refresh(ttyWriter());
    struct Content c = loadGeneralFile(a->url, makeBaseUrl(&ui.current_buffer->document), NULL, NULL, UI_TTY);
    pushContent(ui, c);
}
