#include "form.h"
#include "HtmlTagParsed.h"
#include "menu.h"
#include "Line.h"
#include "AnchorList.h"
#include "Anchor.h"
#include "ContentType.h"
#include "runtime.h"
#include "convertline.h"
#include "quote.h"
#include "rc.h"
#include "w3m.h"
#include "buffer_util.h"
#include "local_cgi.h"
#include "regex.h"
#include "w3m.h"

#include "alloc.h"
#include "myctype.h"

#include <wc.h>
#include <wtf.h>

#include <string.h>
#include <strings.h>
#include <unistd.h>

int FoldTextarea = (false);
#define DEF_EDITOR "/usr/bin/vim"
char* Editor = (DEF_EDITOR);
#define PRE_FORM_FILE RC_DIR "/pre_form"
char* pre_form_file = (PRE_FORM_FILE);

extern Str* textarea_str;
extern struct FormSelectOption* select_option;

void formRecheckRadio(struct UI ui, struct Anchor* a, struct Buffer* buf, struct FormItem* fi)
{
    int i;
    struct Anchor* a2;
    struct FormItem* f2;

    for (i = 0; i < buf->document.formitem->nanchor; i++) {
        a2 = &buf->document.formitem->anchors[i];
        f2 = (struct FormItem*)a2->url;
        if (f2->parent == fi->parent && f2 != fi && f2->type == FORM_INPUT_RADIO && Strcmp(f2->name, fi->name) == 0) {
            f2->checked = 0;
            formUpdateBuffer(a2, buf, f2);
        }
    }
    fi->checked = 1;
    formUpdateBuffer(a, buf, fi);
}

void formResetBuffer(struct Buffer* buf, struct AnchorList* formitem)
{
    int i;
    struct Anchor* a;
    struct FormItem *f1, *f2;

    if (buf == NULL || buf->document.formitem == NULL || formitem == NULL)
        return;
    for (i = 0; i < buf->document.formitem->nanchor && i < formitem->nanchor; i++) {
        a = &buf->document.formitem->anchors[i];
        if (a->y != a->start.line)
            continue;
        f1 = (struct FormItem*)a->url;
        f2 = (struct FormItem*)formitem->anchors[i].url;
        if (f1->type != f2->type || strcmp(((f1->name == NULL) ? "" : f1->name->ptr), ((f2->name == NULL) ? "" : f2->name->ptr)))
            break; /* What's happening */
        switch (f1->type) {
        case FORM_INPUT_TEXT:
        case FORM_INPUT_PASSWORD:
        case FORM_INPUT_FILE:
        case FORM_TEXTAREA:
            f1->value = f2->value;
            f1->init_value = f2->init_value;
            break;
        case FORM_INPUT_CHECKBOX:
        case FORM_INPUT_RADIO:
            f1->checked = f2->checked;
            f1->init_checked = f2->init_checked;
            break;
        case FORM_SELECT:
            f1->select_option = f2->select_option;
            f1->value = f2->value;
            f1->label = f2->label;
            f1->selected = f2->selected;
            f1->init_value = f2->init_value;
            f1->init_label = f2->init_label;
            f1->init_selected = f2->init_selected;
            break;
        default:
            continue;
        }
        formUpdateBuffer(a, buf, f1);
    }
}

static int
form_update_line(struct Line* line, char** str, int spos, int epos, int width,
    int newline, int password)
{
    int c_len = 1, c_width = 1, w, i, len, pos;
    char *p, *buf;
    Lineprop c_type, effect, *prop;

    for (p = *str, w = 0, pos = 0; *p && w < width;) {
        c_type = get_mctype((unsigned char*)p);
        c_len = get_mclen(p);
        c_width = get_mcwidth(p);
        if (c_type == PC_CTRL) {
            if (newline && *p == '\n')
                break;
            if (*p != '\r') {
                w++;
                pos++;
            }
        } else if (password) {
            if (w + c_width > width)
                break;
            w += c_width;
            pos += c_width;
        } else if (c_type & PC_UNKNOWN) {
            w++;
            pos++;
        } else {
            if (w + c_width > width)
                break;
            w += c_width;
            pos += c_len;
        }
        p += c_len;
    }
    pos += width - w;

    len = line->len + pos + spos - epos;
    buf = New_N(char, len + 1);
    buf[len] = '\0';
    prop = New_N(Lineprop, len);
    memcpy(buf, line->lineBuf, spos * sizeof(char));
    memcpy(prop, line->propBuf, spos * sizeof(Lineprop));

    effect = CharEffect(line->propBuf[spos]);
    for (p = *str, w = 0, pos = spos; *p && w < width;) {
        c_type = get_mctype((unsigned char*)p);
        c_len = get_mclen(p);
        c_width = get_mcwidth(p);
        if (c_type == PC_CTRL) {
            if (newline && *p == '\n')
                break;
            if (*p != '\r') {
                buf[pos] = password ? '*' : ' ';
                prop[pos] = effect | PC_ASCII;
                pos++;
                w++;
            }
        } else if (password) {
            if (w + c_width > width)
                break;
            for (i = 0; i < c_width; i++) {
                buf[pos] = '*';
                prop[pos] = effect | PC_ASCII;
                pos++;
                w++;
            }
        } else if (c_type & PC_UNKNOWN) {
            buf[pos] = ' ';
            prop[pos] = effect | PC_ASCII;
            pos++;
            w++;
        } else {
            if (w + c_width > width)
                break;
            buf[pos] = *p;
            prop[pos] = effect | c_type;
            pos++;
            c_type = (c_type & ~PC_WCHAR1) | PC_WCHAR2;
            for (i = 1; i < c_len; i++) {
                buf[pos] = p[i];
                prop[pos] = effect | c_type;
                pos++;
            }
            w += c_width;
        }
        p += c_len;
    }
    for (; w < width; w++) {
        buf[pos] = ' ';
        prop[pos] = effect | PC_ASCII;
        pos++;
    }
    if (newline) {
        if (!FoldTextarea) {
            while (*p && *p != '\r' && *p != '\n')
                p++;
        }
        if (*p == '\r')
            p++;
        if (*p == '\n')
            p++;
    }
    *str = p;

    memcpy(&buf[pos], &line->lineBuf[epos], (line->len - epos) * sizeof(char));
    memcpy(&prop[pos], &line->propBuf[epos], (line->len - epos) * sizeof(Lineprop));
    line->lineBuf = buf;
    line->propBuf = prop;
    line->len = len;
    line->size = len;

    return pos;
}

void formUpdateBuffer(struct Anchor* a, struct Buffer* buf, struct FormItem* form)
{
    int top = buf->document.topLineIndex;
    int current = buf->document.currentLineIndex;
    gotoLine(&buf->document, a->start.line);

    int spos, epos;
    switch (form->type) {
    case FORM_TEXTAREA:
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_CHECKBOX:
    case FORM_INPUT_RADIO:
    case FORM_SELECT:
        spos = a->start.pos;
        epos = a->end.pos;
        break;
    default:
        spos = a->start.pos + 1;
        epos = a->end.pos - 1;
    }

    int rows, c_rows, pos, col = 0;
    char* p;
    switch (form->type) {
    case FORM_INPUT_CHECKBOX:
    case FORM_INPUT_RADIO:
        if (currentLine(&buf->document) == NULL || spos >= currentLine(&buf->document)->l.len || spos < 0)
            break;
        if (form->checked)
            currentLine(&buf->document)->l.lineBuf[spos] = '*';
        else
            currentLine(&buf->document)->l.lineBuf[spos] = ' ';
        break;
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
    case FORM_INPUT_PASSWORD:
    case FORM_TEXTAREA:
    case FORM_SELECT:
        if (form->type == FORM_SELECT) {
            p = form->label->ptr;
            updateSelectOption(form, form->select_option);
        } else {
            if (!form->value)
                break;
            p = form->value->ptr;
        }
        struct LineList* l = currentLine(&buf->document);
        if (!l)
            break;
        if (form->type == FORM_TEXTAREA) {
            int n = a->y - currentLine(&buf->document)->linenumber;
            if (n > 0)
                for (; l && n; l = l->prev, n--)
                    ;
            else if (n < 0)
                for (; l && n; l = l->prev, n++)
                    ;
            if (!l)
                break;
        }
        rows = form->rows ? form->rows : 1;
        col = COLPOS(&l->l, a->start.pos);
        for (c_rows = 0; c_rows < rows; c_rows++, l = l->next) {
            if (l == NULL)
                break;
            if (rows > 1) {
                pos = columnPos(&l->l, col);
                a = retrieveAnchor(buf->document.formitem,
                    (struct BufferPoint) { .line = l->linenumber, .pos = pos });
                if (a == NULL)
                    break;
                spos = a->start.pos;
                epos = a->end.pos;
            }
            if (a->start.line != a->end.line
                || spos > epos
                || epos >= l->l.len
                || spos < 0 || epos < 0 || COLPOS(&l->l, epos) < col)
                break;
            pos = form_update_line(&l->l, &p, spos, epos, COLPOS(&l->l, epos) - col,
                rows > 1,
                form->type == FORM_INPUT_PASSWORD);
            if (pos != epos) {
                shiftAnchorPosition(buf->document.href, buf->document.hmarklist,
                    (struct BufferPoint) { .line = a->start.line, .pos = spos }, pos - epos);
                shiftAnchorPosition(buf->document.name, buf->document.hmarklist,
                    (struct BufferPoint) { .line = a->start.line, .pos = spos }, pos - epos);
                shiftAnchorPosition(buf->document.img, buf->document.hmarklist,
                    (struct BufferPoint) { .line = a->start.line, .pos = spos }, pos - epos);
                shiftAnchorPosition(buf->document.formitem, buf->document.hmarklist,
                    (struct BufferPoint) { .line = a->start.line, .pos = spos }, pos - epos);
            }
        }
        break;
    default:
        break;
    }

    buf->document.topLineIndex = top;
    buf->document.currentLineIndex = current;
}

static void
form_fputs_decode(Str s, FILE* f)
{
    char* p;
    Str z = Strnew();

    for (p = s->ptr; *p;) {
        switch (*p) {
#if !defined(__CYGWIN__) && !defined(__EMX__)
        case '\r':
            if (*(p + 1) == '\n')
                p++;
            /* continue to the next label */
#endif /* !defined( __CYGWIN__ ) && !defined( __EMX__ \
        * ) */
        default:
            Strcat_char(z, *p);
            p++;
            break;
        }
    }
    z = wc_Str_conv_strict(z, InnerCharset, DisplayCharset);
    Strfputs(z, f);
}

void input_textarea(struct UI ui, struct FormItem* fi)
{
    char* tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    Str tmp;
    FILE* f;
    wc_ces charset = DisplayCharset;
    wc_uint8 auto_detect;

    f = fopen(tmpf, "w");
    if (f == NULL) {
        /* FIXME: gettextize? */
        message(ui, MSG_ERR, "Can't open temporary file");
        return;
    }
    if (fi->value)
        form_fputs_decode(fi->value, f);
    fclose(f);

    if (exec_cmd(myEditor(Editor, tmpf, 1)->ptr))
        goto input_end;

    if (fi->readonly)
        goto input_end;
    f = fopen(tmpf, "r");
    if (f == NULL) {
        /* FIXME: gettextize? */
        message(ui, MSG_ERR, "Can't open temporary file");
        goto input_end;
    }
    fi->value = Strnew();
    auto_detect = WcOption.auto_detect;
    WcOption.auto_detect = WC_OPT_DETECT_ON;
    while (tmp = Strfgets(f), tmp->length > 0) {
        if (tmp->length == 1 && tmp->ptr[tmp->length - 1] == '\n') {
            /* null line with bare LF */
            tmp = Strnew_charp("\r\n");
        } else if (tmp->length > 1 && tmp->ptr[tmp->length - 1] == '\n' && tmp->ptr[tmp->length - 2] != '\r') {
            Strshrink(tmp, 1);
            Strcat_charp(tmp, "\r\n");
        }
        tmp = convertLine(tmp, RAW_MODE, &charset, DisplayCharset, InnerCharset);
        Strcat(fi->value, tmp);
    }
    WcOption.auto_detect = auto_detect;
    fclose(f);
input_end:
    unlink(tmpf);
}

int formChooseOptionByMenu(struct UI ui, struct FormItem* fi, int x, int y)
{
    int i, n, selected = -1, init_select = fi->selected;
    struct FormSelectOptionItem* opt;

    for (n = 0, opt = fi->select_option; opt != NULL; n++, opt = opt->next)
        ;
    const char** label = New_N(char*, n + 1);
    for (i = 0, opt = fi->select_option; opt != NULL; i++, opt = opt->next)
        label[i] = opt->label->ptr;
    label[n] = NULL;

    optionMenu(ui, x, y, label, &selected, init_select, NULL);

    if (selected < 0)
        return 0;
    for (i = 0, opt = fi->select_option; opt != NULL; i++, opt = opt->next) {
        if (i == selected) {
            fi->selected = selected;
            fi->value = opt->value;
            fi->label = opt->label;
            break;
        }
    }
    updateSelectOption(fi, fi->select_option);
    return 1;
}

void form_write_data(FILE* f, const char* boundary, const char* name, const char* value)
{
    fprintf(f, "--%s\r\n", boundary);
    fprintf(f, "Content-Disposition: form-data; name=\"%s\"\r\n\r\n", name);
    fprintf(f, "%s\r\n", value);
}

void form_write_from_file(FILE* f, const char* boundary, const char* name, const char* filename, const char* file)
{
    fprintf(f, "--%s\r\n", boundary);
    fprintf(f,
        "Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n",
        name, mybasename(filename));

    enum ContentType content_type = guessContentType(file);
    // fprintf(f, "Content-Type: %s\r\n\r\n",
    //     type ? type : "application/octet-stream");

    struct stat st;
    if (lstat(file, &st) < 0)
        goto write_end;
    if (S_ISDIR(st.st_mode))
        goto write_end;
    FILE* fd = fopen(file, "r");
    if (fd != NULL) {
        int c;
        while ((c = fgetc(fd)) != EOF)
            fputc(c, f);
        fclose(fd);
    }
write_end:
    fprintf(f, "\r\n");
}

struct pre_form_item {
    int type;
    const char* name;
    const char* value;
    int checked;
    struct pre_form_item* next;
};

struct pre_form {
    const char* url;
    Regex* re_url;
    const char* name;
    const char* action;
    struct pre_form_item* item;
    struct pre_form* next;
};

static struct pre_form* PreForm = NULL;

static struct pre_form*
add_pre_form(struct pre_form* prev, const char* url, Regex* re_url, const char* name, const char* action)
{
    struct Url pu;
    struct pre_form* new;

    if (prev)
        new = prev->next = New(struct pre_form);
    else
        new = PreForm = New(struct pre_form);
    if (url && !re_url) {
        pu = parseUrl(url, NULL);
        new->url = parsedURL2Str(&pu)->ptr;
    } else
        new->url = url;
    new->re_url = re_url;
    new->name = (name && *name) ? name : NULL;
    new->action = (action && *action) ? action : NULL;
    new->item = NULL;
    new->next = NULL;
    return new;
}

static struct pre_form_item*
add_pre_form_item(struct pre_form* pf, struct pre_form_item* prev, int type,
    const char* name, const char* value, const char* checked)
{
    struct pre_form_item* new;

    if (!pf)
        return NULL;
    if (prev)
        new = prev->next = New(struct pre_form_item);
    else
        new = pf->item = New(struct pre_form_item);
    new->type = type;
    new->name = name;
    new->value = value;
    if (checked && *checked && (!strcmp(checked, "0") || !strcasecmp(checked, "off") || !strcasecmp(checked, "no")))
        new->checked = 0;
    else
        new->checked = 1;
    new->next = NULL;
    return new;
}

/*
 * url <url>|/<re-url>/
 * form [<name>] <action>
 * text <name> <value>
 * file <name> <value>
 * passwd <name> <value>
 * checkbox <name> <value> [<checked>]
 * radio <name> <value>
 * select <name> <value>
 * submit [<name> [<value>]]
 * image [<name> [<value>]]
 * textarea <name>
 * <value>
 * /textarea
 */

void loadPreForm(struct UI ui)
{
    FILE* fp;
    Str line = NULL, textarea = NULL;
    struct pre_form* pf = NULL;
    struct pre_form_item* pi = NULL;
    int type = -1;
    char* name = NULL;

    PreForm = NULL;
    fp = openSecretFile(ui, pre_form_file);
    if (fp == NULL)
        return;
    while (1) {
        const char *p, *s, *arg;
        Regex* re_arg;

        line = Strfgets(fp);
        if (line->length == 0)
            break;
        if (textarea && !(!strncmp(line->ptr, "/textarea", 9) && IS_SPACE(line->ptr[9]))) {
            Strcat(textarea, line);
            continue;
        }
        Strchop(line);
        Strremovefirstspaces(line);
        p = line->ptr;
        if (*p == '#' || *p == '\0')
            continue; /* comment or empty line */
        s = getWord(&p);

        if (!strcmp(s, "url")) {
            arg = getRegexWord((const char**)&p, &re_arg);
            if (!arg || !*arg)
                continue;
            p = getQWord(&p);
            pf = add_pre_form(pf, arg, re_arg, NULL, p);
            pi = pf->item;
            continue;
        }
        if (!pf)
            continue;

        arg = getWord(&p);
        if (!strcmp(s, "form")) {
            if (!arg || !*arg)
                continue;
            s = getQWord(&p);
            p = getQWord(&p);
            if (!p || !*p) {
                p = s;
                s = NULL;
            }
            if (pf->item) {
                struct pre_form* prev = pf;
                pf = add_pre_form(prev, "", NULL, s, p);
                /* copy previous URL */
                pf->url = prev->url;
                pf->re_url = prev->re_url;
            } else {
                pf->name = s;
                pf->action = (p && *p) ? p : NULL;
            }
            pi = pf->item;
            continue;
        }
        if (!strcmp(s, "text"))
            type = FORM_INPUT_TEXT;
        else if (!strcmp(s, "file"))
            type = FORM_INPUT_FILE;
        else if (!strcmp(s, "passwd") || !strcmp(s, "password"))
            type = FORM_INPUT_PASSWORD;
        else if (!strcmp(s, "checkbox"))
            type = FORM_INPUT_CHECKBOX;
        else if (!strcmp(s, "radio"))
            type = FORM_INPUT_RADIO;
        else if (!strcmp(s, "submit"))
            type = FORM_INPUT_SUBMIT;
        else if (!strcmp(s, "image"))
            type = FORM_INPUT_IMAGE;
        else if (!strcmp(s, "select"))
            type = FORM_SELECT;
        else if (!strcmp(s, "textarea")) {
            type = FORM_TEXTAREA;
            name = Strnew_charp(arg)->ptr;
            textarea = Strnew();
            continue;
        } else if (textarea && name && !strcmp(s, "/textarea")) {
            pi = add_pre_form_item(pf, pi, type, name, textarea->ptr, NULL);
            textarea = NULL;
            name = NULL;
            continue;
        } else
            continue;
        s = getQWord(&p);
        pi = add_pre_form_item(pf, pi, type, arg, s, getQWord(&p));
    }
    fclose(fp);
}

void preFormUpdateBuffer(struct UI ui, struct Buffer* buf)
{
    struct pre_form* pf;
    struct pre_form_item* pi;
    int i;
    struct Anchor* a;
    struct Form* fl;
    struct FormItem* fi;
    struct FormSelectOptionItem* opt;
    int j;

    if (!buf || !buf->document.formitem || !PreForm)
        return;

    for (pf = PreForm; pf; pf = pf->next) {
        if (pf->re_url) {
            Str url = parsedURL2Str(&buf->content.url);
            if (!RegexMatch(pf->re_url, url->ptr, url->length, 1))
                continue;
        } else if (pf->url) {
            if (Strcmp_charp(parsedURL2Str(&buf->content.url), pf->url))
                continue;
        } else
            continue;
        for (i = 0; i < buf->document.formitem->nanchor; i++) {
            a = &buf->document.formitem->anchors[i];
            fi = (struct FormItem*)a->url;
            fl = fi->parent;
            if (pf->name && (!fl->name || strcmp(fl->name, pf->name)))
                continue;
            if (pf->action
                && (!fl->action || Strcmp_charp(fl->action, pf->action)))
                continue;
            for (pi = pf->item; pi; pi = pi->next) {
                if (pi->type != fi->type)
                    continue;
                if (pi->type == FORM_INPUT_SUBMIT || pi->type == FORM_INPUT_IMAGE) {
                    if ((!pi->name || !*pi->name || (fi->name && !Strcmp_charp(fi->name, pi->name))) && (!pi->value || !*pi->value || (fi->value && !Strcmp_charp(fi->value, pi->value))))
                        buf->submit = a;
                    continue;
                }
                if (!pi->name || !fi->name || Strcmp_charp(fi->name, pi->name))
                    continue;
                switch (pi->type) {
                case FORM_INPUT_TEXT:
                case FORM_INPUT_FILE:
                case FORM_INPUT_PASSWORD:
                case FORM_TEXTAREA:
                    fi->value = Strnew_charp(pi->value);
                    formUpdateBuffer(a, buf, fi);
                    break;
                case FORM_INPUT_CHECKBOX:
                    if (pi->value && fi->value && !Strcmp_charp(fi->value, pi->value)) {
                        fi->checked = pi->checked;
                        formUpdateBuffer(a, buf, fi);
                    }
                    break;
                case FORM_INPUT_RADIO:
                    if (pi->value && fi->value && !Strcmp_charp(fi->value, pi->value))
                        formRecheckRadio(ui, a, buf, fi);
                    break;
                case FORM_SELECT:
                    for (j = 0, opt = fi->select_option; opt != NULL;
                        j++, opt = opt->next) {
                        if (pi->value && opt->value && !Strcmp_charp(opt->value, pi->value)) {
                            fi->selected = j;
                            fi->value = opt->value;
                            fi->label = opt->label;
                            updateSelectOption(fi, fi->select_option);
                            formUpdateBuffer(a, buf, fi);
                            break;
                        }
                    }
                    break;
                }
            }
        }
    }
}
