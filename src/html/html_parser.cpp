#include "html_parser.h"
#include "form_item.h"
#include "form.h"
#include "etc.h"
#include "line_layout.h"
#include "http_response.h"
#include "symbol.h"
#include "url_quote.h"
#include "form.h"
#include "html_tag.h"
#include "quote.h"
#include "ctrlcode.h"
#include "myctype.h"
#include "Str.h"
#include "readbuffer.h"
#include "alloc.h"
#include "hash.h"
#include "textlist.h"
#include "table.h"

HtmlParser::HtmlParser() { textarea_str = (Str **)New_N(Str *, max_textarea); }

static int sloppy_parse_line(char **str) {
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

void purgeline(struct html_feed_environ *h_env) {
  char *p, *q;
  Str *tmp;
  TextLine *tl;

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
Str *HtmlParser::getLinkNumberStr(int correction) const {
  return Sprintf("[%d]", cur_hseq + correction);
}

char *HtmlParser::has_hidden_link(struct readbuffer *obuf, int cmd) const {
  Str *line = obuf->line;
  struct link_stack *p;

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

void HtmlParser::push_link(int cmd, int offset, int pos) {
  auto p = (struct link_stack *)New(struct link_stack);
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

void HtmlParser::append_tags(struct readbuffer *obuf) {
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
      this->push_link(obuf->tag_stack[i]->cmd, obuf->line->length, obuf->pos);
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

void HtmlParser::push_tag(struct readbuffer *obuf, const char *cmdname,
                          int cmd) {
  obuf->tag_stack[obuf->tag_sp] = (struct cmdtable *)New(struct cmdtable);
  obuf->tag_stack[obuf->tag_sp]->cmdname = allocStr(cmdname, -1);
  obuf->tag_stack[obuf->tag_sp]->cmd = cmd;
  obuf->tag_sp++;
  if (obuf->tag_sp >= TAG_STACK_SIZE || obuf->flag & (RB_SPECIAL & ~RB_NOBR)) {
    this->append_tags(obuf);
  }
}

void HtmlParser::push_nchars(struct readbuffer *obuf, int width,
                             const char *str, int len, Lineprop mode) {
  this->append_tags(obuf);
  Strcat_charp_n(obuf->line, str, len);
  obuf->pos += width;
  if (width > 0) {
    set_prevchar(obuf->prevchar, str, len);
    obuf->prev_ctype = mode;
  }
  obuf->flag |= RB_NFLUSHED;
}

void HtmlParser::push_charp(readbuffer *obuf, int width, const char *str,
                            Lineprop mode) {
  this->push_nchars(obuf, width, str, strlen(str), mode);
}

void HtmlParser::push_str(readbuffer *obuf, int width, Str *str,
                          Lineprop mode) {
  this->push_nchars(obuf, width, str->ptr, str->length, mode);
}

void HtmlParser::check_breakpoint(struct readbuffer *obuf, int pre_mode,
                                  const char *ch) {
  int tlen, len = obuf->line->length;

  this->append_tags(obuf);
  if (pre_mode)
    return;
  tlen = obuf->line->length - len;
  if (tlen > 0 ||
      is_boundary((unsigned char *)obuf->prevchar->ptr, (unsigned char *)ch))
    set_breakpoint(obuf, tlen);
}

void HtmlParser::push_char(struct readbuffer *obuf, int pre_mode, char ch) {
  this->check_breakpoint(obuf, pre_mode, &ch);
  Strcat_char(obuf->line, ch);
  obuf->pos++;
  set_prevchar(obuf->prevchar, &ch, 1);
  if (ch != ' ')
    obuf->prev_ctype = PC_ASCII;
  obuf->flag |= RB_NFLUSHED;
}

void HtmlParser::push_spaces(struct readbuffer *obuf, int pre_mode, int width) {
  if (width <= 0)
    return;
  this->check_breakpoint(obuf, pre_mode, " ");
  for (int i = 0; i < width; i++)
    Strcat_char(obuf->line, ' ');
  obuf->pos += width;
  set_space_to_prevchar(obuf->prevchar);
  obuf->flag |= RB_NFLUSHED;
}

void HtmlParser::proc_mchar(struct readbuffer *obuf, int pre_mode, int width,
                            const char **str, Lineprop mode) {
  this->check_breakpoint(obuf, pre_mode, *str);
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

void HtmlParser::push_render_image(Str *str, int width, int limit,
                                   struct html_feed_environ *h_env) {
  struct readbuffer *obuf = h_env->obuf;
  int indent = h_env->envs[h_env->envc].indent;

  this->push_spaces(obuf, 1, (limit - width) / 2);
  this->push_str(obuf, width, str, PC_ASCII);
  this->push_spaces(obuf, 1, (limit - width + 1) / 2);
  if (width > 0)
    flushline(h_env, obuf, indent, 0, h_env->limit);
}

#define MAX_CMD_LEN 128

int gethtmlcmd(const char **s) {
  extern Hash_si tagtable;
  char cmdstr[MAX_CMD_LEN];
  const char *p = cmdstr;
  const char *save = *s;
  int cmd;

  (*s)++;
  /* first character */
  if (IS_ALNUM(**s) || **s == '_' || **s == '/') {
    *(char *)(p++) = TOLOWER(**s);
    (*s)++;
  } else
    return HTML_UNKNOWN;
  if (p[-1] == '/')
    SKIP_BLANKS(*s);
  while ((IS_ALNUM(**s) || **s == '_') && p - cmdstr < MAX_CMD_LEN) {
    *(char *)(p++) = TOLOWER(**s);
    (*s)++;
  }
  if (p - cmdstr == MAX_CMD_LEN) {
    /* buffer overflow: perhaps caused by bad HTML source */
    *s = save + 1;
    return HTML_UNKNOWN;
  }
  *(char *)p = '\0';

  /* hash search */
  cmd = getHash_si(&tagtable, cmdstr, HTML_UNKNOWN);
  while (**s && **s != '>')
    (*s)++;
  if (**s == '>')
    (*s)++;
  return cmd;
}
void HtmlParser::passthrough(struct readbuffer *obuf, char *str, int back) {
  int cmd;
  Str *tok = Strnew();
  char *str_bak;

  if (back) {
    Str *str_save = Strnew_charp(str);
    Strshrink(obuf->line, obuf->line->ptr + obuf->line->length - str);
    str = str_save->ptr;
  }
  while (*str) {
    str_bak = str;
    if (sloppy_parse_line(&str)) {
      const char *q = str_bak;
      cmd = gethtmlcmd(&q);
      if (back) {
        struct link_stack *p;
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

void HtmlParser::fillline(struct readbuffer *obuf, int indent) {
  this->push_spaces(obuf, 1, indent - obuf->pos);
  obuf->flag &= ~RB_NFLUSHED;
}

void HtmlParser::flushline(struct html_feed_environ *h_env,
                           struct readbuffer *obuf, int indent, int force,
                           int width) {
  TextLineList *buf = h_env->buf;
  FILE *f = h_env->f;
  Str *line = obuf->line, *pass = NULL;
  char *hidden_anchor = NULL, *hidden_img = NULL, *hidden_bold = NULL,
       *hidden_under = NULL, *hidden_italic = NULL, *hidden_strike = NULL,
       *hidden_ins = NULL, *hidden_input = NULL, *hidden = NULL;

#ifdef DEBUG
  if (w3m_debug) {
    FILE *df = fopen("zzzproc1", "a");
    fprintf(df, "flushline(%s,%d,%d,%d)\n", obuf->line->ptr, indent, force,
            width);
    if (buf) {
      TextLineListItem *p;
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
    char *tp = &line->ptr[obuf->bp.len - obuf->bp.tlen];
    char *ep = &line->ptr[line->length];

    if (obuf->bp.pos == obuf->pos && tp <= ep && tp > line->ptr &&
        tp[-1] == ' ') {
      bcopy(tp, tp - 1, ep - tp + 1);
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
    TextLine *lbuf = newTextLine(line, obuf->pos);
    if (RB_GET_ALIGN(obuf) == RB_CENTER) {
      align(lbuf, width, ALIGN_CENTER);
    } else if (RB_GET_ALIGN(obuf) == RB_RIGHT) {
      align(lbuf, width, ALIGN_RIGHT);
    } else if (RB_GET_ALIGN(obuf) == RB_LEFT && obuf->flag & RB_INTABLE) {
      align(lbuf, width, ALIGN_LEFT);
    }
#ifdef FORMAT_NICE
    else if (obuf->flag & RB_FILL) {
      char *p;
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
#endif /* FORMAT_NICE */
#ifdef TABLE_DEBUG
    if (w3m_debug) {
      FILE *f = fopen("zzzproc1", "a");
      fprintf(f, "pos=%d,%d, maxlimit=%d\n", visible_length(lbuf->line->ptr),
              lbuf->pos, h_env->maxlimit);
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
    Str *tmp = Strnew(), *tmp2 = Strnew();

#define APPEND(str)                                                            \
  if (buf)                                                                     \
    appendTextLine(buf, (str), 0);                                             \
  else if (f)                                                                  \
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
    Str *tmp;
    if (obuf->anchor.hseq > 0)
      obuf->anchor.hseq = -obuf->anchor.hseq;
    tmp = Sprintf("<A HSEQ=\"%d\" HREF=\"", obuf->anchor.hseq);
    Strcat_charp(tmp, html_quote(obuf->anchor.url));
    if (obuf->anchor.target) {
      Strcat_charp(tmp, "\" TARGET=\"");
      Strcat_charp(tmp, html_quote(obuf->anchor.target));
    }
    if (obuf->anchor.option.use_referer()) {
      Strcat_charp(tmp, "\" REFERER=\"");
      Strcat_charp(tmp, html_quote(obuf->anchor.option.referer.c_str()));
    }
    if (obuf->anchor.title) {
      Strcat_charp(tmp, "\" TITLE=\"");
      Strcat_charp(tmp, html_quote(obuf->anchor.title));
    }
    if (obuf->anchor.accesskey) {
      auto c = html_quote_char(obuf->anchor.accesskey);
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
    Str *tmp = Strnew_charp("<IMG_ALT SRC=\"");
    Strcat_charp(tmp, html_quote(obuf->img_alt->ptr));
    Strcat_charp(tmp, "\">");
    push_tag(obuf, tmp->ptr, HTML_IMG_ALT);
  }
  if (!hidden_input && obuf->input_alt.in) {
    Str *tmp;
    if (obuf->input_alt.hseq > 0)
      obuf->input_alt.hseq = -obuf->input_alt.hseq;
    tmp = Sprintf("<INPUT_ALT hseq=\"%d\" fid=\"%d\" name=\"%s\" type=\"%s\" "
                  "value=\"%s\">",
                  obuf->input_alt.hseq, obuf->input_alt.fid,
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

void HtmlParser::do_blankline(struct html_feed_environ *h_env,
                              struct readbuffer *obuf, int indent,
                              int indent_incr, int width) {
  if (h_env->blank_lines == 0)
    flushline(h_env, obuf, indent, 1, width);
}

int HtmlParser::close_effect0(struct readbuffer *obuf, int cmd) {
  int i;
  char *p;

  for (i = obuf->tag_sp - 1; i >= 0; i--) {
    if (obuf->tag_stack[i]->cmd == cmd)
      break;
  }
  if (i >= 0) {
    obuf->tag_sp--;
    bcopy(&obuf->tag_stack[i + 1], &obuf->tag_stack[i],
          (obuf->tag_sp - i) * sizeof(struct cmdtable *));
    return 1;
  } else if ((p = this->has_hidden_link(obuf, cmd)) != NULL) {
    this->passthrough(obuf, p, 1);
    return 1;
  }
  return 0;
}

void HtmlParser::close_anchor(struct html_feed_environ *h_env,
                              struct readbuffer *obuf) {
  if (obuf->anchor.url) {
    int i;
    char *p = NULL;
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
        this->HTMLlineproc1(ANSP, h_env);
        set_space_to_prevchar(obuf->prevchar);
      } else {
        if (i >= 0) {
          obuf->tag_sp--;
          bcopy(&obuf->tag_stack[i + 1], &obuf->tag_stack[i],
                (obuf->tag_sp - i) * sizeof(struct cmdtable *));
        } else {
          passthrough(obuf, p, 1);
        }
        bzero((void *)&obuf->anchor, sizeof(obuf->anchor));
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
  bzero((void *)&obuf->anchor, sizeof(obuf->anchor));
}

void HtmlParser::save_fonteffect(struct html_feed_environ *h_env,
                                 struct readbuffer *obuf) {
  if (obuf->fontstat_sp < FONT_STACK_SIZE)
    bcopy(obuf->fontstat, obuf->fontstat_stack[obuf->fontstat_sp],
          FONTSTAT_SIZE);
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
  bzero(obuf->fontstat, FONTSTAT_SIZE);
}

void HtmlParser::restore_fonteffect(struct html_feed_environ *h_env,
                                    struct readbuffer *obuf) {
  if (obuf->fontstat_sp > 0)
    obuf->fontstat_sp--;
  if (obuf->fontstat_sp < FONT_STACK_SIZE)
    bcopy(obuf->fontstat_stack[obuf->fontstat_sp], obuf->fontstat,
          FONTSTAT_SIZE);
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

void HtmlParser::process_title(struct HtmlTag *tag) {
  if (pre_title) {
    return;
  }
  cur_title = Strnew();
}

Str *HtmlParser::process_n_title(struct HtmlTag *tag) {
  Str *tmp;

  if (pre_title)
    return NULL;
  if (!cur_title)
    return NULL;
  Strremovefirstspaces(cur_title);
  Strremovetrailingspaces(cur_title);
  tmp = Strnew_m_charp("<title_alt title=\"", html_quote(cur_title->ptr), "\">",
                       NULL);
  pre_title = cur_title;
  cur_title = NULL;
  return tmp;
}

void HtmlParser::feed_title(const char *str) {
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

Str *HtmlParser::process_textarea(struct HtmlTag *tag, int width) {
  Str *tmp = NULL;
  const char *p;
#define TEXTAREA_ATTR_COL_MAX 4096
#define TEXTAREA_ATTR_ROWS_MAX 4096

  if (cur_form_id() < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp = process_form(HtmlTag::parse(&s, true));
  }

  p = "";
  tag->parsedtag_get_value(ATTR_NAME, &p);
  cur_textarea = Strnew_charp(p);
  cur_textarea_size = 20;
  if (tag->parsedtag_get_value(ATTR_COLS, &p)) {
    cur_textarea_size = atoi(p);
    if (strlen(p) > 0 && p[strlen(p) - 1] == '%')
      cur_textarea_size = width * cur_textarea_size / 100 - 2;
    if (cur_textarea_size <= 0) {
      cur_textarea_size = 20;
    } else if (cur_textarea_size > TEXTAREA_ATTR_COL_MAX) {
      cur_textarea_size = TEXTAREA_ATTR_COL_MAX;
    }
  }
  cur_textarea_rows = 1;
  if (tag->parsedtag_get_value(ATTR_ROWS, &p)) {
    cur_textarea_rows = atoi(p);
    if (cur_textarea_rows <= 0) {
      cur_textarea_rows = 1;
    } else if (cur_textarea_rows > TEXTAREA_ATTR_ROWS_MAX) {
      cur_textarea_rows = TEXTAREA_ATTR_ROWS_MAX;
    }
  }
  cur_textarea_readonly = tag->parsedtag_exists(ATTR_READONLY);
  if (n_textarea >= max_textarea) {
    max_textarea *= 2;
    textarea_str = (Str **)New_Reuse(Str *, textarea_str, max_textarea);
  }
  textarea_str[n_textarea] = Strnew();
  ignore_nl_textarea = true;

  return tmp;
}

Str *HtmlParser::process_n_textarea() {
  Str *tmp;
  int i;

  if (cur_textarea == NULL)
    return NULL;

  tmp = Strnew();
  Strcat(tmp, Sprintf("<pre_int>[<input_alt hseq=\"%d\" fid=\"%d\" "
                      "type=textarea name=\"%s\" size=%d rows=%d "
                      "top_margin=%d textareanumber=%d",
                      this->cur_hseq, cur_form_id(),
                      html_quote(cur_textarea->ptr), cur_textarea_size,
                      cur_textarea_rows, cur_textarea_rows - 1, n_textarea));
  if (cur_textarea_readonly)
    Strcat_charp(tmp, " readonly");
  Strcat_charp(tmp, "><u>");
  for (i = 0; i < cur_textarea_size; i++)
    Strcat_char(tmp, ' ');
  Strcat_charp(tmp, "</u></input_alt>]</pre_int>\n");
  this->cur_hseq++;
  n_textarea++;
  cur_textarea = NULL;

  return tmp;
}

void HtmlParser::feed_textarea(const char *str) {
  if (cur_textarea == NULL)
    return;
  if (ignore_nl_textarea) {
    if (*str == '\r')
      str++;
    if (*str == '\n')
      str++;
  }
  ignore_nl_textarea = false;
  while (*str) {
    if (*str == '&')
      Strcat_charp(textarea_str[n_textarea], getescapecmd(&str));
    else if (*str == '\n') {
      Strcat_charp(textarea_str[n_textarea], "\r\n");
      str++;
    } else if (*str == '\r')
      str++;
    else
      Strcat_char(textarea_str[n_textarea], *(str++));
  }
}

#define INITIAL_FORM_SIZE 10

Str *HtmlParser::process_form_int(struct HtmlTag *tag, int fid) {
  const char *p, *q, *r, *s, *tg, *n;

  p = "get";
  tag->parsedtag_get_value(ATTR_METHOD, &p);
  q = "!CURRENT_URL!";
  tag->parsedtag_get_value(ATTR_ACTION, &q);
  q = Strnew(url_quote(remove_space(q)))->ptr;
  r = NULL;
  s = NULL;
  tag->parsedtag_get_value(ATTR_ENCTYPE, &s);
  tg = NULL;
  tag->parsedtag_get_value(ATTR_TARGET, &tg);
  n = NULL;
  tag->parsedtag_get_value(ATTR_NAME, &n);

  if (fid < 0) {
    form_max++;
    form_sp++;
    fid = form_max;
  } else { /* <form_int> */
    if (form_max < fid)
      form_max = fid;
    form_sp = fid;
  }
  if (forms_size == 0) {
    forms_size = INITIAL_FORM_SIZE;
    forms = (FormList **)New_N(FormList *, forms_size);
    form_stack = (int *)NewAtom_N(int, forms_size);
  }
  if (forms_size <= form_max) {
    forms_size += form_max;
    forms = (FormList **)New_Reuse(FormList *, forms, forms_size);
    form_stack = (int *)New_Reuse(int, form_stack, forms_size);
  }
  form_stack[form_sp] = fid;

  forms[fid] = newFormList(q, p, r, s, tg, n, NULL);
  return NULL;
}

Str *HtmlParser::process_n_form(void) {
  if (form_sp >= 0)
    form_sp--;
  return NULL;
}

#define PPUSH(p, c)                                                            \
  {                                                                            \
    outp[pos] = (p);                                                           \
    outc[pos] = (c);                                                           \
    pos++;                                                                     \
  }
#define PSIZE                                                                  \
  if (out_size <= pos + 1) {                                                   \
    out_size = pos * 3 / 2;                                                    \
    outc = (char *)New_Reuse(char, outc, out_size);                            \
    outp = (Lineprop *)New_Reuse(Lineprop, outp, out_size);                    \
  }

static int ex_efct(int ex) {
  int effect = 0;

  if (!ex)
    return 0;

  if (ex & PE_EX_ITALIC)
    effect |= PE_EX_ITALIC_E;

  if (ex & PE_EX_INSERT)
    effect |= PE_EX_INSERT_E;

  if (ex & PE_EX_STRIKE)
    effect |= PE_EX_STRIKE_E;

  return effect;
}

#define url_quote_conv(x, c) Strnew(url_quote(x))->ptr

void HtmlParser::HTMLlineproc2body(HttpResponse *res, LineLayout *layout,
                                   Str *(*feed)(), int llimit) {
  static char *outc = NULL;
  static Lineprop *outp = NULL;
  static int out_size = 0;
  Anchor *a_href = NULL, *a_img = NULL, *a_form = NULL;
  const char *p, *q, *r, *s, *t, *str;
  Lineprop mode, effect, ex_effect;
  int pos;
  int nlines;
#ifdef DEBUG
  FILE *debug = NULL;
#endif
  const char *id = NULL;
  int hseq, form_id;
  Str *line;
  const char *endp;
  char symbol = '\0';
  int internal = 0;
  Anchor **a_textarea = NULL;

  if (out_size == 0) {
    out_size = LINELEN;
    outc = (char *)NewAtom_N(char, out_size);
    outp = (Lineprop *)NewAtom_N(Lineprop, out_size);
  }

  n_textarea = -1;
  if (!max_textarea) { /* halfload */
    max_textarea = 10;
    textarea_str = (Str **)New_N(Str *, max_textarea);
    a_textarea = (Anchor **)New_N(Anchor *, max_textarea);
  }

#ifdef DEBUG
  if (w3m_debug)
    debug = fopen("zzzerr", "a");
#endif

  effect = 0;
  ex_effect = 0;
  nlines = 0;
  while ((line = feed()) != NULL) {
#ifdef DEBUG
    if (w3m_debug) {
      Strfputs(line, debug);
      fputc('\n', debug);
    }
#endif
    if (n_textarea >= 0 && *(line->ptr) != '<') { /* halfload */
      Strcat(textarea_str[n_textarea], line);
      continue;
    }
  proc_again:
    if (++nlines == llimit)
      break;
    pos = 0;
#ifdef ENABLE_REMOVE_TRAILINGSPACES
    Strremovetrailingspaces(line);
#endif
    str = line->ptr;
    endp = str + line->length;
    while (str < endp) {
      PSIZE;
      mode = get_mctype(str);
      if ((effect | ex_efct(ex_effect)) & PC_SYMBOL && *str != '<') {
        PPUSH(PC_ASCII | effect | ex_efct(ex_effect), SYMBOL_BASE + symbol);
        str += symbol_width;
      } else if (mode == PC_CTRL || IS_INTSPACE(*str)) {
        PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
        str++;
      } else if (*str != '<' && *str != '&') {
        int len = get_mclen(str);
        PPUSH(mode | effect | ex_efct(ex_effect), *(str++));
        if (--len) {
          mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
          while (len--) {
            PSIZE;
            PPUSH(mode | effect | ex_efct(ex_effect), *(str++));
          }
        }
      } else if (*str == '&') {
        /*
         * & escape processing
         */
        p = getescapecmd(&str);
        while (*p) {
          PSIZE;
          mode = get_mctype(p);
          if (mode == PC_CTRL || IS_INTSPACE(*str)) {
            PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
            p++;
          } else {
            PPUSH(mode | effect | ex_efct(ex_effect), *(p++));
          }
        }
      } else {
        /* tag processing */
        struct HtmlTag *tag;
        if (!(tag = HtmlTag::parse(&str, true)))
          continue;
        switch (tag->tagid) {
        case HTML_B:
          effect |= PE_BOLD;
          break;
        case HTML_N_B:
          effect &= ~PE_BOLD;
          break;
        case HTML_I:
          ex_effect |= PE_EX_ITALIC;
          break;
        case HTML_N_I:
          ex_effect &= ~PE_EX_ITALIC;
          break;
        case HTML_INS:
          ex_effect |= PE_EX_INSERT;
          break;
        case HTML_N_INS:
          ex_effect &= ~PE_EX_INSERT;
          break;
        case HTML_U:
          effect |= PE_UNDER;
          break;
        case HTML_N_U:
          effect &= ~PE_UNDER;
          break;
        case HTML_S:
          ex_effect |= PE_EX_STRIKE;
          break;
        case HTML_N_S:
          ex_effect &= ~PE_EX_STRIKE;
          break;
        case HTML_A:
          p = r = s = NULL;
          q = res->baseTarget;
          t = "";
          hseq = 0;
          id = NULL;
          if (tag->parsedtag_get_value(ATTR_NAME, &id)) {
            id = url_quote_conv(id, name_charset);
            layout->registerName(id, layout->currentLn(), pos);
          }
          if (tag->parsedtag_get_value(ATTR_HREF, &p))
            p = Strnew(url_quote(remove_space(p)))->ptr;
          if (tag->parsedtag_get_value(ATTR_TARGET, &q))
            q = url_quote_conv(q, buf->document_charset);
          if (tag->parsedtag_get_value(ATTR_REFERER, &r))
            r = Strnew(url_quote(r))->ptr;
          tag->parsedtag_get_value(ATTR_TITLE, &s);
          tag->parsedtag_get_value(ATTR_ACCESSKEY, &t);
          tag->parsedtag_get_value(ATTR_HSEQ, &hseq);
          if (hseq > 0) {
            layout->hmarklist()->putHmarker(layout->currentLn(), pos, hseq - 1);
          } else if (hseq < 0) {
            int h = -hseq - 1;
            if (h < (int)layout->hmarklist()->size() &&
                layout->hmarklist()->marks[h].invalid) {
              layout->hmarklist()->marks[h].pos = pos;
              layout->hmarklist()->marks[h].line = layout->currentLn();
              layout->hmarklist()->marks[h].invalid = 0;
              hseq = -hseq;
            }
          }
          if (p) {
            effect |= PE_ANCHOR;
            a_href = layout->href()->putAnchor(p, q, {.referer = r ? r : ""}, s,
                                               *t, layout->currentLn(), pos);
            a_href->hseq = ((hseq > 0) ? hseq : -hseq) - 1;
            a_href->slave = (hseq > 0) ? false : true;
          }
          break;
        case HTML_N_A:
          effect &= ~PE_ANCHOR;
          if (a_href) {
            a_href->end.line = layout->currentLn();
            a_href->end.pos = pos;
            if (a_href->start.line == a_href->end.line &&
                a_href->start.pos == a_href->end.pos) {
              if (a_href->hseq >= 0 &&
                  a_href->hseq < (int)layout->hmarklist()->size()) {
                layout->hmarklist()->marks[a_href->hseq].invalid = 1;
              }
              a_href->hseq = -1;
            }
            a_href = NULL;
          }
          break;

        case HTML_LINK:
          layout->addLink(tag);
          break;

        case HTML_IMG_ALT:
          if (tag->parsedtag_get_value(ATTR_SRC, &p)) {
            s = NULL;
            tag->parsedtag_get_value(ATTR_TITLE, &s);
            p = url_quote_conv(remove_space(p), buf->document_charset);
            a_img = layout->registerImg(p, s, layout->currentLn(), pos);
          }
          effect |= PE_IMAGE;
          break;
        case HTML_N_IMG_ALT:
          effect &= ~PE_IMAGE;
          if (a_img) {
            a_img->end.line = layout->currentLn();
            a_img->end.pos = pos;
          }
          a_img = NULL;
          break;
        case HTML_INPUT_ALT: {
          FormList *form;
          int top = 0, bottom = 0;
          int textareanumber = -1;
          hseq = 0;
          form_id = -1;

          tag->parsedtag_get_value(ATTR_HSEQ, &hseq);
          tag->parsedtag_get_value(ATTR_FID, &form_id);
          tag->parsedtag_get_value(ATTR_TOP_MARGIN, &top);
          tag->parsedtag_get_value(ATTR_BOTTOM_MARGIN, &bottom);
          if (form_id < 0 || form_id > form_max || forms == NULL ||
              forms[form_id] == NULL)
            break; /* outside of <form>..</form> */
          form = forms[form_id];
          if (hseq > 0) {
            int hpos = pos;
            if (*str == '[')
              hpos++;
            layout->hmarklist()->putHmarker(layout->currentLn(), hpos,
                                            hseq - 1);
          } else if (hseq < 0) {
            int h = -hseq - 1;
            int hpos = pos;
            if (*str == '[')
              hpos++;
            if (h < (int)layout->hmarklist()->size() &&
                layout->hmarklist()->marks[h].invalid) {
              layout->hmarklist()->marks[h].pos = hpos;
              layout->hmarklist()->marks[h].line = layout->currentLn();
              layout->hmarklist()->marks[h].invalid = 0;
              hseq = -hseq;
            }
          }

          if (!form->target)
            form->target = res->baseTarget;
          if (a_textarea &&
              tag->parsedtag_get_value(ATTR_TEXTAREANUMBER, &textareanumber)) {
            if (textareanumber >= max_textarea) {
              max_textarea = 2 * textareanumber;
              textarea_str =
                  (Str **)New_Reuse(Str *, textarea_str, max_textarea);
              a_textarea =
                  (Anchor **)New_Reuse(Anchor *, a_textarea, max_textarea);
            }
          }
          a_form =
              layout->registerForm(this, form, tag, layout->currentLn(), pos);
          if (a_textarea && textareanumber >= 0)
            a_textarea[textareanumber] = a_form;
          if (a_form) {
            a_form->hseq = hseq - 1;
            a_form->y = layout->currentLn() - top;
            a_form->rows = 1 + top + bottom;
            if (!tag->parsedtag_exists(ATTR_NO_EFFECT))
              effect |= PE_FORM;
            break;
          }
        }
        case HTML_N_INPUT_ALT:
          effect &= ~PE_FORM;
          if (a_form) {
            a_form->end.line = layout->currentLn();
            a_form->end.pos = pos;
            if (a_form->start.line == a_form->end.line &&
                a_form->start.pos == a_form->end.pos)
              a_form->hseq = -1;
          }
          a_form = NULL;
          break;
        case HTML_MAP:
          // if (tag->parsedtag_get_value( ATTR_NAME, &p)) {
          //   MapList *m = (MapList *)New(MapList);
          //   m->name = Strnew_charp(p);
          //   m->area = newGeneralList();
          //   m->next = buf->maplist;
          //   buf->maplist = m;
          // }
          break;
        case HTML_N_MAP:
          /* nothing to do */
          break;
        case HTML_AREA:
          // if (buf->maplist == NULL) /* outside of <map>..</map> */
          //   break;
          // if (tag->parsedtag_get_value( ATTR_HREF, &p)) {
          //   MapArea *a;
          //   p = url_encode(remove_space(p), base, buf->document_charset);
          //   t = NULL;
          //   tag->parsedtag_get_value( ATTR_TARGET, &t);
          //   q = "";
          //   tag->parsedtag_get_value( ATTR_ALT, &q);
          //   r = NULL;
          //   s = NULL;
          //   a = newMapArea(p, t, q, r, s);
          //   pushValue(buf->maplist->area, (void *)a);
          // }
          break;
        case HTML_FRAMESET:
          // frameset_sp++;
          // if (frameset_sp >= FRAMESTACK_SIZE)
          //   break;
          // frameset_s[frameset_sp] = newFrameSet(tag);
          // if (frameset_s[frameset_sp] == NULL)
          //   break;
          break;
        case HTML_N_FRAMESET:
          // if (frameset_sp >= 0)
          //   frameset_sp--;
          break;
        case HTML_FRAME:
          // if (frameset_sp >= 0 && frameset_sp < FRAMESTACK_SIZE) {
          // }
          break;
        case HTML_BASE:
          if (tag->parsedtag_get_value(ATTR_HREF, &p)) {
            p = Strnew(url_quote(remove_space(p)))->ptr;
            res->baseURL = urlParse(p, res->currentURL);
          }
          if (tag->parsedtag_get_value(ATTR_TARGET, &p))
            res->baseTarget = url_quote_conv(p, buf->document_charset);
          break;
        case HTML_META:
          p = q = NULL;
          tag->parsedtag_get_value(ATTR_HTTP_EQUIV, &p);
          tag->parsedtag_get_value(ATTR_CONTENT, &q);
          if (p && q && !strcasecmp(p, "refresh") && MetaRefresh) {
            Str *tmp = NULL;
            int refresh_interval = getMetaRefreshParam(q, &tmp);
            if (tmp) {
              p = Strnew(url_quote(remove_space(tmp->ptr)))->ptr;
              // TODO:
              // App::instance().task(refresh_interval, FUNCNAME_gorURL, p);
            } else if (refresh_interval > 0) {
              layout->refresh_interval = refresh_interval;
            }
          }
          break;
        case HTML_INTERNAL:
          internal = HTML_INTERNAL;
          break;
        case HTML_N_INTERNAL:
          internal = HTML_N_INTERNAL;
          break;
        case HTML_FORM_INT:
          if (tag->parsedtag_get_value(ATTR_FID, &form_id))
            process_form_int(tag, form_id);
          break;
        case HTML_TEXTAREA_INT:
          if (tag->parsedtag_get_value(ATTR_TEXTAREANUMBER, &n_textarea) &&
              n_textarea >= 0 && n_textarea < max_textarea) {
            textarea_str[n_textarea] = Strnew();
          } else
            n_textarea = -1;
          break;
        case HTML_N_TEXTAREA_INT:
          if (a_textarea && n_textarea >= 0) {
            FormItemList *item = (FormItemList *)a_textarea[n_textarea]->url;
            item->init_value = item->value = textarea_str[n_textarea];
          }
          break;
        case HTML_TITLE_ALT:
          if (tag->parsedtag_get_value(ATTR_TITLE, &p))
            layout->title = html_unquote(p);
          break;
        case HTML_SYMBOL:
          effect |= PC_SYMBOL;
          if (tag->parsedtag_get_value(ATTR_TYPE, &p))
            symbol = (char)atoi(p);
          break;
        case HTML_N_SYMBOL:
          effect &= ~PC_SYMBOL;
          break;

        default:
          break;
        }
#ifdef ID_EXT
        id = NULL;
        if (tag->parsedtag_get_value(ATTR_ID, &id)) {
          id = url_quote_conv(id, name_charset);
          layout->registerName(id, layout->currentLn(), pos);
        }
#endif /* ID_EXT */
      }
    }
    /* end of processing for one line */
    if (!internal) {
      layout->addnewline(outc, outp, pos, -1, nlines);
    }
    if (internal == HTML_N_INTERNAL)
      internal = 0;
    if (str != endp) {
      line = Strsubstr(line, str - line->ptr, endp - str);
      goto proc_again;
    }
  }
#ifdef DEBUG
  if (w3m_debug)
    fclose(debug);
#endif
  for (form_id = 1; form_id <= form_max; form_id++)
    if (forms[form_id])
      forms[form_id]->next = forms[form_id - 1];
  layout->formlist = (form_max >= 0) ? forms[form_max] : NULL;
  if (n_textarea) {
    layout->addMultirowsForm(layout->formitem().get());
  }
}
