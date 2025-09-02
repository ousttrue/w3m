#include "readbuffer.h"
#include "alloc.h"
#include "etc.h"
#include "file.h"
#include "myctype.h"
#include "table.h"
#include "ui.h"
#include "indep.h"

struct link_stack {
    int cmd;
    short offset;
    short pos;
    struct link_stack* next;
};
static struct link_stack* link_stack = NULL;

void push_link(int cmd, int offset, int pos)
{
    struct link_stack* p = New(struct link_stack);
    p->cmd = cmd;
    p->offset = (short)offset;
    if (p->offset < 0)
        p->offset = 0;
    p->pos = (short)pos;
    if (p->pos < 0)
        p->pos = 0;
    p->next = link_stack;
    link_stack = p;
}

char* has_hidden_link(struct readbuffer* obuf, enum HtmlTag cmd)
{
    Str line = obuf->line;
    struct link_stack* p;

    if (Strlastchar(line) != '>')
        return NULL;

    for (p = link_stack; p; p = p->next)
        if (p->cmd == cmd)
            break;
    if (!p)
        return NULL;

    if (obuf->pos == p->pos)
        return line->ptr + p->offset;

    return NULL;
}

void append_tags(struct readbuffer* obuf)
{
    int i;
    int len = obuf->line->length;
    int set_bp = 0;

    for (i = 0; i < obuf->tag_sp; i++) {
        switch (obuf->tag_stack[i]->cmd) {
        case HTML_A:
        case HTML_IMG_ALT:
        case HTML_B:
        case HTML_U:
        case HTML_I:
        case HTML_S:
            push_link(obuf->tag_stack[i]->cmd, obuf->line->length, obuf->pos);
            break;
        }
        Strcat_charp(obuf->line, obuf->tag_stack[i]->cmdname);
        switch (obuf->tag_stack[i]->cmd) {
        case HTML_NOBR:
            if (obuf->nobr_level > 1)
                break;
        case HTML_WBR:
            set_bp = 1;
            break;
        }
    }
    obuf->tag_sp = 0;
    if (set_bp)
        set_breakpoint(obuf, obuf->line->length - len);
}

void push_tag(struct readbuffer* obuf, const char* cmdname, enum HtmlTag cmd)
{
    obuf->tag_stack[obuf->tag_sp] = New(struct cmdtable);
    obuf->tag_stack[obuf->tag_sp]->cmdname = allocStr(cmdname, -1);
    obuf->tag_stack[obuf->tag_sp]->cmd = cmd;
    obuf->tag_sp++;
    if (obuf->tag_sp >= TAG_STACK_SIZE || obuf->flag & (RB_SPECIAL & ~RB_NOBR))
        append_tags(obuf);
}

static int
sloppy_parse_line(char** str)
{
    if (**str == '<') {
        while (**str && **str != '>')
            (*str)++;
        if (**str == '>')
            (*str)++;
        return 1;
    } else {
        while (**str && **str != '<')
            (*str)++;
        return 0;
    }
}

void passthrough(struct readbuffer* obuf, char* str, int back)
{
    int cmd;
    Str tok = Strnew();
    char* str_bak;

    if (back) {
        Str str_save = Strnew_charp(str);
        Strshrink(obuf->line, obuf->line->ptr + obuf->line->length - str);
        str = str_save->ptr;
    }
    while (*str) {
        str_bak = str;
        if (sloppy_parse_line(&str)) {
            char* q = str_bak;
            cmd = gethtmlcmd(&q);
            if (back) {
                struct link_stack* p;
                for (p = link_stack; p; p = p->next) {
                    if (p->cmd == cmd) {
                        link_stack = p->next;
                        break;
                    }
                }
                back = 0;
            } else {
                Strcat_charp_n(tok, str_bak, str - str_bak);
                push_tag(obuf, tok->ptr, cmd);
                Strclear(tok);
            }
        } else {
            push_nchars(obuf, 0, str_bak, str - str_bak, obuf->prev_ctype);
        }
    }
}

void flushline(struct html_feed_environ* h_env, struct readbuffer* obuf, int indent, int force, int width)
{
    TextLineList* buf = h_env->buf;
    FILE* f = h_env->f;
    Str line = obuf->line, pass = NULL;
    char *hidden_anchor = NULL, *hidden_img = NULL, *hidden_bold = NULL,
         *hidden_under = NULL, *hidden_italic = NULL, *hidden_strike = NULL,
         *hidden_ins = NULL, *hidden_input = NULL, *hidden = NULL;

#ifdef DEBUG
    if (w3m_debug) {
        FILE* df = fopen("zzzproc1", "a");
        fprintf(df, "flushline(%s,%d,%d,%d)\n", obuf->line->ptr, indent, force,
            width);
        if (buf) {
            TextLineListItem* p;
            for (p = buf->first; p; p = p->next) {
                fprintf(df, "buf=\"%s\"\n", p->ptr->line->ptr);
            }
        }
        fclose(df);
    }
#endif

    if (!(obuf->flag & (RB_SPECIAL & ~RB_NOBR)) && Strlastchar(line) == ' ') {
        Strshrink(line, 1);
        obuf->pos--;
    }

    append_tags(obuf);

    if (obuf->anchor.url)
        hidden = hidden_anchor = has_hidden_link(obuf, HTML_A);
    if (obuf->img_alt) {
        if ((hidden_img = has_hidden_link(obuf, HTML_IMG_ALT)) != NULL) {
            if (!hidden || hidden_img < hidden)
                hidden = hidden_img;
        }
    }
    if (obuf->input_alt.in) {
        if ((hidden_input = has_hidden_link(obuf, HTML_INPUT_ALT)) != NULL) {
            if (!hidden || hidden_input < hidden)
                hidden = hidden_input;
        }
    }
    if (obuf->in_bold) {
        if ((hidden_bold = has_hidden_link(obuf, HTML_B)) != NULL) {
            if (!hidden || hidden_bold < hidden)
                hidden = hidden_bold;
        }
    }
    if (obuf->in_italic) {
        if ((hidden_italic = has_hidden_link(obuf, HTML_I)) != NULL) {
            if (!hidden || hidden_italic < hidden)
                hidden = hidden_italic;
        }
    }
    if (obuf->in_under) {
        if ((hidden_under = has_hidden_link(obuf, HTML_U)) != NULL) {
            if (!hidden || hidden_under < hidden)
                hidden = hidden_under;
        }
    }
    if (obuf->in_strike) {
        if ((hidden_strike = has_hidden_link(obuf, HTML_S)) != NULL) {
            if (!hidden || hidden_strike < hidden)
                hidden = hidden_strike;
        }
    }
    if (obuf->in_ins) {
        if ((hidden_ins = has_hidden_link(obuf, HTML_INS)) != NULL) {
            if (!hidden || hidden_ins < hidden)
                hidden = hidden_ins;
        }
    }
    if (hidden) {
        pass = Strnew_charp(hidden);
        Strshrink(line, line->ptr + line->length - hidden);
    }

    if (!(obuf->flag & (RB_SPECIAL & ~RB_NOBR)) && obuf->pos > width) {
        char* tp = &line->ptr[obuf->bp.len - obuf->bp.tlen];
        char* ep = &line->ptr[line->length];

        if (obuf->bp.pos == obuf->pos && tp <= ep && tp > line->ptr && tp[-1] == ' ') {
            memcpy(tp - 1, tp, ep - tp + 1);
            line->length--;
            obuf->pos--;
        }
    }

    if (obuf->anchor.url && !hidden_anchor)
        Strcat_charp(line, "</a>");
    if (obuf->img_alt && !hidden_img)
        Strcat_charp(line, "</img_alt>");
    if (obuf->input_alt.in && !hidden_input)
        Strcat_charp(line, "</input_alt>");
    if (obuf->in_bold && !hidden_bold)
        Strcat_charp(line, "</b>");
    if (obuf->in_italic && !hidden_italic)
        Strcat_charp(line, "</i>");
    if (obuf->in_under && !hidden_under)
        Strcat_charp(line, "</u>");
    if (obuf->in_strike && !hidden_strike)
        Strcat_charp(line, "</s>");
    if (obuf->in_ins && !hidden_ins)
        Strcat_charp(line, "</ins>");

    if (obuf->top_margin > 0) {
        int i;
        struct html_feed_environ h;
        struct readbuffer o;
        struct environment e[1];

        init_henv(&h, &o, e, 1, NULL, width, indent);
        o.line = Strnew_size(width + 20);
        o.pos = obuf->pos;
        o.flag = obuf->flag;
        o.top_margin = -1;
        o.bottom_margin = -1;
        Strcat_charp(o.line, "<pre_int>");
        for (i = 0; i < o.pos; i++)
            Strcat_char(o.line, ' ');
        Strcat_charp(o.line, "</pre_int>");
        for (i = 0; i < obuf->top_margin; i++)
            flushline(h_env, &o, indent, force, width);
    }

    if (force == 1 || obuf->flag & RB_NFLUSHED) {
        TextLine* lbuf = newTextLine(line, obuf->pos);
        if (RB_GET_ALIGN(obuf) == RB_CENTER) {
            align(lbuf, width, ALIGN_CENTER);
        } else if (RB_GET_ALIGN(obuf) == RB_RIGHT) {
            align(lbuf, width, ALIGN_RIGHT);
        } else if (RB_GET_ALIGN(obuf) == RB_LEFT && obuf->flag & RB_INTABLE) {
            align(lbuf, width, ALIGN_LEFT);
        } else if (obuf->flag & RB_FILL) {
            char* p;
            int rest, rrest;
            int nspace, d, i;

            rest = width - get_Str_strwidth(line);
            if (rest > 1) {
                nspace = 0;
                for (p = line->ptr + indent; *p; p++) {
                    if (*p == ' ')
                        nspace++;
                }
                if (nspace > 0) {
                    int indent_here = 0;
                    d = rest / nspace;
                    p = line->ptr;
                    while (IS_SPACE(*p)) {
                        p++;
                        indent_here++;
                    }
                    rrest = rest - d * nspace;
                    line = Strnew_size(width + 1);
                    for (i = 0; i < indent_here; i++)
                        Strcat_char(line, ' ');
                    for (; *p; p++) {
                        Strcat_char(line, *p);
                        if (*p == ' ') {
                            for (i = 0; i < d; i++)
                                Strcat_char(line, ' ');
                            if (rrest > 0) {
                                Strcat_char(line, ' ');
                                rrest--;
                            }
                        }
                    }
                    lbuf = newTextLine(line, width);
                }
            }
        }
#ifdef TABLE_DEBUG
        if (w3m_debug) {
            FILE* f = fopen("zzzproc1", "a");
            fprintf(f, "pos=%d,%d, maxlimit=%d\n",
                visible_length(lbuf->line->ptr), lbuf->pos,
                h_env->maxlimit);
            fclose(f);
        }
#endif
        if (lbuf->pos > h_env->maxlimit)
            h_env->maxlimit = lbuf->pos;
        if (buf)
            pushTextLine(buf, lbuf);
        else if (f) {
            Strfputs(lbuf->line, f);
            fputc('\n', f);
        }
        if (obuf->flag & RB_SPECIAL || obuf->flag & RB_NFLUSHED)
            h_env->blank_lines = 0;
        else
            h_env->blank_lines++;
    } else {
        char *p = line->ptr, *q;
        Str tmp = Strnew(), tmp2 = Strnew();

#define APPEND(str)                    \
    if (buf)                           \
        appendTextLine(buf, (str), 0); \
    else if (f)                        \
    Strfputs((str), f)

        while (*p) {
            q = p;
            if (sloppy_parse_line(&p)) {
                Strcat_charp_n(tmp, q, p - q);
                if (force == 2) {
                    APPEND(tmp);
                } else
                    Strcat(tmp2, tmp);
                Strclear(tmp);
            }
        }
        if (force == 2) {
            if (pass) {
                APPEND(pass);
            }
            pass = NULL;
        } else {
            if (pass)
                Strcat(tmp2, pass);
            pass = tmp2;
        }
    }

    if (obuf->bottom_margin > 0) {
        int i;
        struct html_feed_environ h;
        struct readbuffer o;
        struct environment e[1];

        init_henv(&h, &o, e, 1, NULL, width, indent);
        o.line = Strnew_size(width + 20);
        o.pos = obuf->pos;
        o.flag = obuf->flag;
        o.top_margin = -1;
        o.bottom_margin = -1;
        Strcat_charp(o.line, "<pre_int>");
        for (i = 0; i < o.pos; i++)
            Strcat_char(o.line, ' ');
        Strcat_charp(o.line, "</pre_int>");
        for (i = 0; i < obuf->bottom_margin; i++)
            flushline(h_env, &o, indent, force, width);
    }
    if (obuf->top_margin < 0 || obuf->bottom_margin < 0)
        return;

    obuf->line = Strnew_size(256);
    obuf->pos = 0;
    obuf->top_margin = 0;
    obuf->bottom_margin = 0;
    set_space_to_prevchar(obuf->prevchar);
    obuf->bp.init_flag = 1;
    obuf->flag &= ~RB_NFLUSHED;
    set_breakpoint(obuf, 0);
    obuf->prev_ctype = PC_ASCII;
    link_stack = NULL;
    fillline(obuf, indent);
    if (pass)
        passthrough(obuf, pass->ptr, 0);
    if (!hidden_anchor && obuf->anchor.url) {
        Str tmp;
        if (obuf->anchor.hseq > 0)
            obuf->anchor.hseq = -obuf->anchor.hseq;
        tmp = Sprintf("<A HSEQ=\"%d\" HREF=\"", obuf->anchor.hseq);
        Strcat_charp(tmp, html_quote(obuf->anchor.url));
        if (obuf->anchor.target) {
            Strcat_charp(tmp, "\" TARGET=\"");
            Strcat_charp(tmp, html_quote(obuf->anchor.target));
        }
        if (obuf->anchor.referer) {
            Strcat_charp(tmp, "\" REFERER=\"");
            Strcat_charp(tmp, html_quote((char*)obuf->anchor.referer));
        }
        if (obuf->anchor.title) {
            Strcat_charp(tmp, "\" TITLE=\"");
            Strcat_charp(tmp, html_quote(obuf->anchor.title));
        }
        if (obuf->anchor.accesskey) {
            char* c = html_quote_char(obuf->anchor.accesskey);
            Strcat_charp(tmp, "\" ACCESSKEY=\"");
            if (c)
                Strcat_charp(tmp, c);
            else
                Strcat_char(tmp, obuf->anchor.accesskey);
        }
        Strcat_charp(tmp, "\">");
        push_tag(obuf, tmp->ptr, HTML_A);
    }
    if (!hidden_img && obuf->img_alt) {
        Str tmp = Strnew_charp("<IMG_ALT SRC=\"");
        Strcat_charp(tmp, html_quote(obuf->img_alt->ptr));
        Strcat_charp(tmp, "\">");
        push_tag(obuf, tmp->ptr, HTML_IMG_ALT);
    }
    if (!hidden_input && obuf->input_alt.in) {
        Str tmp;
        if (obuf->input_alt.hseq > 0)
            obuf->input_alt.hseq = -obuf->input_alt.hseq;
        tmp = Sprintf("<INPUT_ALT hseq=\"%d\" fid=\"%d\" name=\"%s\" type=\"%s\" value=\"%s\">",
            obuf->input_alt.hseq,
            obuf->input_alt.fid,
            obuf->input_alt.name ? obuf->input_alt.name->ptr : "",
            obuf->input_alt.type ? obuf->input_alt.type->ptr : "",
            obuf->input_alt.value ? obuf->input_alt.value->ptr : "");
        push_tag(obuf, tmp->ptr, HTML_INPUT_ALT);
    }
    if (!hidden_bold && obuf->in_bold)
        push_tag(obuf, "<B>", HTML_B);
    if (!hidden_italic && obuf->in_italic)
        push_tag(obuf, "<I>", HTML_I);
    if (!hidden_under && obuf->in_under)
        push_tag(obuf, "<U>", HTML_U);
    if (!hidden_strike && obuf->in_strike)
        push_tag(obuf, "<S>", HTML_S);
    if (!hidden_ins && obuf->in_ins)
        push_tag(obuf, "<INS>", HTML_INS);
}

void purgeline(struct html_feed_environ* h_env)
{
    char *p, *q;
    Str tmp;
    TextLine* tl;

    if (h_env->buf == NULL || h_env->blank_lines == 0)
        return;

    if (!(tl = rpopTextLine(h_env->buf)))
        return;
    p = tl->line->ptr;
    tmp = Strnew();
    while (*p) {
        q = p;
        if (sloppy_parse_line(&p)) {
            Strcat_charp_n(tmp, q, p - q);
        }
    }
    appendTextLine(h_env->buf, tmp, 0);
    h_env->blank_lines--;
}

void set_breakpoint(struct readbuffer* obuf, int tag_length)
{
    obuf->bp.len = obuf->line->length;
    obuf->bp.pos = obuf->pos;
    obuf->bp.tlen = tag_length;
    obuf->bp.flag = obuf->flag;
    obuf->bp.flag &= ~RB_FILL;
    obuf->bp.top_margin = obuf->top_margin;
    obuf->bp.bottom_margin = obuf->bottom_margin;

    if (!obuf->bp.init_flag)
        return;

    memcpy((void*)&obuf->bp.anchor, (void*)&obuf->anchor, sizeof(obuf->anchor));
    obuf->bp.img_alt = obuf->img_alt;
    obuf->bp.input_alt = obuf->input_alt;
    obuf->bp.in_bold = obuf->in_bold;
    obuf->bp.in_italic = obuf->in_italic;
    obuf->bp.in_under = obuf->in_under;
    obuf->bp.in_strike = obuf->in_strike;
    obuf->bp.in_ins = obuf->in_ins;
    obuf->bp.nobr_level = obuf->nobr_level;
    obuf->bp.prev_ctype = obuf->prev_ctype;
    obuf->bp.init_flag = 0;
}
