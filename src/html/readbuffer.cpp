#include "readbuffer.h"
#include "gc_helper.h"
#include "indep.h"
#include "file.h"
// #include "tagstack.h"
#include "myctype.h"
#include "entity.h"
#include <list>

int sloppy_parse_line(char **str)
{
    if (**str == '<')
    {
        while (**str && **str != '>')
            (*str)++;
        if (**str == '>')
            (*str)++;
        return 1;
    }
    else
    {
        while (**str && **str != '<')
            (*str)++;
        return 0;
    }
}

void Breakpoint::set(const struct readbuffer *obuf, int tag_length)
{
    _len = obuf->line->Size();
    _tlen = tag_length;

    _pos = obuf->pos;
    flag = obuf->flag;
    top_margin = obuf->top_margin;
    bottom_margin = obuf->bottom_margin;

    if (init_flag)
    {
        init_flag = 0;

        anchor = obuf->anchor;
        img_alt = obuf->img_alt;
        fontstat = obuf->fontstat;
        nobr_level = obuf->nobr_level;
        prev_ctype = obuf->prev_ctype;
    }
}

void Breakpoint::back_to(struct readbuffer *obuf)
{
    obuf->pos = _pos;
    obuf->flag = flag;
    obuf->top_margin = top_margin;
    obuf->bottom_margin = bottom_margin;

    obuf->anchor = anchor;
    obuf->img_alt = img_alt;
    obuf->fontstat = fontstat;
    obuf->prev_ctype = prev_ctype;
    if (obuf->flag & RB_NOBR)
        obuf->nobr_level = nobr_level;
}

struct link_item
{
    HtmlTags cmd;
    short offset;
    short pos;
};

static struct std::list<link_item> link_stack;

static void push_link(HtmlTags cmd, int offset, int pos)
{
    link_stack.push_front({});
    auto p = &link_stack.front();
    p->cmd = cmd;
    p->offset = offset;
    p->pos = pos;
}

#define MAX_CMD_LEN 128

static HtmlTags gethtmlcmd(char **s)
{
    char cmdstr[MAX_CMD_LEN];
    char *p = cmdstr;
    char *save = *s;

    (*s)++;
    /* first character */
    if (IS_ALNUM(**s) || **s == '_' || **s == '/')
    {
        *(p++) = TOLOWER(**s);
        (*s)++;
    }
    else
        return HTML_UNKNOWN;
    if (p[-1] == '/')
        SKIP_BLANKS(s);
    while ((IS_ALNUM(**s) || **s == '_') && p - cmdstr < MAX_CMD_LEN)
    {
        *(p++) = TOLOWER(**s);
        (*s)++;
    }
    if (p - cmdstr == MAX_CMD_LEN)
    {
        /* buffer overflow: perhaps caused by bad HTML source */
        *s = save + 1;
        return HTML_UNKNOWN;
    }
    *p = '\0';

    /* hash search */
    //     extern Hash_si tagtable;
    //     int cmd = getHash_si(&tagtable, cmdstr, HTML_UNKNOWN);
    auto cmd = GetTag(cmdstr, HTML_UNKNOWN);
    while (**s && **s != '>')
        (*s)++;
    if (**s == '>')
        (*s)++;
    return cmd;
}

///
///
///
readbuffer::readbuffer()
{
    this->line = Strnew();
    this->cprop = P_UNKNOWN;
    this->pos = 0;
    this->prevchar = Strnew_size(8);
    this->set_space_to_prevchar();
    this->flag = RB_IGNORE_P;
    this->flag_sp = 0;
    this->status = R_ST_NORMAL;
    this->table_level = -1;
    this->nobr_level = 0;
    this->anchor = {};
    this->img_alt = 0;
    this->fontstat = {};
    this->prev_ctype = PC_ASCII;
    this->tag_sp = 0;
    this->fontstat_sp = 0;
    this->top_margin = 0;
    this->bottom_margin = 0;
    this->bp.set(this, 0);
}

void readbuffer::reset()
{
    this->line = Strnew_size(256);
    this->pos = 0;
    this->top_margin = 0;
    this->bottom_margin = 0;
    this->set_space_to_prevchar();
    this->flag &= ~RB_NFLUSHED;
    this->bp = {};
    this->bp.set(this, 0);
    this->prev_ctype = PC_ASCII;
    link_stack.clear();
}

void readbuffer::passthrough(char *str, int back)
{
    Str tok = Strnew();
    char *str_bak;

    if (back)
    {
        Str str_save = Strnew(str);
        this->line->Pop(this->line->ptr + this->line->Size() - str);
        str = str_save->ptr;
    }
    while (*str)
    {
        str_bak = str;
        if (sloppy_parse_line(&str))
        {
            char *q = str_bak;
            auto cmd = gethtmlcmd(&q);
            if (back)
            {
                for (auto p = link_stack.begin(); p != link_stack.end(); ++p)
                {
                    if (p->cmd == cmd)
                    {
                        link_stack.erase(link_stack.begin(), p);
                        break;
                    }
                }
                back = 0;
            }
            else
            {
                tok->Push(str_bak, str - str_bak);
                push_tag(tok->ptr, cmd);
                tok->Clear();
            }
        }
        else
        {
            push_nchars(0, str_bak, str - str_bak, this->prev_ctype);
        }
    }
}

int readbuffer::close_effect0(int cmd)
{
    int i;
    char *p;

    for (i = this->tag_sp - 1; i >= 0; i--)
    {
        if (this->tag_stack[i]->cmd == cmd)
            break;
    }
    if (i >= 0)
    {
        this->tag_sp--;
        bcopy(&this->tag_stack[i + 1], &this->tag_stack[i],
              (this->tag_sp - i) * sizeof(struct cmdtable *));
        return 1;
    }
    else if ((p = has_hidden_link(cmd)) != NULL)
    {
        passthrough(p, 1);
        return 1;
    }
    return 0;
}

char *readbuffer::has_hidden_link(int cmd)
{
    Str line = this->line;
    if (line->Back() != '>')
        return NULL;

    auto p = std::find_if(link_stack.begin(), link_stack.end(), [cmd](auto &p) -> bool {
        return p.cmd == cmd;
    });
    if (p == link_stack.end())
        return NULL;

    if (this->pos == p->pos)
        return line->ptr + p->offset;

    return NULL;
}

void readbuffer::process_idattr(int cmd, HtmlTagPtr tag)
{
    char *id = NULL, *framename = NULL;
    Str idtag = NULL;

    /* 
     * HTML_TABLE is handled by the other process.
     */
    if (cmd == HTML_TABLE)
        return;

    tag->TryGetAttributeValue(ATTR_ID, &id);
    tag->TryGetAttributeValue(ATTR_FRAMENAME, &framename);
    if (id == NULL)
        return;
    if (framename)
        idtag = Sprintf("<_id id=\"%s\" framename=\"%s\">",
                        html_quote(id), html_quote(framename));
    else
        idtag = Sprintf("<_id id=\"%s\">", html_quote(id));
    push_tag(idtag->ptr, HTML_NOP);
}

void readbuffer::proc_escape(const char **str_return)
{
    const char *str = *str_return;
    auto [pos, ech] = ucs4_from_entity(*str_return);
    *str_return = pos.data();
    int n_add = *str_return - str;

    if (ech == -1)
    {
        auto pos = proc_mchar(this->flag & RB_SPECIAL, 1, str, PC_ASCII);
        *str_return = pos.data();
        return;
    }

    auto unicode = SingleCharacter::unicode_to_utf8(ech);
    Lineprop mode;
    auto width = wcwidth(ech);
    if (unicode.size() == 1)
    {
        mode = IS_CNTRL(ech) ? PC_CTRL : PC_ASCII;
    }
    else
    {
        mode = PC_KANJI1;
    }

    // estr = (char *)from_unicode(ech, w3mApp::Instance().InnerCharset);
    auto estr = (const char *)unicode.bytes.data();

    check_breakpoint(this->flag & RB_SPECIAL, estr);
    // auto width = get_strwidth(estr);
    if (width == 1 && ech == (unsigned char)*estr &&
        ech != '&' && ech != '<' && ech != '>')
    {
        if (IS_CNTRL(ech))
            mode = PC_CTRL;
        push_charp(width, estr, mode);
    }
    else
    {
        push_nchars(width, str, n_add, mode);
    }
    this->prevchar->CopyFrom(estr, strlen(estr));
    this->prev_ctype = mode;
}

void readbuffer::push_nchars(int width, const char *str, int len, Lineprop mode)
{
    append_tags();
    this->line->Push(str, len);
    this->pos += width;
    if (width > 0)
    {
        this->prevchar->CopyFrom(str, len);
        this->prev_ctype = mode;
    }
    this->flag |= RB_NFLUSHED;
}

void readbuffer::push_charp(int width, const char *str, Lineprop mode)
{
    push_nchars(width, str, strlen(str), mode);
}

void readbuffer::push_str(int width, Str str, Lineprop mode)
{
    push_nchars(width, str->ptr, str->Size(), mode);
}

void readbuffer::check_breakpoint(int pre_mode, const char *ch)
{
    int tlen, len = this->line->Size();

    append_tags();
    if (pre_mode)
        return;
    tlen = this->line->Size() - len;
    if (tlen > 0 || is_boundary((unsigned char *)this->prevchar->ptr,
                                (unsigned char *)ch))
        this->bp.set(this, tlen);
}

void readbuffer::push_char(int pre_mode, char ch)
{
    check_breakpoint(pre_mode, &ch);
    this->line->Push(ch);
    this->pos++;
    this->prevchar->CopyFrom(&ch, 1);
    if (ch != ' ')
        this->prev_ctype = PC_ASCII;
    this->flag |= RB_NFLUSHED;
}

void readbuffer::PUSH(int c)
{
    push_char(this->flag & RB_SPECIAL, c);
}

void readbuffer::set_space_to_prevchar()
{
    prevchar->CopyFrom(" ", 1);
}

void readbuffer::push_spaces(int pre_mode, int width)
{
    if (width <= 0)
        return;
    check_breakpoint(pre_mode, " ");
    for (int i = 0; i < width; i++)
        this->line->Push(' ');
    this->pos += width;
    set_space_to_prevchar();
    this->flag |= RB_NFLUSHED;
}

void readbuffer::fillline(int indent)
{
    push_spaces(1, indent - this->pos);
    this->flag &= ~RB_NFLUSHED;
}

std::string_view readbuffer::proc_mchar(int pre_mode, int width, std::string_view str, Lineprop mode)
{
    check_breakpoint(pre_mode, str.data());
    this->pos += width;
    auto len = SingleCharacter::as_utf8(str.data()).size();
    this->line->Push(str.data(), len);
    if (width > 0)
    {
        this->prevchar->CopyFrom(str.data(), 1);
        if (str[0] != ' ')
            this->prev_ctype = mode;
    }
    this->flag |= RB_NFLUSHED;
    return str.substr(len);
}

void readbuffer::append_tags()
{
    int i;
    int len = this->line->Size();
    int set_bp = 0;

    for (i = 0; i < this->tag_sp; i++)
    {
        switch (this->tag_stack[i]->cmd)
        {
        case HTML_A:
        case HTML_IMG_ALT:
        case HTML_B:
        case HTML_U:
        case HTML_I:
        case HTML_S:
            push_link(this->tag_stack[i]->cmd, this->line->Size(), this->pos);
            break;
        }
        this->line->Push(this->tag_stack[i]->cmdname);
        switch (this->tag_stack[i]->cmd)
        {
        case HTML_NOBR:
            if (this->nobr_level > 1)
                break;
        case HTML_WBR:
            set_bp = 1;
            break;
        }
    }
    this->tag_sp = 0;
    if (set_bp)
        this->bp.set(this, this->line->Size() - len);
}

void readbuffer::push_tag(const char *cmdname, HtmlTags cmd)
{
    this->tag_stack[this->tag_sp] = New(struct cmdtable);
    this->tag_stack[this->tag_sp]->cmdname = allocStr(cmdname, -1);
    this->tag_stack[this->tag_sp]->cmd = cmd;
    this->tag_sp++;
    if (this->tag_sp >= TAG_STACK_SIZE || this->flag & (RB_SPECIAL & ~RB_NOBR))
        append_tags();
}

void readbuffer::save_fonteffect()
{
    if (this->fontstat_sp < FONT_STACK_SIZE)
        this->fontstat_stack[this->fontstat_sp] = this->fontstat;

    this->fontstat_sp++;
    if (this->fontstat.in_bold)
        push_tag("</b>", HTML_N_B);
    if (this->fontstat.in_italic)
        push_tag("</i>", HTML_N_I);
    if (this->fontstat.in_under)
        push_tag("</u>", HTML_N_U);
    if (this->fontstat.in_strike)
        push_tag("</s>", HTML_N_S);
    if (this->fontstat.in_ins)
        push_tag("</ins>", HTML_N_INS);
    this->fontstat = {};
}

void readbuffer::restore_fonteffect()
{
    if (this->fontstat_sp > 0)
        this->fontstat_sp--;
    if (this->fontstat_sp < FONT_STACK_SIZE)
    {
        this->fontstat = this->fontstat_stack[this->fontstat_sp];
    }
    if (this->fontstat.in_bold)
        push_tag("<b>", HTML_B);
    if (this->fontstat.in_italic)
        push_tag("<i>", HTML_I);
    if (this->fontstat.in_under)
        push_tag("<u>", HTML_U);
    if (this->fontstat.in_strike)
        push_tag("<s>", HTML_S);
    if (this->fontstat.in_ins)
        push_tag("<ins>", HTML_INS);
}

void readbuffer::clear_ignore_p_flag(int cmd)
{
    static int clear_flag_cmd[] = {
        HTML_HR, HTML_UNKNOWN};
    int i;

    for (i = 0; clear_flag_cmd[i] != HTML_UNKNOWN; i++)
    {
        if (cmd == clear_flag_cmd[i])
        {
            this->flag &= ~RB_IGNORE_P;
            return;
        }
    }
}

void readbuffer::set_alignment(HtmlTagPtr tag)
{
    ReadBufferFlags flag = (ReadBufferFlags)-1;

    int align;
    if (tag->TryGetAttributeValue(ATTR_ALIGN, &align))
    {
        switch (align)
        {
        case ALIGN_CENTER:
            flag = RB_CENTER;
            break;
        case ALIGN_RIGHT:
            flag = RB_RIGHT;
            break;
        case ALIGN_LEFT:
            flag = RB_LEFT;
        }
    }

    RB_SAVE_FLAG();
    if (flag != -1)
    {
        RB_SET_ALIGN(flag);
    }
}

TokenStatusTypes next_status(char c, TokenStatusTypes status)
{
    switch (status)
    {
    case R_ST_NORMAL:
        if (c == '<')
        {
            return R_ST_TAG0;
        }
        else if (c == '&')
        {
            return R_ST_AMP;
        }
        return R_ST_NORMAL;

    case R_ST_TAG0:
        if (c == '!')
        {
            return R_ST_CMNT1;
        }
        /* continues to next case */
    case R_ST_TAG:
        if (c == '>')
            return R_ST_NORMAL;
        else if (c == '=')
            return R_ST_EQL;
        return R_ST_TAG;

    case R_ST_EQL:
        if (c == '"')
            return R_ST_DQUOTE;
        else if (c == '\'')
            return R_ST_QUOTE;
        else if (IS_SPACE(c))
            return R_ST_EQL;
        else if (c == '>')
            return R_ST_NORMAL;
        else
            return R_ST_VALUE;
        assert(false);
        return R_ST_EQL;

    case R_ST_QUOTE:
        if (c == '\'')
            return R_ST_TAG;
        return R_ST_QUOTE;

    case R_ST_DQUOTE:
        if (c == '"')
            return R_ST_TAG;
        return R_ST_DQUOTE;

    case R_ST_VALUE:
        if (c == '>')
            return R_ST_NORMAL;
        else if (IS_SPACE(c))
            return R_ST_TAG;
        return R_ST_VALUE;

    case R_ST_AMP:
        if (c == ';')
        {
            return R_ST_NORMAL;
        }
        else if (c != '#' && !IS_ALNUM(c) && c != '_')
        {
            /* something's wrong! */
            return R_ST_NORMAL;
        }
        return R_ST_AMP;

    case R_ST_CMNT1:
        if (c == '-')
            return R_ST_CMNT2;
        else if (c == '>')
            return R_ST_NORMAL;
        return R_ST_IRRTAG;

    case R_ST_CMNT2:
        if (c == '-')
            return R_ST_CMNT;
        else if (c == '>')
            return R_ST_NORMAL;
        return R_ST_IRRTAG;

    case R_ST_CMNT:
        if (c == '-')
            return R_ST_NCMNT1;
        return R_ST_CMNT;

    case R_ST_NCMNT1:
        if (c == '-')
            return R_ST_NCMNT2;
        return R_ST_CMNT;

    case R_ST_NCMNT2:
        if (c == '>')
            return R_ST_NORMAL;
        else if (c == '-')
            return R_ST_NCMNT2;
        else if (IS_SPACE(c))
            return R_ST_NCMNT3;
        return R_ST_CMNT;

    case R_ST_NCMNT3:
        if (c == '>')
            return R_ST_NORMAL;
        else if (c == '-')
            return R_ST_NCMNT1;
        else if (IS_SPACE(c))
            return R_ST_NCMNT3;
        return R_ST_CMNT;

    case R_ST_IRRTAG:
        if (c == '>')
            return R_ST_NORMAL;
        return R_ST_IRRTAG;
    }

    /* notreached */
    assert(false);
    return status;
}

std::tuple<std::string_view, TokenStatusTypes, std::string> read_token(std::string_view instr, TokenStatusTypes status, bool pre)
{
    if (instr.empty())
        return {};

    std::string buf;
    auto p = instr;
    for (; p.size(); p.remove_prefix(1))
    {
        auto prev_status = status;
        status = next_status(p[0], status);
        switch (status)
        {
        case R_ST_NORMAL:
            if (prev_status == R_ST_AMP && p[0] != ';')
            {
                p = std::string_view(p.data() - 1, p.size() + 1);
                break;
            }
            if (prev_status == R_ST_NCMNT2 || prev_status == R_ST_NCMNT3 ||
                prev_status == R_ST_IRRTAG || prev_status == R_ST_CMNT1)
            {
                // if (prev_status == R_ST_CMNT1 && !append && !pre){
                //     buf->Clear();
                if (pre)
                    buf.push_back(p[0]);
                p.remove_prefix(1);
                return {p, status, buf};
            }
            buf.push_back((!pre && IS_SPACE(p[0])) ? ' ' : p[0]);
            if (ST_IS_REAL_TAG(prev_status))
            {
                instr = p.substr(1);
                if (buf.size() < 2 ||
                    buf[buf.size() - 2] != '<' ||
                    buf[buf.size() - 1] != '>')
                    return {instr, status, buf};
                buf.pop_back();
                buf.pop_back();
            }
            break;
        case R_ST_TAG0:
        case R_ST_TAG:
            if (prev_status == R_ST_NORMAL && p != instr)
            {
                return {p, status, buf};
            }
            if (status == R_ST_TAG0 && !REALLY_THE_BEGINNING_OF_A_TAG(p.data()))
            {
                /* it seems that this '<' is not a beginning of a tag */
                /*
                 * buf->Push( "&lt;");
                 */
                buf.push_back('<');
                status = R_ST_NORMAL;
            }
            else
                buf.push_back(p[0]);
            break;
        case R_ST_EQL:
        case R_ST_QUOTE:
        case R_ST_DQUOTE:
        case R_ST_VALUE:
        case R_ST_AMP:
            buf.push_back(p[0]);
            break;
        case R_ST_CMNT:
        case R_ST_IRRTAG:
            if (pre)
                buf.push_back(p[0]);
            else /* if (!append) */
                buf.clear();
            break;
        case R_ST_CMNT1:
        case R_ST_CMNT2:
        case R_ST_NCMNT1:
        case R_ST_NCMNT2:
        case R_ST_NCMNT3:
            /* do nothing */
            if (pre)
                buf.push_back(p[0]);
            break;
        }
    }
    return {p, status, buf};
}
