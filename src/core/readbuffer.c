#include <stdlib.h>
#include "readbuffer.h"
#include "str_util.h"
#include "quote.h"
#include "html_quote.h"
#include "buffer_loader.h"
#include "url.h"
#include "HtmlTagParsed.h"
#include "html_title.h"
#include "alloc.h"
#include "myctype.h"
#include "table.h"
#include "ui.h"
#include "ctrlcode.h"
#include "symbol.h"
#include "display.h"
#include "hash.h"
#include <strings.h>

#include <wc.h>
#include <wtf.h>

char DisableCenter = (false);
int IndentIncr = (4);
char DisplayBorders = (false);
int displayInsDel = (DISPLAY_INS_DEL_NORMAL);
int view_unseenobject = (false);

int cur_hseq;
int cur_iseq;
Str getLinkNumberStr(int correction)
{
    return Sprintf("[%d]", cur_hseq + correction);
}

int need_number = 0;

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
    int len = obuf->line->length;
    int set_bp = 0;
    for (int i = 0; i < obuf->tag_sp; i++) {
        switch (obuf->tag_stack[i]->cmd) {
        case HTML_A:
        case HTML_IMG_ALT:
        case HTML_B:
        case HTML_U:
        case HTML_I:
        case HTML_S:
            push_link(obuf->tag_stack[i]->cmd, obuf->line->length, obuf->pos);
            break;
        default:
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
        default:
            break;
        }
    }
    obuf->tag_sp = 0;
    if (set_bp)
        set_breakpoint(obuf, obuf->line->length - len);
}

void back_to_breakpoint(struct readbuffer* obuf)
{
    obuf->flag = obuf->bp.flag;
    memcpy(&obuf->anchor, &obuf->bp.anchor, sizeof(obuf->anchor));
    obuf->img_alt = obuf->bp.img_alt;
    obuf->input_alt = obuf->bp.input_alt;
    obuf->in_bold = obuf->bp.in_bold;
    obuf->in_italic = obuf->bp.in_italic;
    obuf->in_under = obuf->bp.in_under;
    obuf->in_strike = obuf->bp.in_strike;
    obuf->in_ins = obuf->bp.in_ins;
    obuf->prev_ctype = obuf->bp.prev_ctype;
    obuf->pos = obuf->bp.pos;
    obuf->top_margin = obuf->bp.top_margin;
    obuf->bottom_margin = obuf->bp.bottom_margin;
    if (obuf->flag & RB_NOBR)
        obuf->nobr_level = obuf->bp.nobr_level;
}

void fillline(struct readbuffer* obuf, int indent)
{
    push_spaces(obuf, 1, indent - obuf->pos);
    obuf->flag &= ~RB_NFLUSHED;
}

void push_nchars(struct readbuffer* obuf, int width, const char* str, int len, Lineprop mode)
{
    append_tags(obuf);
    Strcat_charp_n(obuf->line, str, len);
    obuf->pos += width;
    if (width > 0) {
        set_prevchar(obuf->prevchar, str, len);
        obuf->prev_ctype = mode;
    }
    obuf->flag |= RB_NFLUSHED;
}

void check_breakpoint(struct readbuffer* obuf, bool pre_mode, const char* ch)
{
    int len = obuf->line->length;
    append_tags(obuf);
    if (pre_mode)
        return;

    int tlen = obuf->line->length - len;
    if (tlen > 0
        || is_boundary((unsigned char*)obuf->prevchar->ptr,
            (unsigned char*)ch))
        set_breakpoint(obuf, tlen);
}

void push_char(struct readbuffer* obuf, int pre_mode, char ch)
{
    check_breakpoint(obuf, pre_mode, &ch);
    Strcat_char(obuf->line, ch);
    obuf->pos++;
    set_prevchar(obuf->prevchar, &ch, 1);
    if (ch != ' ')
        obuf->prev_ctype = PC_ASCII;
    obuf->flag |= RB_NFLUSHED;
}

void proc_mchar(struct readbuffer* obuf, bool pre_mode, int width, const char** str, Lineprop mode)
{
    check_breakpoint(obuf, pre_mode, *str);
    obuf->pos += width;
    Strcat_charp_n(obuf->line, *str, get_mclen(*str));
    if (width > 0) {
        set_prevchar(obuf->prevchar, *str, 1);
        if (**str != ' ')
            obuf->prev_ctype = mode;
    }
    (*str) += get_mclen(*str);
    obuf->flag |= RB_NFLUSHED;
}

void set_alignment(struct readbuffer* obuf, struct HtmlTagParsed* tag)
{
    long flag = -1;
    int align;
    if (parsedtag_get_value(tag, ATTR_ALIGN, &align)) {
        switch (align) {
        case ALIGN_CENTER:
            if (DisableCenter)
                flag = RB_LEFT;
            else
                flag = RB_CENTER;
            break;
        case ALIGN_RIGHT:
            flag = RB_RIGHT;
            break;
        case ALIGN_LEFT:
            flag = RB_LEFT;
        }
    }
    RB_SAVE_FLAG(obuf);
    if (flag != -1) {
        RB_SET_ALIGN(obuf, flag);
    }
}

void clear_ignore_p_flag(struct readbuffer* obuf, int cmd)
{
    static int clear_flag_cmd[] = {
        HTML_HR, HTML_UNKNOWN
    };
    int i;

    for (i = 0; clear_flag_cmd[i] != HTML_UNKNOWN; i++) {
        if (cmd == clear_flag_cmd[i]) {
            obuf->flag &= ~RB_IGNORE_P;
            return;
        }
    }
}

int close_effect0(struct readbuffer* obuf, enum HtmlTag cmd)
{
    int i;
    for (i = obuf->tag_sp - 1; i >= 0; i--) {
        if (obuf->tag_stack[i]->cmd == cmd)
            break;
    }

    char* p;
    if (i >= 0) {
        obuf->tag_sp--;
        memcpy(&obuf->tag_stack[i], &obuf->tag_stack[i + 1], (obuf->tag_sp - i) * sizeof(struct cmdtable*));
        return 1;
    } else if ((p = has_hidden_link(obuf, cmd)) != NULL) {
        passthrough(obuf, p, 1);
        return 1;
    }
    return 0;
}

void push_spaces(struct readbuffer* obuf, bool pre_mode, int width)
{
    if (width <= 0)
        return;
    check_breakpoint(obuf, pre_mode, " ");
    for (int i = 0; i < width; i++)
        Strcat_char(obuf->line, ' ');
    obuf->pos += width;
    set_space_to_prevchar(obuf->prevchar);
    obuf->flag |= RB_NFLUSHED;
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

#define MAX_CMD_LEN 128

static int gethtmlcmd(const char** s)
{
    extern Hash_si tagtable;
    char cmdstr[MAX_CMD_LEN];
    char* p = cmdstr;
    const char* save = *s;
    int cmd;

    (*s)++;
    /* first character */
    if (IS_ALNUM(**s) || **s == '_' || **s == '/') {
        *(p++) = TOLOWER(**s);
        (*s)++;
    } else
        return HTML_UNKNOWN;
    if (p[-1] == '/')
        SKIP_BLANKS(*s);
    while ((IS_ALNUM(**s) || **s == '_') && p - cmdstr < MAX_CMD_LEN) {
        *(p++) = TOLOWER(**s);
        (*s)++;
    }
    if (p - cmdstr == MAX_CMD_LEN) {
        /* buffer overflow: perhaps caused by bad HTML source */
        *s = save + 1;
        return HTML_UNKNOWN;
    }
    *p = '\0';

    /* hash search */
    cmd = getHash_si(&tagtable, cmdstr, HTML_UNKNOWN);
    while (**s && **s != '>')
        (*s)++;
    if (**s == '>')
        (*s)++;
    return cmd;
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
            const char* c = html_quote_char(obuf->anchor.accesskey);
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

void push_render_image(Str str, int width, int limit,
    struct html_feed_environ* h_env)
{
    struct readbuffer* obuf = h_env->obuf;
    int indent = h_env->envs[h_env->envc].indent;

    push_spaces(obuf, 1, (limit - width) / 2);
    push_str(obuf, width, str, PC_ASCII);
    push_spaces(obuf, 1, (limit - width + 1) / 2);
    if (width > 0)
        flushline(h_env, obuf, indent, 0, h_env->limit);
}

void do_blankline(struct html_feed_environ* h_env, struct readbuffer* obuf,
    int indent, int indent_incr, int width)
{
    if (h_env->blank_lines == 0)
        flushline(h_env, obuf, indent, 1, width);
}

void save_fonteffect(struct html_feed_environ* h_env, struct readbuffer* obuf)
{
    if (obuf->fontstat_sp < FONT_STACK_SIZE)
        memcpy(obuf->fontstat_stack[obuf->fontstat_sp], obuf->fontstat, FONTSTAT_SIZE);
    if (obuf->fontstat_sp < INT_MAX)
        obuf->fontstat_sp++;
    if (obuf->in_bold)
        push_tag(obuf, "</b>", HTML_N_B);
    if (obuf->in_italic)
        push_tag(obuf, "</i>", HTML_N_I);
    if (obuf->in_under)
        push_tag(obuf, "</u>", HTML_N_U);
    if (obuf->in_strike)
        push_tag(obuf, "</s>", HTML_N_S);
    if (obuf->in_ins)
        push_tag(obuf, "</ins>", HTML_N_INS);
    memset(obuf->fontstat, 0, FONTSTAT_SIZE);
}

void restore_fonteffect(struct html_feed_environ* h_env, struct readbuffer* obuf)
{
    if (obuf->fontstat_sp > 0)
        obuf->fontstat_sp--;
    if (obuf->fontstat_sp < FONT_STACK_SIZE)
        memcpy(obuf->fontstat, obuf->fontstat_stack[obuf->fontstat_sp], FONTSTAT_SIZE);
    if (obuf->in_bold)
        push_tag(obuf, "<b>", HTML_B);
    if (obuf->in_italic)
        push_tag(obuf, "<i>", HTML_I);
    if (obuf->in_under)
        push_tag(obuf, "<u>", HTML_U);
    if (obuf->in_strike)
        push_tag(obuf, "<s>", HTML_S);
    if (obuf->in_ins)
        push_tag(obuf, "<ins>", HTML_INS);
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

static void close_anchor(struct html_feed_environ* h_env, struct readbuffer* obuf)
{
    if (obuf->anchor.url) {
        int i;
        char* p = NULL;
        int is_erased = 0;

        for (i = obuf->tag_sp - 1; i >= 0; i--) {
            if (obuf->tag_stack[i]->cmd == HTML_A)
                break;
        }
        if (i < 0 && obuf->anchor.hseq > 0 && Strlastchar(obuf->line) == ' ') {
            Strshrink(obuf->line, 1);
            obuf->pos--;
            is_erased = 1;
        }

        if (i >= 0 || (p = has_hidden_link(obuf, HTML_A))) {
            if (obuf->anchor.hseq > 0) {
                HTMLlineproc0(ANSP, h_env, true);
                set_space_to_prevchar(obuf->prevchar);
            } else {
                if (i >= 0) {
                    obuf->tag_sp--;
                    memcpy(&obuf->tag_stack[i], &obuf->tag_stack[i + 1], (obuf->tag_sp - i) * sizeof(struct cmdtable*));
                } else {
                    passthrough(obuf, p, 1);
                }
                memset((void*)&obuf->anchor, 0, sizeof(obuf->anchor));
                return;
            }
            is_erased = 0;
        }
        if (is_erased) {
            Strcat_char(obuf->line, ' ');
            obuf->pos++;
        }

        push_tag(obuf, "</a>", HTML_N_A);
    }
    memset((void*)&obuf->anchor, 0, sizeof(obuf->anchor));
}

#define CLOSE_P                                                            \
    if (obuf->flag & RB_P) {                                               \
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit); \
        RB_RESTORE_FLAG(obuf);                                             \
        obuf->flag &= ~RB_P;                                               \
    }

#define HTML5_CLOSE_A                  \
    do {                               \
        if (obuf->flag & RB_HTML5) {   \
            close_anchor(h_env, obuf); \
        }                              \
    } while (0)

#define CLOSE_A                         \
    do {                                \
        CLOSE_P;                        \
        if (!(obuf->flag & RB_HTML5)) { \
            close_anchor(h_env, obuf);  \
        }                               \
    } while (0)

#define CLOSE_DT                            \
    if (obuf->flag & RB_IN_DT) {            \
        obuf->flag &= ~RB_IN_DT;            \
        HTMLlineproc0("</b>", h_env, true); \
    }

#define PUSH_ENV(cmd)                                                             \
    if (++h_env->envc_real < h_env->nenv) {                                       \
        ++h_env->envc;                                                            \
        envs[h_env->envc].env = cmd;                                              \
        envs[h_env->envc].count = 0;                                              \
        if (h_env->envc <= MAX_INDENT_LEVEL)                                      \
            envs[h_env->envc].indent = envs[h_env->envc - 1].indent + IndentIncr; \
        else                                                                      \
            envs[h_env->envc].indent = envs[h_env->envc - 1].indent;              \
    }

#define PUSH_ENV_NOINDENT(cmd)                                   \
    if (++h_env->envc_real < h_env->nenv) {                      \
        ++h_env->envc;                                           \
        envs[h_env->envc].env = cmd;                             \
        envs[h_env->envc].count = 0;                             \
        envs[h_env->envc].indent = envs[h_env->envc - 1].indent; \
    }

#define POP_ENV                           \
    if (h_env->envc_real-- < h_env->nenv) \
        h_env->envc--;

static int
ul_type(struct HtmlTagParsed* tag, int default_type)
{
    char* p;
    if (parsedtag_get_value(tag, ATTR_TYPE, &p)) {
        if (!strcasecmp(p, "disc"))
            return (int)'d';
        else if (!strcasecmp(p, "circle"))
            return (int)'c';
        else if (!strcasecmp(p, "square"))
            return (int)'s';
    }
    return default_type;
}

struct table* tables[MAX_TABLE];
struct table_mode table_mode[MAX_TABLE];
wc_ces content_charset = 0;
wc_ces meta_charset = 0;

int table_width(struct html_feed_environ* h_env, int table_level)
{
    int width;
    if (table_level < 0)
        return 0;
    width = tables[table_level]->total_width;
    if (table_level > 0 || width > 0)
        return width;
    return h_env->limit - h_env->envs[h_env->envc].indent;
}

int HTMLtagproc1(struct HtmlTagParsed* tag, struct html_feed_environ* h_env)
{
    char *p, *q, *r;
    int i, w, x, y, z, count, width;
    struct readbuffer* obuf = h_env->obuf;
    struct environment* envs = h_env->envs;
    Str tmp;
    int hseq;
    int cmd;
    char* id = NULL;

    cmd = tag->tagid;

    if (obuf->flag & RB_PRE) {
        switch (cmd) {
        case HTML_NOBR:
        case HTML_N_NOBR:
        case HTML_PRE_INT:
        case HTML_N_PRE_INT:
            return 1;
        }
    }

    switch (cmd) {
    case HTML_B:
        if (obuf->in_bold < FONTSTAT_MAX)
            obuf->in_bold++;
        if (obuf->in_bold > 1)
            return 1;
        return 0;
    case HTML_N_B:
        if (obuf->in_bold == 1 && close_effect0(obuf, HTML_B))
            obuf->in_bold = 0;
        if (obuf->in_bold > 0) {
            obuf->in_bold--;
            if (obuf->in_bold == 0)
                return 0;
        }
        return 1;
    case HTML_I:
        if (obuf->in_italic < FONTSTAT_MAX)
            obuf->in_italic++;
        if (obuf->in_italic > 1)
            return 1;
        return 0;
    case HTML_N_I:
        if (obuf->in_italic == 1 && close_effect0(obuf, HTML_I))
            obuf->in_italic = 0;
        if (obuf->in_italic > 0) {
            obuf->in_italic--;
            if (obuf->in_italic == 0)
                return 0;
        }
        return 1;
    case HTML_U:
        if (obuf->in_under < FONTSTAT_MAX)
            obuf->in_under++;
        if (obuf->in_under > 1)
            return 1;
        return 0;
    case HTML_N_U:
        if (obuf->in_under == 1 && close_effect0(obuf, HTML_U))
            obuf->in_under = 0;
        if (obuf->in_under > 0) {
            obuf->in_under--;
            if (obuf->in_under == 0)
                return 0;
        }
        return 1;
    case HTML_EM:
        HTMLlineproc0("<i>", h_env, true);
        return 1;
    case HTML_N_EM:
        HTMLlineproc0("</i>", h_env, true);
        return 1;
    case HTML_STRONG:
        HTMLlineproc0("<b>", h_env, true);
        return 1;
    case HTML_N_STRONG:
        HTMLlineproc0("</b>", h_env, true);
        return 1;
    case HTML_Q:
        if (DisplayCharset != WC_CES_US_ASCII) {
            HTMLlineproc0((obuf->q_level & 1 ? "&lsquo;" : "&ldquo;"), h_env, true);
            obuf->q_level += 1;
        } else
            HTMLlineproc0("`", h_env, true);
        return 1;
    case HTML_N_Q:
        if (DisplayCharset != WC_CES_US_ASCII) {
            obuf->q_level -= 1;
            HTMLlineproc0((obuf->q_level & 1 ? "&rsquo;" : "&rdquo;"), h_env, true);
        } else
            HTMLlineproc0("'", h_env, true);
        return 1;
    case HTML_FIGURE:
    case HTML_N_FIGURE:
    case HTML_P:
    case HTML_N_P:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P)) {
            flushline(h_env, obuf, envs[h_env->envc].indent, 1, h_env->limit);
            do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                h_env->limit);
        }
        obuf->flag |= RB_IGNORE_P;
        if (cmd == HTML_P) {
            set_alignment(obuf, tag);
            obuf->flag |= RB_P;
        }
        return 1;
    case HTML_FIGCAPTION:
    case HTML_N_FIGCAPTION:
    case HTML_BR:
        flushline(h_env, obuf, envs[h_env->envc].indent, 1, h_env->limit);
        h_env->blank_lines = 0;
        return 1;
    case HTML_H:
        if (!(obuf->flag & (RB_PREMODE | RB_IGNORE_P))) {
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
            do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                h_env->limit);
        }
        HTMLlineproc0("<b>", h_env, true);
        set_alignment(obuf, tag);
        return 1;
    case HTML_N_H:
        HTMLlineproc0("</b>", h_env, true);
        if (!(obuf->flag & RB_PREMODE)) {
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        }
        do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        RB_RESTORE_FLAG(obuf);
        close_anchor(h_env, obuf);
        obuf->flag |= RB_IGNORE_P;
        return 1;
    case HTML_UL:
    case HTML_OL:
    case HTML_BLQ:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P)) {
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
            if (!(obuf->flag & RB_PREMODE) && (h_env->envc == 0 || cmd == HTML_BLQ))
                do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                    h_env->limit);
        }
        PUSH_ENV(cmd);
        if (cmd == HTML_UL || cmd == HTML_OL) {
            if (parsedtag_get_value(tag, ATTR_START, &count)) {
                envs[h_env->envc].count = count - 1;
            }
        }
        if (cmd == HTML_OL) {
            envs[h_env->envc].type = '1';
            if (parsedtag_get_value(tag, ATTR_TYPE, &p)) {
                envs[h_env->envc].type = (int)*p;
            }
        }
        if (cmd == HTML_UL)
            envs[h_env->envc].type = ul_type(tag, 0);
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        return 1;
    case HTML_N_UL:
    case HTML_N_OL:
    case HTML_N_DL:
    case HTML_N_BLQ:
    case HTML_N_DD:
        CLOSE_DT;
        CLOSE_A;
        if (h_env->envc > 0) {
            flushline(h_env, obuf, envs[h_env->envc - 1].indent, 0,
                h_env->limit);
            POP_ENV;
            if (!(obuf->flag & RB_PREMODE) && (h_env->envc == 0 || cmd == HTML_N_BLQ)) {
                do_blankline(h_env, obuf,
                    envs[h_env->envc].indent,
                    IndentIncr, h_env->limit);
                obuf->flag |= RB_IGNORE_P;
            }
        }
        close_anchor(h_env, obuf);
        return 1;
    case HTML_DL:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P)) {
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
            if (!(obuf->flag & RB_PREMODE) && envs[h_env->envc].env != HTML_DL
                && envs[h_env->envc].env != HTML_DL_COMPACT
                && envs[h_env->envc].env != HTML_DD)
                do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                    h_env->limit);
        }
        PUSH_ENV_NOINDENT(cmd);
        if (parsedtag_exists(tag, ATTR_COMPACT))
            envs[h_env->envc].env = HTML_DL_COMPACT;
        obuf->flag |= RB_IGNORE_P;
        return 1;
    case HTML_LI:
        CLOSE_A;
        CLOSE_DT;
        if (h_env->envc > 0) {
            Str num;
            flushline(h_env, obuf,
                envs[h_env->envc - 1].indent, 0, h_env->limit);
            envs[h_env->envc].count++;
            if (parsedtag_get_value(tag, ATTR_VALUE, &p)) {
                count = atoi(p);
                if (count > 0)
                    envs[h_env->envc].count = count;
                else
                    envs[h_env->envc].count = 0;
            }
            switch (envs[h_env->envc].env) {
            case HTML_UL:
                envs[h_env->envc].type = ul_type(tag, envs[h_env->envc].type);
                for (i = 0; i < IndentIncr - 3; i++)
                    push_charp(obuf, 1, NBSP, PC_ASCII);
                tmp = Strnew();
                switch (envs[h_env->envc].type) {
                case 'd':
                    push_symbol(tmp, UL_SYMBOL_DISC, symbol_width, 1);
                    break;
                case 'c':
                    push_symbol(tmp, UL_SYMBOL_CIRCLE, symbol_width, 1);
                    break;
                case 's':
                    push_symbol(tmp, UL_SYMBOL_SQUARE, symbol_width, 1);
                    break;
                default:
                    push_symbol(tmp,
                        UL_SYMBOL((h_env->envc_real - 1) % MAX_UL_LEVEL), symbol_width,
                        1);
                    break;
                }
                if (symbol_width == 1)
                    push_charp(obuf, 1, NBSP, PC_ASCII);
                push_str(obuf, symbol_width, tmp, PC_ASCII);
                push_charp(obuf, 1, NBSP, PC_ASCII);
                set_space_to_prevchar(obuf->prevchar);
                break;
            case HTML_OL:
                if (parsedtag_get_value(tag, ATTR_TYPE, &p))
                    envs[h_env->envc].type = (int)*p;
                switch ((envs[h_env->envc].count > 0) ? envs[h_env->envc].type : '1') {
                case 'i':
                    num = romanNumeral(envs[h_env->envc].count);
                    break;
                case 'I':
                    num = romanNumeral(envs[h_env->envc].count);
                    Strupper(num);
                    break;
                case 'a':
                    num = romanAlphabet(envs[h_env->envc].count);
                    break;
                case 'A':
                    num = romanAlphabet(envs[h_env->envc].count);
                    Strupper(num);
                    break;
                default:
                    num = Sprintf("%d", envs[h_env->envc].count);
                    break;
                }
                if (IndentIncr >= 4)
                    Strcat_charp(num, ". ");
                else
                    Strcat_char(num, '.');
                push_spaces(obuf, 1, IndentIncr - num->length);
                push_str(obuf, num->length, num, PC_ASCII);
                if (IndentIncr >= 4)
                    set_space_to_prevchar(obuf->prevchar);
                break;
            default:
                push_spaces(obuf, 1, IndentIncr);
                break;
            }
        } else {
            flushline(h_env, obuf, 0, 0, h_env->limit);
        }
        obuf->flag |= RB_IGNORE_P;
        return 1;
    case HTML_DT:
        CLOSE_A;
        if (h_env->envc == 0 || (h_env->envc_real < h_env->nenv && envs[h_env->envc].env != HTML_DL && envs[h_env->envc].env != HTML_DL_COMPACT)) {
            PUSH_ENV_NOINDENT(HTML_DL);
        }
        if (h_env->envc > 0) {
            flushline(h_env, obuf,
                envs[h_env->envc - 1].indent, 0, h_env->limit);
        }
        if (!(obuf->flag & RB_IN_DT)) {
            HTMLlineproc0("<b>", h_env, true);
            obuf->flag |= RB_IN_DT;
        }
        obuf->flag |= RB_IGNORE_P;
        return 1;
    case HTML_N_DT:
        if (!(obuf->flag & RB_IN_DT)) {
            return 1;
        }
        obuf->flag &= ~RB_IN_DT;
        HTMLlineproc0("</b>", h_env, true);
        if (h_env->envc > 0 && envs[h_env->envc].env == HTML_DL)
            flushline(h_env, obuf,
                envs[h_env->envc - 1].indent, 0, h_env->limit);
        return 1;
    case HTML_DD:
        CLOSE_A;
        CLOSE_DT;
        if (envs[h_env->envc].env == HTML_DL || envs[h_env->envc].env == HTML_DL_COMPACT) {
            PUSH_ENV(HTML_DD);
        }

        if (h_env->envc > 0 && envs[h_env->envc - 1].env == HTML_DL_COMPACT) {
            if (obuf->pos > envs[h_env->envc].indent)
                flushline(h_env, obuf, envs[h_env->envc].indent, 0,
                    h_env->limit);
            else
                push_spaces(obuf, 1, envs[h_env->envc].indent - obuf->pos);
        } else
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        /* obuf->flag |= RB_IGNORE_P; */
        return 1;
    case HTML_TITLE:
        close_anchor(h_env, obuf);
        process_title(tag);
        obuf->flag |= RB_TITLE;
        obuf->end_tag = HTML_N_TITLE;
        return 1;
    case HTML_N_TITLE:
        if (!(obuf->flag & RB_TITLE))
            return 1;
        obuf->flag &= ~RB_TITLE;
        obuf->end_tag = 0;
        tmp = process_n_title(tag);
        if (tmp)
            HTMLlineproc0(tmp->ptr, h_env, true);
        return 1;
    case HTML_TITLE_ALT:
        if (parsedtag_get_value(tag, ATTR_TITLE, &p))
            h_env->title = html_unquote(p);
        return 0;
    case HTML_FRAMESET:
        PUSH_ENV(cmd);
        push_charp(obuf, 9, "--FRAME--", PC_ASCII);
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        return 0;
    case HTML_N_FRAMESET:
        if (h_env->envc > 0) {
            POP_ENV;
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        }
        return 0;
    case HTML_NOFRAMES:
        CLOSE_A;
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        obuf->flag |= (RB_NOFRAMES | RB_IGNORE_P);
        /* istr = str; */
        return 1;
    case HTML_N_NOFRAMES:
        CLOSE_A;
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        obuf->flag &= ~RB_NOFRAMES;
        return 1;
    case HTML_FRAME:
        q = r = NULL;
        parsedtag_get_value(tag, ATTR_SRC, &q);
        parsedtag_get_value(tag, ATTR_NAME, &r);
        if (q) {
            q = html_quote(q);
            push_tag(obuf, Sprintf("<a hseq=\"%d\" href=\"%s\">", cur_hseq++, q)->ptr, HTML_A);
            if (r)
                q = html_quote(r);
            push_charp(obuf, get_strwidth(q), q, PC_ASCII);
            push_tag(obuf, "</a>", HTML_N_A);
        }
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        return 0;
    case HTML_HR:
        close_anchor(h_env, obuf);
        tmp = process_hr(tag, h_env->limit, envs[h_env->envc].indent);
        HTMLlineproc0(tmp->ptr, h_env, true);
        set_space_to_prevchar(obuf->prevchar);
        return 1;
    case HTML_PRE:
        x = parsedtag_exists(tag, ATTR_FOR_TABLE);
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P)) {
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
            if (!x)
                do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                    h_env->limit);
        } else
            fillline(obuf, envs[h_env->envc].indent);
        obuf->flag |= (RB_PRE | RB_IGNORE_P);
        /* istr = str; */
        return 1;
    case HTML_N_PRE:
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        if (!(obuf->flag & RB_IGNORE_P)) {
            do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                h_env->limit);
            obuf->flag |= RB_IGNORE_P;
            h_env->blank_lines++;
        }
        obuf->flag &= ~RB_PRE;
        close_anchor(h_env, obuf);
        return 1;
    case HTML_PRE_INT:
        i = obuf->line->length;
        append_tags(obuf);
        if (!(obuf->flag & RB_SPECIAL)) {
            set_breakpoint(obuf, obuf->line->length - i);
        }
        obuf->flag |= RB_PRE_INT;
        return 0;
    case HTML_N_PRE_INT:
        push_tag(obuf, "</pre_int>", HTML_N_PRE_INT);
        obuf->flag &= ~RB_PRE_INT;
        if (!(obuf->flag & RB_SPECIAL) && obuf->pos > obuf->bp.pos) {
            set_prevchar(obuf->prevchar, "", 0);
            obuf->prev_ctype = PC_CTRL;
        }
        return 1;
    case HTML_NOBR:
        obuf->flag |= RB_NOBR;
        obuf->nobr_level++;
        return 0;
    case HTML_N_NOBR:
        if (obuf->nobr_level > 0)
            obuf->nobr_level--;
        if (obuf->nobr_level == 0)
            obuf->flag &= ~RB_NOBR;
        return 0;
    case HTML_PRE_PLAIN:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P)) {
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
            do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                h_env->limit);
        }
        obuf->flag |= (RB_PRE | RB_IGNORE_P);
        return 1;
    case HTML_N_PRE_PLAIN:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P)) {
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
            do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                h_env->limit);
            obuf->flag |= RB_IGNORE_P;
        }
        obuf->flag &= ~RB_PRE;
        return 1;
    case HTML_LISTING:
    case HTML_XMP:
    case HTML_PLAINTEXT:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P)) {
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
            do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                h_env->limit);
        }
        obuf->flag |= (RB_PLAIN | RB_IGNORE_P);
        switch (cmd) {
        case HTML_LISTING:
            obuf->end_tag = HTML_N_LISTING;
            break;
        case HTML_XMP:
            obuf->end_tag = HTML_N_XMP;
            break;
        case HTML_PLAINTEXT:
            obuf->end_tag = MAX_HTMLTAG;
            break;
        }
        return 1;
    case HTML_N_LISTING:
    case HTML_N_XMP:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P)) {
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
            do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                h_env->limit);
            obuf->flag |= RB_IGNORE_P;
        }
        obuf->flag &= ~RB_PLAIN;
        obuf->end_tag = 0;
        return 1;
    case HTML_SCRIPT:
        obuf->flag |= RB_SCRIPT;
        obuf->end_tag = HTML_N_SCRIPT;
        return 1;
    case HTML_STYLE:
        obuf->flag |= RB_STYLE;
        obuf->end_tag = HTML_N_STYLE;
        return 1;
    case HTML_N_SCRIPT:
        obuf->flag &= ~RB_SCRIPT;
        obuf->end_tag = 0;
        return 1;
    case HTML_N_STYLE:
        obuf->flag &= ~RB_STYLE;
        obuf->end_tag = 0;
        return 1;
    case HTML_A:
        if (obuf->anchor.url)
            close_anchor(h_env, obuf);

        hseq = 0;

        if (parsedtag_get_value(tag, ATTR_HREF, &p))
            obuf->anchor.url = Strnew_charp(p)->ptr;
        if (parsedtag_get_value(tag, ATTR_TARGET, &p))
            obuf->anchor.target = Strnew_charp(p)->ptr;
        if (parsedtag_get_value(tag, ATTR_REFERER, &p))
            obuf->anchor.referer = Strnew_charp(p)->ptr;
        if (parsedtag_get_value(tag, ATTR_TITLE, &p))
            obuf->anchor.title = Strnew_charp(p)->ptr;
        if (parsedtag_get_value(tag, ATTR_ACCESSKEY, &p))
            obuf->anchor.accesskey = (unsigned char)*p;
        if (parsedtag_get_value(tag, ATTR_HSEQ, &hseq))
            obuf->anchor.hseq = hseq;

        if (hseq == 0 && obuf->anchor.url) {
            obuf->anchor.hseq = cur_hseq;
            tmp = process_anchor(tag, h_env->tagbuf->ptr);
            push_tag(obuf, tmp->ptr, HTML_A);
            return 1;
        }
        return 0;
    case HTML_N_A:
        close_anchor(h_env, obuf);
        return 1;
    case HTML_IMG:
        if (parsedtag_exists(tag, ATTR_USEMAP))
            HTML5_CLOSE_A;
        tmp = process_img(tag, h_env->limit);
        if (need_number) {
            tmp = Strnew_m_charp(getLinkNumberStr(-1)->ptr, tmp->ptr, NULL);
            need_number = 0;
        }
        HTMLlineproc0(tmp->ptr, h_env, true);
        return 1;
    case HTML_IMG_ALT:
        if (parsedtag_get_value(tag, ATTR_SRC, &p))
            obuf->img_alt = Strnew_charp(p);
        i = 0;
        if (parsedtag_get_value(tag, ATTR_TOP_MARGIN, &i)) {
            if ((short)i > obuf->top_margin)
                obuf->top_margin = (short)i;
        }
        i = 0;
        if (parsedtag_get_value(tag, ATTR_BOTTOM_MARGIN, &i)) {
            if ((short)i > obuf->bottom_margin)
                obuf->bottom_margin = (short)i;
        }
        return 0;
    case HTML_N_IMG_ALT:
        if (obuf->img_alt) {
            if (!close_effect0(obuf, HTML_IMG_ALT))
                push_tag(obuf, "</img_alt>", HTML_N_IMG_ALT);
            obuf->img_alt = NULL;
        }
        return 1;
    case HTML_INPUT_ALT:
        i = 0;
        if (parsedtag_get_value(tag, ATTR_TOP_MARGIN, &i)) {
            if ((short)i > obuf->top_margin)
                obuf->top_margin = (short)i;
        }
        i = 0;
        if (parsedtag_get_value(tag, ATTR_BOTTOM_MARGIN, &i)) {
            if ((short)i > obuf->bottom_margin)
                obuf->bottom_margin = (short)i;
        }
        if (parsedtag_get_value(tag, ATTR_HSEQ, &hseq)) {
            obuf->input_alt.hseq = hseq;
        }
        if (parsedtag_get_value(tag, ATTR_FID, &i)) {
            obuf->input_alt.fid = i;
        }
        if (parsedtag_get_value(tag, ATTR_TYPE, &p)) {
            obuf->input_alt.type = Strnew_charp(p);
        }
        if (parsedtag_get_value(tag, ATTR_VALUE, &p)) {
            obuf->input_alt.value = Strnew_charp(p);
        }
        if (parsedtag_get_value(tag, ATTR_NAME, &p)) {
            obuf->input_alt.name = Strnew_charp(p);
        }
        obuf->input_alt.in = 1;
        return 0;
    case HTML_N_INPUT_ALT:
        if (obuf->input_alt.in) {
            if (!close_effect0(obuf, HTML_INPUT_ALT))
                push_tag(obuf, "</input_alt>", HTML_N_INPUT_ALT);
            obuf->input_alt.hseq = 0;
            obuf->input_alt.fid = -1;
            obuf->input_alt.in = 0;
            obuf->input_alt.type = NULL;
            obuf->input_alt.name = NULL;
            obuf->input_alt.value = NULL;
        }
        return 1;
    case HTML_TABLE:
        close_anchor(h_env, obuf);
        if (obuf->table_level + 1 >= MAX_TABLE)
            break;
        obuf->table_level++;
        w = BORDER_NONE;
        /* x: cellspacing, y: cellpadding */
        x = 2;
        y = 1;
        z = 0;
        width = 0;
        if (parsedtag_exists(tag, ATTR_BORDER)) {
            if (parsedtag_get_value(tag, ATTR_BORDER, &w)) {
                if (w > 2)
                    w = BORDER_THICK;
                else if (w < 0) { /* weird */
                    w = BORDER_THIN;
                }
            } else
                w = BORDER_THIN;
        }
        if (DisplayBorders && w == BORDER_NONE)
            w = BORDER_THIN;
        if (parsedtag_get_value(tag, ATTR_WIDTH, &i)) {
            if (obuf->table_level == 0)
                width = REAL_WIDTH(i, h_env->limit - envs[h_env->envc].indent);
            else
                width = RELATIVE_WIDTH(i);
        }
        if (parsedtag_exists(tag, ATTR_HBORDER))
            w = BORDER_NOWIN;
#define MAX_CELLSPACING 1000
#define MAX_CELLPADDING 1000
#define MAX_VSPACE 1000
        parsedtag_get_value(tag, ATTR_CELLSPACING, &x);
        parsedtag_get_value(tag, ATTR_CELLPADDING, &y);
        parsedtag_get_value(tag, ATTR_VSPACE, &z);
        if (x < 0)
            x = 0;
        if (y < 0)
            y = 0;
        if (z < 0)
            z = 0;
        if (x > MAX_CELLSPACING)
            x = MAX_CELLSPACING;
        if (y > MAX_CELLPADDING)
            y = MAX_CELLPADDING;
        if (z > MAX_VSPACE)
            z = MAX_VSPACE;
        parsedtag_get_value(tag, ATTR_ID, &id);
        tables[obuf->table_level] = begin_table(w, x, y, z);
        if (id != NULL)
            tables[obuf->table_level]->id = Strnew_charp(id);
        table_mode[obuf->table_level].pre_mode = 0;
        table_mode[obuf->table_level].indent_level = 0;
        table_mode[obuf->table_level].nobr_level = 0;
        table_mode[obuf->table_level].caption = 0;
        table_mode[obuf->table_level].end_tag = 0; /* HTML_UNKNOWN */
        tables[obuf->table_level]->total_width = width;
        return 1;
    case HTML_N_TABLE:
        /* should be processed in HTMLlineproc() */
        return 1;
    case HTML_CENTER:
        CLOSE_A;
        if (!(obuf->flag & (RB_PREMODE | RB_IGNORE_P)))
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        RB_SAVE_FLAG(obuf);
        if (DisableCenter)
            RB_SET_ALIGN(obuf, RB_LEFT);
        else
            RB_SET_ALIGN(obuf, RB_CENTER);
        return 1;
    case HTML_N_CENTER:
        CLOSE_A;
        if (!(obuf->flag & RB_PREMODE))
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        RB_RESTORE_FLAG(obuf);
        return 1;
    case HTML_DIV:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P))
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        set_alignment(obuf, tag);
        return 1;
    case HTML_N_DIV:
        CLOSE_A;
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        RB_RESTORE_FLAG(obuf);
        return 1;
    case HTML_DIV_INT:
        CLOSE_P;
        if (!(obuf->flag & RB_IGNORE_P))
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        set_alignment(obuf, tag);
        return 1;
    case HTML_N_DIV_INT:
        CLOSE_P;
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        RB_RESTORE_FLAG(obuf);
        return 1;
    case HTML_FORM:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P))
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        tmp = process_form(tag);
        if (tmp)
            HTMLlineproc0(tmp->ptr, h_env, true);
        return 1;
    case HTML_N_FORM:
        CLOSE_A;
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        obuf->flag |= RB_IGNORE_P;
        process_n_form();
        return 1;
    case HTML_INPUT:
        close_anchor(h_env, obuf);
        tmp = process_input(tag);
        if (tmp)
            HTMLlineproc0(tmp->ptr, h_env, true);
        return 1;
    case HTML_BUTTON:
        HTML5_CLOSE_A;
        tmp = process_button(tag);
        if (tmp)
            HTMLlineproc0(tmp->ptr, h_env, true);
        return 1;
    case HTML_N_BUTTON:
        tmp = process_n_button();
        if (tmp)
            HTMLlineproc0(tmp->ptr, h_env, true);
        return 1;
    case HTML_SELECT:
        close_anchor(h_env, obuf);
        tmp = process_select(tag);
        if (tmp)
            HTMLlineproc0(tmp->ptr, h_env, true);
        obuf->flag |= RB_INSELECT;
        obuf->end_tag = HTML_N_SELECT;
        return 1;
    case HTML_N_SELECT:
        obuf->flag &= ~RB_INSELECT;
        obuf->end_tag = 0;
        tmp = process_n_select();
        if (tmp)
            HTMLlineproc0(tmp->ptr, h_env, true);
        return 1;
    case HTML_OPTION:
        /* nothing */
        return 1;
    case HTML_TEXTAREA:
        close_anchor(h_env, obuf);
        tmp = process_textarea(tag, h_env->limit);
        if (tmp)
            HTMLlineproc0(tmp->ptr, h_env, true);
        obuf->flag |= RB_INTXTA;
        obuf->end_tag = HTML_N_TEXTAREA;
        return 1;
    case HTML_N_TEXTAREA:
        obuf->flag &= ~RB_INTXTA;
        obuf->end_tag = 0;
        tmp = process_n_textarea();
        if (tmp)
            HTMLlineproc0(tmp->ptr, h_env, true);
        return 1;
    case HTML_ISINDEX:
        p = "";
        q = "!CURRENT_URL!";
        parsedtag_get_value(tag, ATTR_PROMPT, &p);
        parsedtag_get_value(tag, ATTR_ACTION, &q);
        tmp = Strnew_m_charp("<form method=get action=\"",
            html_quote(q),
            "\">",
            html_quote(p),
            "<input type=text name=\"\" accept></form>",
            NULL);
        HTMLlineproc0(tmp->ptr, h_env, true);
        return 1;
    case HTML_DOCTYPE:
        if (!parsedtag_exists(tag, ATTR_PUBLIC)) {
            obuf->flag |= RB_HTML5;
        }
        return 1;
    case HTML_META:
        p = q = r = NULL;
        parsedtag_get_value(tag, ATTR_HTTP_EQUIV, &p);
        parsedtag_get_value(tag, ATTR_CONTENT, &q);
        parsedtag_get_value(tag, ATTR_CHARSET, &r);
        if (r) {
            /* <meta charset=""> */
            SKIP_BLANKS(r);
            meta_charset = wc_guess_charset(r, 0);
        } else if (p && q && !strcasecmp(p, "Content-Type") && (q = strcasestr(q, "charset")) != NULL) {
            q += 7;
            SKIP_BLANKS(q);
            if (*q == '=') {
                q++;
                SKIP_BLANKS(q);
                meta_charset = wc_guess_charset(q, 0);
            }
        } else if (p && q && !strcasecmp(p, "refresh")) {
            int refresh_interval;
            tmp = NULL;
            refresh_interval = getMetaRefreshParam(q, &tmp);
            if (tmp) {
                q = html_quote(tmp->ptr);
                tmp = Sprintf("Refresh (%d sec) <a href=\"%s\">%s</a>",
                    refresh_interval, q, q);
            } else if (refresh_interval > 0)
                tmp = Sprintf("Refresh (%d sec)", refresh_interval);
            if (tmp) {
                HTMLlineproc0(tmp->ptr, h_env, true);
                do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                    h_env->limit);
            }
        }
        return 1;
    case HTML_BASE:

        p = NULL;
        if (parsedtag_get_value(tag, ATTR_HREF, &p)) {
            cur_baseURL = New(struct Url);
            *cur_baseURL = parseUrl(p, NULL);
        }

    case HTML_MAP:
    case HTML_N_MAP:
    case HTML_AREA:
        return 0;
    case HTML_DEL:
        switch (displayInsDel) {
        case DISPLAY_INS_DEL_SIMPLE:
            obuf->flag |= RB_DEL;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            HTMLlineproc0("<U>[DEL:</U>", h_env, true);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            if (obuf->in_strike < FONTSTAT_MAX)
                obuf->in_strike++;
            if (obuf->in_strike == 1) {
                push_tag(obuf, "<s>", HTML_S);
            }
            break;
        }
        return 1;
    case HTML_N_DEL:
        switch (displayInsDel) {
        case DISPLAY_INS_DEL_SIMPLE:
            obuf->flag &= ~RB_DEL;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            HTMLlineproc0("<U>:DEL]</U>", h_env, true);
        case DISPLAY_INS_DEL_FONTIFY:
            if (obuf->in_strike == 0)
                return 1;
            if (obuf->in_strike == 1 && close_effect0(obuf, HTML_S))
                obuf->in_strike = 0;
            if (obuf->in_strike > 0) {
                obuf->in_strike--;
                if (obuf->in_strike == 0) {
                    push_tag(obuf, "</s>", HTML_N_S);
                }
            }
            break;
        }
        return 1;
    case HTML_S:
        switch (displayInsDel) {
        case DISPLAY_INS_DEL_SIMPLE:
            obuf->flag |= RB_S;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            HTMLlineproc0("<U>[S:</U>", h_env, true);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            if (obuf->in_strike < FONTSTAT_MAX)
                obuf->in_strike++;
            if (obuf->in_strike == 1) {
                push_tag(obuf, "<s>", HTML_S);
            }
            break;
        }
        return 1;
    case HTML_N_S:
        switch (displayInsDel) {
        case DISPLAY_INS_DEL_SIMPLE:
            obuf->flag &= ~RB_S;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            HTMLlineproc0("<U>:S]</U>", h_env, true);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            if (obuf->in_strike == 0)
                return 1;
            if (obuf->in_strike == 1 && close_effect0(obuf, HTML_S))
                obuf->in_strike = 0;
            if (obuf->in_strike > 0) {
                obuf->in_strike--;
                if (obuf->in_strike == 0) {
                    push_tag(obuf, "</s>", HTML_N_S);
                }
            }
        }
        return 1;
    case HTML_INS:
        switch (displayInsDel) {
        case DISPLAY_INS_DEL_SIMPLE:
            break;
        case DISPLAY_INS_DEL_NORMAL:
            HTMLlineproc0("<U>[INS:</U>", h_env, true);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            if (obuf->in_ins < FONTSTAT_MAX)
                obuf->in_ins++;
            if (obuf->in_ins == 1) {
                push_tag(obuf, "<ins>", HTML_INS);
            }
            break;
        }
        return 1;
    case HTML_N_INS:
        switch (displayInsDel) {
        case DISPLAY_INS_DEL_SIMPLE:
            break;
        case DISPLAY_INS_DEL_NORMAL:
            HTMLlineproc0("<U>:INS]</U>", h_env, true);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            if (obuf->in_ins == 0)
                return 1;
            if (obuf->in_ins == 1 && close_effect0(obuf, HTML_INS))
                obuf->in_ins = 0;
            if (obuf->in_ins > 0) {
                obuf->in_ins--;
                if (obuf->in_ins == 0) {
                    push_tag(obuf, "</ins>", HTML_N_INS);
                }
            }
            break;
        }
        return 1;
    case HTML_SUP:
        if (!(obuf->flag & (RB_DEL | RB_S)))
            HTMLlineproc0("^", h_env, true);
        return 1;
    case HTML_N_SUP:
        return 1;
    case HTML_SUB:
        if (!(obuf->flag & (RB_DEL | RB_S)))
            HTMLlineproc0("[", h_env, true);
        return 1;
    case HTML_N_SUB:
        if (!(obuf->flag & (RB_DEL | RB_S)))
            HTMLlineproc0("]", h_env, true);
        return 1;
    case HTML_FONT:
    case HTML_N_FONT:
    case HTML_NOP:
        return 1;
    case HTML_BGSOUND:
        if (view_unseenobject) {
            if (parsedtag_get_value(tag, ATTR_SRC, &p)) {
                Str s;
                q = html_quote(p);
                s = Sprintf("<A HREF=\"%s\">bgsound(%s)</A>", q, q);
                HTMLlineproc0(s->ptr, h_env, true);
            }
        }
        return 1;
    case HTML_EMBED:
        HTML5_CLOSE_A;
        if (view_unseenobject) {
            if (parsedtag_get_value(tag, ATTR_SRC, &p)) {
                Str s;
                q = html_quote(p);
                s = Sprintf("<A HREF=\"%s\">embed(%s)</A>", q, q);
                HTMLlineproc0(s->ptr, h_env, true);
            }
        }
        return 1;
    case HTML_APPLET:
        if (view_unseenobject) {
            if (parsedtag_get_value(tag, ATTR_ARCHIVE, &p)) {
                Str s;
                q = html_quote(p);
                s = Sprintf("<A HREF=\"%s\">applet archive(%s)</A>", q, q);
                HTMLlineproc0(s->ptr, h_env, true);
            }
        }
        return 1;
    case HTML_BODY:
        if (view_unseenobject) {
            if (parsedtag_get_value(tag, ATTR_BACKGROUND, &p)) {
                Str s;
                q = html_quote(p);
                s = Sprintf("<IMG SRC=\"%s\" ALT=\"bg image(%s)\"><BR>", q, q);
                HTMLlineproc0(s->ptr, h_env, true);
            }
        }
    case HTML_N_HEAD:
        if (obuf->flag & RB_TITLE)
            HTMLlineproc0("</title>", h_env, true);
    case HTML_HEAD:
    case HTML_N_BODY:
        return 1;
    default:
        /* obuf->prevchar = '\0'; */
        return 0;
    }
    /* not reached */
    return 0;
}

void completeHTMLstream(struct html_feed_environ* h_env, struct readbuffer* obuf)
{
    close_anchor(h_env, obuf);
    if (obuf->img_alt) {
        push_tag(obuf, "</img_alt>", HTML_N_IMG_ALT);
        obuf->img_alt = NULL;
    }
    if (obuf->input_alt.in) {
        push_tag(obuf, "</input_alt>", HTML_N_INPUT_ALT);
        obuf->input_alt.hseq = 0;
        obuf->input_alt.fid = -1;
        obuf->input_alt.in = 0;
        obuf->input_alt.type = NULL;
        obuf->input_alt.name = NULL;
        obuf->input_alt.value = NULL;
    }
    if (obuf->in_bold) {
        push_tag(obuf, "</b>", HTML_N_B);
        obuf->in_bold = 0;
    }
    if (obuf->in_italic) {
        push_tag(obuf, "</i>", HTML_N_I);
        obuf->in_italic = 0;
    }
    if (obuf->in_under) {
        push_tag(obuf, "</u>", HTML_N_U);
        obuf->in_under = 0;
    }
    if (obuf->in_strike) {
        push_tag(obuf, "</s>", HTML_N_S);
        obuf->in_strike = 0;
    }
    if (obuf->in_ins) {
        push_tag(obuf, "</ins>", HTML_N_INS);
        obuf->in_ins = 0;
    }
    if (obuf->flag & RB_INTXTA)
        HTMLlineproc0("</textarea>", h_env, true);
    /* for unbalanced select tag */
    if (obuf->flag & RB_INSELECT)
        HTMLlineproc0("</select>", h_env, true);
    if (obuf->flag & RB_TITLE)
        HTMLlineproc0("</title>", h_env, true);

    /* for unbalanced table tag */
    if (obuf->table_level >= MAX_TABLE)
        obuf->table_level = MAX_TABLE - 1;

    while (obuf->table_level >= 0) {
        int tmp = obuf->table_level;
        table_mode[obuf->table_level].pre_mode
            &= ~(TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN);
        HTMLlineproc0("</table>", h_env, true);
        if (obuf->table_level >= tmp)
            break;
    }
}

void init_henv(struct html_feed_environ* h_env, struct readbuffer* obuf,
    struct environment* envs, int nenv, TextLineList* buf,
    int limit, int indent)
{
    envs[0].indent = indent;

    obuf->line = Strnew();
    obuf->cprop = 0;
    obuf->pos = 0;
    obuf->prevchar = Strnew_size(8);
    set_space_to_prevchar(obuf->prevchar);
    obuf->flag = RB_IGNORE_P;
    obuf->flag_sp = 0;
    obuf->status = R_ST_NORMAL;
    obuf->table_level = -1;
    obuf->nobr_level = 0;
    obuf->q_level = 0;
    memset((void*)&obuf->anchor, 0, sizeof(obuf->anchor));
    obuf->img_alt = 0;
    obuf->input_alt.hseq = 0;
    obuf->input_alt.fid = -1;
    obuf->input_alt.in = 0;
    obuf->input_alt.type = NULL;
    obuf->input_alt.name = NULL;
    obuf->input_alt.value = NULL;
    obuf->in_bold = 0;
    obuf->in_italic = 0;
    obuf->in_under = 0;
    obuf->in_strike = 0;
    obuf->in_ins = 0;
    obuf->prev_ctype = PC_ASCII;
    obuf->tag_sp = 0;
    obuf->fontstat_sp = 0;
    obuf->top_margin = 0;
    obuf->bottom_margin = 0;
    obuf->bp.init_flag = 1;
    set_breakpoint(obuf, 0);

    h_env->buf = buf;
    h_env->f = NULL;
    h_env->obuf = obuf;
    h_env->tagbuf = Strnew();
    h_env->limit = limit;
    h_env->maxlimit = 0;
    h_env->envs = envs;
    h_env->nenv = nenv;
    h_env->envc = 0;
    h_env->envc_real = 0;
    h_env->title = NULL;
    h_env->blank_lines = 0;
}

static int
is_period_char(unsigned char* ch)
{
    switch (*ch) {
    case ',':
    case '.':
    case ':':
    case ';':
    case '?':
    case '!':
    case ')':
    case ']':
    case '}':
    case '>':
        return 1;
    default:
        return 0;
    }
}

static int
is_beginning_char(unsigned char* ch)
{
    switch (*ch) {
    case '(':
    case '[':
    case '{':
    case '`':
    case '<':
        return 1;
    default:
        return 0;
    }
}

static int
is_combining_char(unsigned char* ch)
{
    Lineprop ctype = get_mctype(ch);

    if (ctype & PC_WCHAR2)
        return 1;
    return 0;
}

static int
is_word_char(unsigned char* ch)
{
    Lineprop ctype = get_mctype(ch);

    if (ctype & (PC_CTRL | PC_KANJI | PC_UNKNOWN))
        return 0;
    if (ctype & (PC_WCHAR1 | PC_WCHAR2))
        return 1;

    if (IS_ALNUM(*ch))
        return 1;

    switch (*ch) {
    case ',':
    case '.':
    case ':':
    case '\"': /* " */
    case '\'':
    case '$':
    case '%':
    case '*':
    case '+':
    case '-':
    case '@':
    case '~':
    case '_':
        return 1;
    }
    if (*ch == NBSP_CODE)
        return 1;
    return 0;
}

int is_boundary(unsigned char* ch1, unsigned char* ch2)
{
    if (!*ch1 || !*ch2)
        return 1;

    if (*ch1 == ' ' && *ch2 == ' ')
        return 0;

    if (*ch1 != ' ' && is_period_char(ch2))
        return 0;

    if (*ch2 != ' ' && is_beginning_char(ch1))
        return 0;

    if (is_combining_char(ch2))
        return 0;
    if (is_word_char(ch1) && is_word_char(ch2))
        return 0;

    return 1;
}
