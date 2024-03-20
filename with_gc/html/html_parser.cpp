#include "html_parser.h"
#include "html_feed_env.h"
#include "textline.h"
#include "entity.h"
#include "anchorlist.h"
#include "form_item.h"
#include "form.h"
#include "etc.h"
#include "line_data.h"
#include "line_layout.h"
#include "symbol.h"
#include "url_quote.h"
#include "form.h"
#include "html_tag.h"
#include "quote.h"
#include "ctrlcode.h"
#include "myctype.h"
#include "Str.h"
#include "readbuffer.h"
#include "hash.h"
#include "table.h"
#include "utf8.h"
#include "stringtoken.h"

#define HR_ATTR_WIDTH_MAX 65535
#define MAX_INPUT_SIZE 80 /* TODO - max should be screen line length */

#define IMG_SYMBOL UL_SYMBOL(12)
#define HR_SYMBOL 26
#define TEXTAREA_ATTR_COL_MAX 4096
#define TEXTAREA_ATTR_ROWS_MAX 4096
#define INITIAL_FORM_SIZE 10

HtmlParser::HtmlParser() {}

void HtmlParser::HTMLlineproc1(const char *x, struct html_feed_environ *y) {
  parseLine(x, y, true);
}

void HtmlParser::CLOSE_DT(readbuffer *obuf, html_feed_environ *h_env) {
  if (obuf->flag & RB_IN_DT) {
    obuf->flag &= ~RB_IN_DT;
    HTMLlineproc1("</b>", h_env);
  }
}

void HtmlParser::CLOSE_P(readbuffer *obuf, html_feed_environ *h_env) {
  if (obuf->flag & RB_P) {
    struct environment *envs = h_env->envs.data();
    flushline(h_env, envs[h_env->envc].indent, 0, h_env->limit);
    obuf->RB_RESTORE_FLAG();
    obuf->flag &= ~RB_P;
  }
}

void HtmlParser::CLOSE_A(readbuffer *obuf, html_feed_environ *h_env) {
  do {
    CLOSE_P(obuf, h_env);
    if (!(obuf->flag & RB_HTML5)) {
      this->close_anchor(h_env);
    }
  } while (0);
}

void HtmlParser::HTML5_CLOSE_A(readbuffer *obuf, html_feed_environ *h_env) {
  do {
    if (obuf->flag & RB_HTML5) {
      this->close_anchor(h_env);
    }
  } while (0);
}

Str *HtmlParser::getLinkNumberStr(int correction) const {
  return Sprintf("[%d]", cur_hseq + correction);
}

void HtmlParser::push_charp(readbuffer *obuf, int width, const char *str,
                            Lineprop mode) {
  obuf->push_nchars(width, str, strlen(str), mode);
}

void HtmlParser::push_str(readbuffer *obuf, int width, Str *str,
                          Lineprop mode) {
  obuf->push_nchars(width, str->ptr, str->length, mode);
}

void HtmlParser::check_breakpoint(struct readbuffer *obuf, int pre_mode,
                                  const char *ch) {
  int tlen, len = obuf->line->length;

  obuf->append_tags();
  if (pre_mode)
    return;
  tlen = obuf->line->length - len;
  if (tlen > 0 ||
      is_boundary((unsigned char *)obuf->prevchar->ptr, (unsigned char *)ch))
    obuf->set_breakpoint(tlen);
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
  struct readbuffer *obuf = &h_env->obuf;
  int indent = h_env->envs[h_env->envc].indent;

  this->push_spaces(obuf, 1, (limit - width) / 2);
  this->push_str(obuf, width, str, PC_ASCII);
  this->push_spaces(obuf, 1, (limit - width + 1) / 2);
  if (width > 0)
    flushline(h_env, indent, 0, h_env->limit);
}

void HtmlParser::fillline(struct readbuffer *obuf, int indent) {
  this->push_spaces(obuf, 1, indent - obuf->pos);
  obuf->flag &= ~RB_NFLUSHED;
}

void HtmlParser::flushline(struct html_feed_environ *h_env, int indent,
                           int force, int width) {
  auto buf = h_env->buf;
  Str *line = h_env->obuf.line, *pass = nullptr;
  // char *hidden_anchor = nullptr, *hidden_img = nullptr, *hidden_bold =
  // nullptr,
  //      *hidden_under = nullptr, *hidden_italic = nullptr, *hidden_strike =
  //      nullptr, *hidden_ins = nullptr, *hidden_input = nullptr, *hidden =
  //      nullptr;
  char *hidden = nullptr;
  char *hidden_anchor = nullptr;

  if (!(h_env->obuf.flag & (RB_SPECIAL & ~RB_NOBR)) &&
      Strlastchar(line) == ' ') {
    Strshrink(line, 1);
    h_env->obuf.pos--;
  }

  h_env->obuf.append_tags();

  auto obuf = &h_env->obuf;
  if (obuf->anchor.url.size()) {
    hidden = hidden_anchor = obuf->has_hidden_link(HTML_A);
  }

  char *hidden_img = nullptr;
  if (obuf->img_alt) {
    if ((hidden_img = obuf->has_hidden_link(HTML_IMG_ALT))) {
      if (!hidden || hidden_img < hidden)
        hidden = hidden_img;
    }
  }

  char *hidden_input = nullptr;
  if (obuf->input_alt.in) {
    if ((hidden_input = obuf->has_hidden_link(HTML_INPUT_ALT))) {
      if (!hidden || hidden_input < hidden) {
        hidden = hidden_input;
      }
    }
  }

  char *hidden_bold = nullptr;
  if (obuf->fontstat.in_bold) {
    if ((hidden_bold = obuf->has_hidden_link(HTML_B))) {
      if (!hidden || hidden_bold < hidden) {
        hidden = hidden_bold;
      }
    }
  }

  char *hidden_italic = nullptr;
  if (obuf->fontstat.in_italic) {
    if ((hidden_italic = obuf->has_hidden_link(HTML_I))) {
      if (!hidden || hidden_italic < hidden) {
        hidden = hidden_italic;
      }
    }
  }

  char *hidden_under = nullptr;
  if (obuf->fontstat.in_under) {
    if ((hidden_under = obuf->has_hidden_link(HTML_U))) {
      if (!hidden || hidden_under < hidden) {
        hidden = hidden_under;
      }
    }
  }

  char *hidden_strike = nullptr;
  if (obuf->fontstat.in_strike) {
    if ((hidden_strike = obuf->has_hidden_link(HTML_S))) {
      if (!hidden || hidden_strike < hidden) {
        hidden = hidden_strike;
      }
    }
  }

  char *hidden_ins = nullptr;
  if (obuf->fontstat.in_ins) {
    if ((hidden_ins = obuf->has_hidden_link(HTML_INS))) {
      if (!hidden || hidden_ins < hidden) {
        hidden = hidden_ins;
      }
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
      memcpy(tp - 1, tp, ep - tp + 1);
      line->length--;
      obuf->pos--;
    }
  }

  if (obuf->anchor.url.size() && !hidden_anchor)
    Strcat_charp(line, "</a>");
  if (obuf->img_alt && !hidden_img)
    Strcat_charp(line, "</img_alt>");
  if (obuf->input_alt.in && !hidden_input)
    Strcat_charp(line, "</input_alt>");
  if (obuf->fontstat.in_bold && !hidden_bold)
    Strcat_charp(line, "</b>");
  if (obuf->fontstat.in_italic && !hidden_italic)
    Strcat_charp(line, "</i>");
  if (obuf->fontstat.in_under && !hidden_under)
    Strcat_charp(line, "</u>");
  if (obuf->fontstat.in_strike && !hidden_strike)
    Strcat_charp(line, "</s>");
  if (obuf->fontstat.in_ins && !hidden_ins)
    Strcat_charp(line, "</ins>");

  if (obuf->top_margin > 0) {
    int i;

    struct html_feed_environ h(1, width, indent);
    h.obuf.line = Strnew_size(width + 20);
    h.obuf.pos = obuf->pos;
    h.obuf.flag = obuf->flag;
    h.obuf.top_margin = -1;
    h.obuf.bottom_margin = -1;
    Strcat_charp(h.obuf.line, "<pre_int>");
    for (i = 0; i < h.obuf.pos; i++)
      Strcat_char(h.obuf.line, ' ');
    Strcat_charp(h.obuf.line, "</pre_int>");
    for (i = 0; i < obuf->top_margin; i++) {
      flushline(h_env, indent, force, width);
    }
  }

  if (force == 1 || obuf->flag & RB_NFLUSHED) {
    TextLine *lbuf = TextLine::newTextLine(line, obuf->pos);
    if (obuf->RB_GET_ALIGN() == RB_CENTER) {
      lbuf->align(width, ALIGN_CENTER);
    } else if (obuf->RB_GET_ALIGN() == RB_RIGHT) {
      lbuf->align(width, ALIGN_RIGHT);
    } else if (obuf->RB_GET_ALIGN() == RB_LEFT && obuf->flag & RB_INTABLE) {
      lbuf->align(width, ALIGN_LEFT);
    } else if (obuf->flag & RB_FILL) {
      char *p;
      int rest, rrest;
      int nspace, d, i;

      rest = width - get_strwidth(line->ptr);
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
          lbuf = TextLine::newTextLine(line, width);
        }
      }
    }

    if (lbuf->pos() > h_env->maxlimit) {
      h_env->maxlimit = lbuf->pos();
    }

    if (buf) {
      buf->pushValue(lbuf);
    }
    if (obuf->flag & RB_SPECIAL || obuf->flag & RB_NFLUSHED)
      h_env->blank_lines = 0;
    else
      h_env->blank_lines++;
  } else {
    const char *p = line->ptr;
    Str *tmp = Strnew(), *tmp2 = Strnew();

    while (*p) {
      stringtoken st(p);
      auto token = st.sloppy_parse_line();
      p = st.ptr();
      if (token) {
        Strcat(tmp, *token);
        if (force == 2) {
          if (buf) {
            appendTextLine(buf, tmp, 0);
          }
        } else
          Strcat(tmp2, tmp);
        Strclear(tmp);
      }
    }
    if (force == 2) {
      if (pass) {
        if (buf) {
          appendTextLine(buf, pass, 0);
        }
      }
      pass = nullptr;
    } else {
      if (pass)
        Strcat(tmp2, pass);
      pass = tmp2;
    }
  }

  if (obuf->bottom_margin > 0) {
    int i;

    struct html_feed_environ h(1, width, indent);
    h.obuf.line = Strnew_size(width + 20);
    h.obuf.pos = obuf->pos;
    h.obuf.flag = obuf->flag;
    h.obuf.top_margin = -1;
    h.obuf.bottom_margin = -1;
    Strcat_charp(h.obuf.line, "<pre_int>");
    for (i = 0; i < h.obuf.pos; i++)
      Strcat_char(h.obuf.line, ' ');
    Strcat_charp(h.obuf.line, "</pre_int>");
    for (i = 0; i < obuf->bottom_margin; i++) {
      flushline(h_env, indent, force, width);
    }
  }
  if (obuf->top_margin < 0 || obuf->bottom_margin < 0) {
    return;
  }

  obuf->line = Strnew_size(256);
  obuf->pos = 0;
  obuf->top_margin = 0;
  obuf->bottom_margin = 0;
  set_space_to_prevchar(obuf->prevchar);
  obuf->bp.init_flag = 1;
  obuf->flag &= ~RB_NFLUSHED;
  obuf->set_breakpoint(0);
  obuf->prev_ctype = PC_ASCII;
  obuf->link_stack = nullptr;
  fillline(obuf, indent);
  if (pass)
    obuf->passthrough(pass->ptr, 0);
  if (!hidden_anchor && obuf->anchor.url.size()) {
    Str *tmp;
    if (obuf->anchor.hseq > 0)
      obuf->anchor.hseq = -obuf->anchor.hseq;
    tmp = Sprintf("<A HSEQ=\"%d\" HREF=\"", obuf->anchor.hseq);
    Strcat_charp(tmp, html_quote(obuf->anchor.url.c_str()));
    if (obuf->anchor.target.size()) {
      Strcat_charp(tmp, "\" TARGET=\"");
      Strcat_charp(tmp, html_quote(obuf->anchor.target.c_str()));
    }
    if (obuf->anchor.option.use_referer()) {
      Strcat_charp(tmp, "\" REFERER=\"");
      Strcat_charp(tmp, html_quote(obuf->anchor.option.referer.c_str()));
    }
    if (obuf->anchor.title.size()) {
      Strcat_charp(tmp, "\" TITLE=\"");
      Strcat_charp(tmp, html_quote(obuf->anchor.title.c_str()));
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
    obuf->push_tag(tmp->ptr, HTML_A);
  }
  if (!hidden_img && obuf->img_alt) {
    Str *tmp = Strnew_charp("<IMG_ALT SRC=\"");
    Strcat_charp(tmp, html_quote(obuf->img_alt->ptr));
    Strcat_charp(tmp, "\">");
    obuf->push_tag(tmp->ptr, HTML_IMG_ALT);
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
    obuf->push_tag(tmp->ptr, HTML_INPUT_ALT);
  }
  if (!hidden_bold && obuf->fontstat.in_bold)
    obuf->push_tag("<B>", HTML_B);
  if (!hidden_italic && obuf->fontstat.in_italic)
    obuf->push_tag("<I>", HTML_I);
  if (!hidden_under && obuf->fontstat.in_under)
    obuf->push_tag("<U>", HTML_U);
  if (!hidden_strike && obuf->fontstat.in_strike)
    obuf->push_tag("<S>", HTML_S);
  if (!hidden_ins && obuf->fontstat.in_ins)
    obuf->push_tag("<INS>", HTML_INS);
}

void HtmlParser::do_blankline(struct html_feed_environ *h_env,
                              struct readbuffer *obuf, int indent,
                              int indent_incr, int width) {
  if (h_env->blank_lines == 0) {
    flushline(h_env, indent, 1, width);
  }
}

int HtmlParser::close_effect0(struct readbuffer *obuf, HtmlCommand cmd) {
  int i;
  char *p;

  for (i = obuf->tag_sp - 1; i >= 0; i--) {
    if (obuf->tag_stack[i]->cmd == cmd)
      break;
  }
  if (i >= 0) {
    obuf->tag_sp--;
    memcpy(&obuf->tag_stack[i], &obuf->tag_stack[i + 1],
           (obuf->tag_sp - i) * sizeof(struct cmdtable *));
    return 1;
  } else if ((p = obuf->has_hidden_link(cmd)) != nullptr) {
    obuf->passthrough(p, 1);
    return 1;
  }
  return 0;
}

void HtmlParser::close_anchor(struct html_feed_environ *h_env) {
  auto obuf = &h_env->obuf;
  if (obuf->anchor.url.size()) {
    int i;
    char *p = nullptr;
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

    if (i >= 0 || (p = obuf->has_hidden_link(HTML_A))) {
      if (obuf->anchor.hseq > 0) {
        this->HTMLlineproc1(ANSP, h_env);
        set_space_to_prevchar(obuf->prevchar);
      } else {
        if (i >= 0) {
          obuf->tag_sp--;
          memcpy(&obuf->tag_stack[i], &obuf->tag_stack[i + 1],
                 (obuf->tag_sp - i) * sizeof(struct cmdtable *));
        } else {
          obuf->passthrough(p, 1);
        }
        obuf->anchor = {};
        return;
      }
      is_erased = 0;
    }
    if (is_erased) {
      Strcat_char(obuf->line, ' ');
      obuf->pos++;
    }

    obuf->push_tag("</a>", HTML_N_A);
  }
  obuf->anchor = {};
}

void HtmlParser::save_fonteffect(struct html_feed_environ *h_env) {
  auto obuf = &h_env->obuf;
  if (obuf->fontstat_sp < FONT_STACK_SIZE) {
    obuf->fontstat_stack[obuf->fontstat_sp] = obuf->fontstat;
  }
  if (obuf->fontstat_sp < INT_MAX) {
    obuf->fontstat_sp++;
  }

  if (obuf->fontstat.in_bold)
    obuf->push_tag("</b>", HTML_N_B);
  if (obuf->fontstat.in_italic)
    obuf->push_tag("</i>", HTML_N_I);
  if (obuf->fontstat.in_under)
    obuf->push_tag("</u>", HTML_N_U);
  if (obuf->fontstat.in_strike)
    obuf->push_tag("</s>", HTML_N_S);
  if (obuf->fontstat.in_ins)
    obuf->push_tag("</ins>", HTML_N_INS);
  obuf->fontstat = {};
}

void HtmlParser::restore_fonteffect(struct html_feed_environ *h_env) {
  auto obuf = &h_env->obuf;
  if (obuf->fontstat_sp > 0)
    obuf->fontstat_sp--;
  if (obuf->fontstat_sp < FONT_STACK_SIZE) {
    obuf->fontstat = obuf->fontstat_stack[obuf->fontstat_sp];
  }

  if (obuf->fontstat.in_bold)
    obuf->push_tag("<b>", HTML_B);
  if (obuf->fontstat.in_italic)
    obuf->push_tag("<i>", HTML_I);
  if (obuf->fontstat.in_under)
    obuf->push_tag("<u>", HTML_U);
  if (obuf->fontstat.in_strike)
    obuf->push_tag("<s>", HTML_S);
  if (obuf->fontstat.in_ins)
    obuf->push_tag("<ins>", HTML_INS);
}

void HtmlParser::process_title(const std::shared_ptr<HtmlTag> &tag) {
  if (pre_title) {
    return;
  }
  cur_title = Strnew();
}

Str *HtmlParser::process_n_title(const std::shared_ptr<HtmlTag> &tag) {
  Str *tmp;

  if (pre_title)
    return nullptr;
  if (!cur_title)
    return nullptr;
  Strremovefirstspaces(cur_title);
  Strremovetrailingspaces(cur_title);
  tmp = Strnew_m_charp("<title_alt title=\"", html_quote(cur_title->ptr), "\">",
                       nullptr);
  pre_title = cur_title;
  cur_title = nullptr;
  return tmp;
}

void HtmlParser::feed_title(const char *str) {
  if (pre_title)
    return;
  if (!cur_title)
    return;
  while (*str) {
    if (*str == '&')
      Strcat(cur_title, getescapecmd(&str));
    else if (*str == '\n' || *str == '\r') {
      Strcat_char(cur_title, ' ');
      str++;
    } else
      Strcat_char(cur_title, *(str++));
  }
}

Str *HtmlParser::process_textarea(const std::shared_ptr<HtmlTag> &tag,
                                  int width) {
  Str *tmp = nullptr;

  if (cur_form_id() < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp = process_form(HtmlTag::parse(&s, true));
  }

  auto p = "";
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
  // if (n_textarea >= max_textarea) {
  //   max_textarea *= 2;
  // textarea_str = (Str **)New_Reuse(Str *, textarea_str, max_textarea);
  // }
  ignore_nl_textarea = true;

  return tmp;
}

Str *HtmlParser::process_n_textarea() {
  if (cur_textarea == nullptr) {
    return nullptr;
  }

  auto tmp = Strnew();
  Strcat(tmp, Sprintf("<pre_int>[<input_alt hseq=\"%d\" fid=\"%d\" "
                      "type=textarea name=\"%s\" size=%d rows=%d "
                      "top_margin=%d textareanumber=%d",
                      this->cur_hseq, cur_form_id(),
                      html_quote(cur_textarea->ptr), cur_textarea_size,
                      cur_textarea_rows, cur_textarea_rows - 1, n_textarea));
  if (cur_textarea_readonly)
    Strcat_charp(tmp, " readonly");
  Strcat_charp(tmp, "><u>");
  for (int i = 0; i < cur_textarea_size; i++) {
    Strcat_char(tmp, ' ');
  }
  Strcat_charp(tmp, "</u></input_alt>]</pre_int>\n");
  this->cur_hseq++;
  textarea_str.push_back({});
  a_textarea.push_back({});
  n_textarea++;
  cur_textarea = nullptr;

  return tmp;
}

void HtmlParser::feed_textarea(const char *str) {
  if (cur_textarea == nullptr)
    return;
  if (ignore_nl_textarea) {
    if (*str == '\r') {
      str++;
    }
    if (*str == '\n') {
      str++;
    }
  }
  ignore_nl_textarea = false;
  while (*str) {
    if (*str == '&') {
      textarea_str[n_textarea] += getescapecmd(&str);
    } else if (*str == '\n') {
      textarea_str[n_textarea] += "\r\n";
      str++;
    } else if (*str == '\r') {
      str++;
    } else {
      textarea_str[n_textarea] += *(str++);
    }
  }
}

Str *HtmlParser::process_form_int(const std::shared_ptr<HtmlTag> &tag,
                                  int fid) {
  auto p = "get";
  tag->parsedtag_get_value(ATTR_METHOD, &p);
  auto q = "!CURRENT_URL!";
  tag->parsedtag_get_value(ATTR_ACTION, &q);
  q = Strnew(url_quote(remove_space(q)))->ptr;
  auto s = "";
  tag->parsedtag_get_value(ATTR_ENCTYPE, &s);
  auto tg = "";
  tag->parsedtag_get_value(ATTR_TARGET, &tg);
  auto n = "";
  tag->parsedtag_get_value(ATTR_NAME, &n);

  if (fid < 0) {
    forms.push_back({});
    form_stack.push_back({});
    form_sp++;
    fid = forms.size() - 1;
  } else { /* <form_int> */
    while (fid >= (int)forms.size()) {
      forms.push_back({});
      form_stack.push_back({});
    }
    form_sp = fid;
  }
  form_stack[form_sp] = fid;

  forms[fid] = std::make_shared<Form>(q, p, s, tg, n);
  return nullptr;
}

Str *HtmlParser::process_n_form(void) {
  if (form_sp >= 0)
    form_sp--;
  return nullptr;
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

static Str *textfieldrep(Str *s, int width) {
  Lineprop c_type;
  Str *n = Strnew_size(width + 2);
  int i, j, k, c_len;

  j = 0;
  for (i = 0; i < s->length; i += c_len) {
    c_type = get_mctype(&s->ptr[i]);
    c_len = get_mclen(&s->ptr[i]);
    if (s->ptr[i] == '\r')
      continue;
    k = j + get_mcwidth(&s->ptr[i]);
    if (k > width)
      break;
    if (c_type == PC_CTRL)
      Strcat_char(n, ' ');
    else if (s->ptr[i] == '&')
      Strcat_charp(n, "&amp;");
    else if (s->ptr[i] == '<')
      Strcat_charp(n, "&lt;");
    else if (s->ptr[i] == '>')
      Strcat_charp(n, "&gt;");
    else
      Strcat_charp_n(n, &s->ptr[i], c_len);
    j = k;
  }
  for (; j < width; j++)
    Strcat_char(n, ' ');
  return n;
}

std::shared_ptr<LineLayout>
HtmlParser::render(const std::shared_ptr<LineLayout> &layout,
                   const Url &currentUrl, html_feed_environ *h_env) {

  auto old = *layout->data._formitem;
  layout->data.clear();
  layout->data.baseURL = currentUrl;
  layout->data.title = h_env->title;

  LineFeed feed(h_env->buf);
  for (int nlines = 0; auto str = feed.textlist_feed(); ++nlines) {
    if (n_textarea >= 0 && *str != '<') {
      textarea_str[n_textarea] += str;
      continue;
    }

    auto line = renderLine(currentUrl, h_env, &layout->data, nlines, str, &old);

    /* end of processing for one line */
    if (!internal) {
      line.PPUSH(0, 0);
      layout->data.lines.push_back(line);
    }
    if (internal == HTML_N_INTERNAL) {
      internal = {};
    }
  }

  layout->data.formlist = forms;
  if (n_textarea) {
    layout->data.addMultirowsForm();
  }

  layout->fix_size(h_env->limit);
  layout->formResetBuffer(layout->data._formitem.get());

  return layout;
}

Line HtmlParser::renderLine(const Url &url, html_feed_environ *h_env,
                            LineData *data, int nlines, const char *str,
                            AnchorList<FormAnchor> *old) {

  Line line;

  auto endp = str + strlen(str);
  while (str < endp) {
    auto mode = get_mctype(str);
    if ((effect | ex_efct(ex_effect)) & PC_SYMBOL && *str != '<') {
      line.PPUSH(PC_ASCII | effect | ex_efct(ex_effect), SYMBOL_BASE + symbol);
      str += symbol_width;
    } else if (mode == PC_CTRL || IS_INTSPACE(*str)) {
      line.PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
      str++;
    } else if (*str != '<' && *str != '&') {
      str += line.push_mc(effect | ex_efct(ex_effect), str);
    } else if (*str == '&') {
      /*
       * & escape processing
       */
      auto _p = getescapecmd(&str);
      auto p = _p.c_str();
      while (*p) {
        mode = get_mctype(p);
        if (mode == PC_CTRL || IS_INTSPACE(*str)) {
          line.PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
          p++;
        } else {
          p += line.push_mc(mode | effect | ex_efct(ex_effect), p);
        }
      }
    } else {
      /* tag processing */
      std::shared_ptr<HtmlTag> tag;
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
      case HTML_A: {
        const char *p = nullptr;
        const char *r = nullptr;
        const char *s = nullptr;
        // auto q = res->baseTarget;
        auto t = "";
        auto hseq = 0;
        char *id = nullptr;
        if (tag->parsedtag_get_value(ATTR_NAME, &id)) {
          auto _id = url_quote(id);
          data->registerName(_id.c_str(), nlines, line.len());
        }
        if (tag->parsedtag_get_value(ATTR_HREF, &p)) {
          p = Strnew(url_quote(remove_space(p)))->ptr;
        }
        const char *q = nullptr;
        if (tag->parsedtag_get_value(ATTR_TARGET, &q)) {
          q = Strnew(url_quote(q))->ptr;
        }
        if (tag->parsedtag_get_value(ATTR_REFERER, &r))
          r = Strnew(url_quote(r))->ptr;
        tag->parsedtag_get_value(ATTR_TITLE, &s);
        tag->parsedtag_get_value(ATTR_ACCESSKEY, &t);
        tag->parsedtag_get_value(ATTR_HSEQ, &hseq);
        if (hseq > 0) {
          data->_hmarklist->putHmarker(nlines, line.len(), hseq - 1);
        } else if (hseq < 0) {
          int h = -hseq - 1;
          if (data->_hmarklist->tryMark(h, nlines, line.len())) {
            hseq = -hseq;
          }
        }
        if (p) {
          effect |= PE_ANCHOR;
          auto bp = BufferPoint{
              .line = nlines,
              .pos = line.len(),
          };
          a_href = data->_href->putAnchor(Anchor{
              .url = p,
              .target = q ? q : "",
              .option = {.referer = r ? r : ""},
              .title = s ? s : "",
              .accesskey = (unsigned char)*t,
              .start = bp,
              .end = bp,
              .hseq = ((hseq > 0) ? hseq : -hseq) - 1,
              .slave = (hseq > 0) ? false : true,
          });
        }
        break;
      }

      case HTML_N_A:
        effect &= ~PE_ANCHOR;
        if (a_href) {
          a_href->end.line = nlines;
          a_href->end.pos = line.len();
          if (a_href->start.line == a_href->end.line &&
              a_href->start.pos == a_href->end.pos) {
            if (a_href->hseq >= 0 &&
                a_href->hseq < (int)data->_hmarklist->size()) {
              data->_hmarklist->invalidate(a_href->hseq);
            }
            a_href->hseq = -1;
          }
          a_href = nullptr;
        }
        break;

      case HTML_LINK:
        data->addLink(tag);
        break;

      case HTML_IMG_ALT: {
        const char *p;
        if (tag->parsedtag_get_value(ATTR_SRC, &p)) {
          const char *s = nullptr;
          tag->parsedtag_get_value(ATTR_TITLE, &s);
          auto pp = url_quote(remove_space(p));
          a_img = data->registerImg(pp.c_str(), s, nlines, line.len());
        }
        effect |= PE_IMAGE;
        break;
      }

      case HTML_N_IMG_ALT:
        effect &= ~PE_IMAGE;
        if (a_img) {
          a_img->end.line = nlines;
          a_img->end.pos = line.len();
        }
        a_img = nullptr;
        break;
      case HTML_INPUT_ALT: {
        int top = 0, bottom = 0;
        int textareanumber = -1;
        auto hseq = 0;
        auto form_id = -1;

        tag->parsedtag_get_value(ATTR_HSEQ, &hseq);
        tag->parsedtag_get_value(ATTR_FID, &form_id);
        tag->parsedtag_get_value(ATTR_TOP_MARGIN, &top);
        tag->parsedtag_get_value(ATTR_BOTTOM_MARGIN, &bottom);
        if (form_id < 0 || form_id >= (int)forms.size() ||
            forms[form_id] == nullptr) {
          break; /* outside of <form>..</form> */
        }
        auto form = forms[form_id];
        if (hseq > 0) {
          int hpos = line.len();
          if (*str == '[')
            hpos++;
          data->_hmarklist->putHmarker(nlines, hpos, hseq - 1);
        } else if (hseq < 0) {
          int h = -hseq - 1;
          int hpos = line.len();
          if (*str == '[')
            hpos++;
          if (data->_hmarklist->tryMark(h, nlines, hpos)) {
            hseq = -hseq;
          }
        }

        if (form->target.empty()) {
          form->target = data->baseTarget;
        }
        if (a_textarea.size() &&
            tag->parsedtag_get_value(ATTR_TEXTAREANUMBER, &textareanumber)) {
        }

        BufferPoint bp{
            .line = nlines,
            .pos = line.len(),
        };
        a_form = data->registerForm(h_env, form, tag, bp);
        if (textareanumber >= 0) {
          while (textareanumber >= (int)a_textarea.size()) {
            textarea_str.push_back({});
            a_textarea.push_back({});
          }
          a_textarea[textareanumber] = a_form;
        }

        if (a_form) {

          {
            auto aa = old->retrieveAnchor(a_form->start);
            if (aa) {
              if (auto fi = aa->formItem) {
                a_form->formItem->value = fi->value;
                ;
              }
            }
          }

          a_form->hseq = hseq - 1;
          a_form->y = nlines - top;
          a_form->rows = 1 + top + bottom;
          if (!tag->parsedtag_exists(ATTR_NO_EFFECT))
            effect |= PE_FORM;
          break;
        }
      }
      case HTML_N_INPUT_ALT:
        effect &= ~PE_FORM;
        if (a_form) {
          a_form->end.line = nlines;
          a_form->end.pos = line.len();
          if (a_form->start.line == a_form->end.line &&
              a_form->start.pos == a_form->end.pos)
            a_form->hseq = -1;
        }
        a_form = nullptr;
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
        // if (buf->maplist == nullptr) /* outside of <map>..</map> */
        //   break;
        // if (tag->parsedtag_get_value( ATTR_HREF, &p)) {
        //   MapArea *a;
        //   p = url_encode(remove_space(p), base, buf->document_charset);
        //   t = nullptr;
        //   tag->parsedtag_get_value( ATTR_TARGET, &t);
        //   q = "";
        //   tag->parsedtag_get_value( ATTR_ALT, &q);
        //   r = nullptr;
        //   s = nullptr;
        //   a = newMapArea(p, t, q, r, s);
        //   pushValue(buf->maplist->area, (void *)a);
        // }
        break;
      case HTML_FRAMESET:
        // frameset_sp++;
        // if (frameset_sp >= FRAMESTACK_SIZE)
        //   break;
        // frameset_s[frameset_sp] = newFrameSet(tag);
        // if (frameset_s[frameset_sp] == nullptr)
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
      case HTML_BASE: {
        const char *p;
        if (tag->parsedtag_get_value(ATTR_HREF, &p)) {
          p = Strnew(url_quote(remove_space(p)))->ptr;
          data->baseURL = {p, url};
        }
        if (tag->parsedtag_get_value(ATTR_TARGET, &p))
          data->baseTarget = url_quote(p);
        break;
      }

      case HTML_META: {
        const char *p = nullptr;
        const char *q = nullptr;
        tag->parsedtag_get_value(ATTR_HTTP_EQUIV, &p);
        tag->parsedtag_get_value(ATTR_CONTENT, &q);
        if (p && q && !strcasecmp(p, "refresh") && MetaRefresh) {
          Str *tmp = nullptr;
          int refresh_interval = getMetaRefreshParam(q, &tmp);
          if (tmp) {
            p = Strnew(url_quote(remove_space(tmp->ptr)))->ptr;
            // TODO:
            // App::instance().task(refresh_interval, FUNCNAME_gorURL, p);
          } else if (refresh_interval > 0) {
            data->refresh_interval = refresh_interval;
          }
        }
        break;
      }

      case HTML_INTERNAL:
        internal = HTML_INTERNAL;
        break;
      case HTML_N_INTERNAL:
        internal = HTML_N_INTERNAL;
        break;
      case HTML_FORM_INT: {
        int form_id;
        if (tag->parsedtag_get_value(ATTR_FID, &form_id))
          process_form_int(tag, form_id);
        break;
      }
      case HTML_TEXTAREA_INT:
        if (tag->parsedtag_get_value(ATTR_TEXTAREANUMBER, &n_textarea) &&
            n_textarea >= 0) {
        } else {
          n_textarea = -1;
        }
        break;
      case HTML_N_TEXTAREA_INT:
        if (n_textarea >= 0) {
          auto item = a_textarea[n_textarea]->formItem;
          item->init_value = item->value = textarea_str[n_textarea];
        }
        break;

      case HTML_TITLE_ALT: {
        const char *p;
        if (tag->parsedtag_get_value(ATTR_TITLE, &p))
          data->title = html_unquote(p);
        break;
      }

      case HTML_SYMBOL: {
        // effect |= PC_SYMBOL;
        // const char *p;
        // if (tag->parsedtag_get_value(ATTR_TYPE, &p))
        //   symbol = (char)atoi(p);
        break;
      }

      case HTML_N_SYMBOL:
        // effect &= ~PC_SYMBOL;
        break;

      default:
        break;
      }
    }
  }

  return line;
}

int getMetaRefreshParam(const char *q, Str **refresh_uri) {
  int refresh_interval;
  const char *r;
  Str *s_tmp = nullptr;

  if (q == nullptr || refresh_uri == nullptr)
    return 0;

  refresh_interval = atoi(q);
  if (refresh_interval < 0)
    return 0;

  while (*q) {
    if (!strncasecmp(q, "url=", 4)) {
      q += 4;
      if (*q == '\"' || *q == '\'') /* " or ' */
        q++;
      r = q;
      while (*r && !IS_SPACE(*r) && *r != ';')
        r++;
      s_tmp = Strnew_charp_n(q, r - q);

      if (s_tmp->length > 0 &&
          (s_tmp->ptr[s_tmp->length - 1] == '\"' ||  /* " */
           s_tmp->ptr[s_tmp->length - 1] == '\'')) { /* ' */
        s_tmp->length--;
        s_tmp->ptr[s_tmp->length] = '\0';
      }
      q = r;
    }
    while (*q && *q != ';')
      q++;
    if (*q == ';')
      q++;
    while (*q && *q == ' ')
      q++;
  }
  *refresh_uri = s_tmp;
  return refresh_interval;
}

Str *HtmlParser::process_img(const std::shared_ptr<HtmlTag> &tag, int width) {
  const char *p, *q, *r, *r2 = nullptr, *s, *t;
  int w, i, nw, n;
  int pre_int = false, ext_pre_int = false;
  Str *tmp = Strnew();

  if (!tag->parsedtag_get_value(ATTR_SRC, &p))
    return tmp;
  p = Strnew(url_quote(remove_space(p)))->ptr;
  q = nullptr;
  tag->parsedtag_get_value(ATTR_ALT, &q);
  if (!pseudoInlines && (q == nullptr || (*q == '\0' && ignore_null_img_alt)))
    return tmp;
  t = q;
  tag->parsedtag_get_value(ATTR_TITLE, &t);
  w = -1;
  if (tag->parsedtag_get_value(ATTR_WIDTH, &w)) {
    if (w < 0) {
      if (width > 0)
        w = (int)(-width * pixel_per_char * w / 100 + 0.5);
      else
        w = -1;
    }
  }
  i = -1;
  tag->parsedtag_get_value(ATTR_HEIGHT, &i);
  r = nullptr;
  tag->parsedtag_get_value(ATTR_USEMAP, &r);
  if (tag->parsedtag_exists(ATTR_PRE_INT))
    ext_pre_int = true;

  tmp = Strnew_size(128);
  if (r) {
    Str *tmp2;
    r2 = strchr(r, '#');
    s = "<form_int method=internal action=map>";
    tmp2 = this->process_form(HtmlTag::parse(&s, true));
    if (tmp2)
      Strcat(tmp, tmp2);
    Strcat(tmp, Sprintf("<input_alt fid=\"%d\" "
                        "type=hidden name=link value=\"",
                        this->cur_form_id()));
    Strcat_charp(tmp, html_quote((r2) ? r2 + 1 : r));
    Strcat(tmp, Sprintf("\"><input_alt hseq=\"%d\" fid=\"%d\" "
                        "type=submit no_effect=true>",
                        this->cur_hseq++, this->cur_form_id()));
  }
  {
    if (w < 0) {
      w = static_cast<int>(12 * pixel_per_char);
    }
    nw = w ? (int)((w - 1) / pixel_per_char + 1) : 1;
    if (r) {
      Strcat_charp(tmp, "<pre_int>");
      pre_int = true;
    }
    Strcat_charp(tmp, "<img_alt src=\"");
  }
  Strcat_charp(tmp, html_quote(p));
  Strcat_charp(tmp, "\"");
  if (t) {
    Strcat_charp(tmp, " title=\"");
    Strcat_charp(tmp, html_quote(t));
    Strcat_charp(tmp, "\"");
  }
  Strcat_charp(tmp, ">");
  if (q != nullptr && *q == '\0' && ignore_null_img_alt)
    q = nullptr;
  if (q != nullptr) {
    n = get_strwidth(q);
    Strcat_charp(tmp, html_quote(q));
    goto img_end;
  }
  if (w > 0 && i > 0) {
    /* guess what the image is! */
    if (w < 32 && i < 48) {
      /* must be an icon or space */
      n = 1;
      if (strcasestr(p, "space") || strcasestr(p, "blank"))
        Strcat_charp(tmp, "_");
      else {
        if (w * i < 8 * 16)
          Strcat_charp(tmp, "*");
        else {
          if (!pre_int) {
            Strcat_charp(tmp, "<pre_int>");
            pre_int = true;
          }
          push_symbol(tmp, IMG_SYMBOL, symbol_width, 1);
          n = symbol_width;
        }
      }
      goto img_end;
    }
    if (w > 200 && i < 13) {
      /* must be a horizontal line */
      if (!pre_int) {
        Strcat_charp(tmp, "<pre_int>");
        pre_int = true;
      }
      w = static_cast<int>(w / pixel_per_char / symbol_width);
      if (w <= 0)
        w = 1;
      push_symbol(tmp, HR_SYMBOL, symbol_width, w);
      n = w * symbol_width;
      goto img_end;
    }
  }
  for (q = p; *q; q++)
    ;
  while (q > p && *q != '/')
    q--;
  if (*q == '/')
    q++;
  Strcat_char(tmp, '[');
  n = 1;
  p = q;
  for (; *q; q++) {
    if (!IS_ALNUM(*q) && *q != '_' && *q != '-') {
      break;
    }
    Strcat_char(tmp, *q);
    n++;
    if (n + 1 >= nw)
      break;
  }
  Strcat_char(tmp, ']');
  n++;
img_end:
  Strcat_charp(tmp, "</img_alt>");
  if (pre_int && !ext_pre_int)
    Strcat_charp(tmp, "</pre_int>");
  if (r) {
    Strcat_charp(tmp, "</input_alt>");
    this->process_n_form();
  }
  return tmp;
}

Str *HtmlParser::process_anchor(const std::shared_ptr<HtmlTag> &tag,
                                const char *tagbuf) {
  if (tag->parsedtag_need_reconstruct()) {
    tag->parsedtag_set_value(ATTR_HSEQ, Sprintf("%d", this->cur_hseq++)->ptr);
    return tag->parsedtag2str();
  } else {
    Str *tmp = Sprintf("<a hseq=\"%d\"", this->cur_hseq++);
    Strcat_charp(tmp, tagbuf + 2);
    return tmp;
  }
}

Str *HtmlParser::process_input(const std::shared_ptr<HtmlTag> &tag) {
  int i = 20, v, x, y, z, iw, ih, size = 20;
  const char *q, *p, *r, *p2, *s;
  Str *tmp = nullptr;
  auto qq = "";
  int qlen = 0;

  if (this->cur_form_id() < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp = this->process_form(HtmlTag::parse(&s, true));
  }
  if (tmp == nullptr)
    tmp = Strnew();

  p = "text";
  tag->parsedtag_get_value(ATTR_TYPE, &p);
  q = nullptr;
  tag->parsedtag_get_value(ATTR_VALUE, &q);
  r = "";
  tag->parsedtag_get_value(ATTR_NAME, &r);
  tag->parsedtag_get_value(ATTR_SIZE, &size);
  if (size > MAX_INPUT_SIZE)
    size = MAX_INPUT_SIZE;
  tag->parsedtag_get_value(ATTR_MAXLENGTH, &i);
  p2 = nullptr;
  tag->parsedtag_get_value(ATTR_ALT, &p2);
  x = tag->parsedtag_exists(ATTR_CHECKED);
  y = tag->parsedtag_exists(ATTR_ACCEPT);
  z = tag->parsedtag_exists(ATTR_READONLY);

  v = formtype(p);
  if (v == FORM_UNKNOWN)
    return nullptr;

  if (!q) {
    switch (v) {
    case FORM_INPUT_IMAGE:
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
      q = "SUBMIT";
      break;
    case FORM_INPUT_RESET:
      q = "RESET";
      break;
      /* if no VALUE attribute is specified in
       * <INPUT TYPE=CHECKBOX> tag, then the value "on" is used
       * as a default value. It is not a part of HTML4.0
       * specification, but an imitation of Netscape behaviour.
       */
    case FORM_INPUT_CHECKBOX:
      q = "on";
    }
  }
  /* VALUE attribute is not allowed in <INPUT TYPE=FILE> tag. */
  if (v == FORM_INPUT_FILE)
    q = nullptr;
  if (q) {
    qq = html_quote(q);
    qlen = get_strwidth(q);
  }

  Strcat_charp(tmp, "<pre_int>");
  switch (v) {
  case FORM_INPUT_PASSWORD:
  case FORM_INPUT_TEXT:
  case FORM_INPUT_FILE:
  case FORM_INPUT_CHECKBOX:
    if (displayLinkNumber)
      Strcat(tmp, this->getLinkNumberStr(0));
    Strcat_char(tmp, '[');
    break;
  case FORM_INPUT_RADIO:
    if (displayLinkNumber)
      Strcat(tmp, this->getLinkNumberStr(0));
    Strcat_char(tmp, '(');
  }
  Strcat(tmp, Sprintf("<input_alt hseq=\"%d\" fid=\"%d\" type=\"%s\" "
                      "name=\"%s\" width=%d maxlength=%d value=\"%s\"",
                      this->cur_hseq++, this->cur_form_id(), html_quote(p),
                      html_quote(r), size, i, qq));
  if (x)
    Strcat_charp(tmp, " checked");
  if (y)
    Strcat_charp(tmp, " accept");
  if (z)
    Strcat_charp(tmp, " readonly");
  Strcat_char(tmp, '>');

  if (v == FORM_INPUT_HIDDEN)
    Strcat_charp(tmp, "</input_alt></pre_int>");
  else {
    switch (v) {
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
      Strcat_charp(tmp, "<u>");
      break;
    case FORM_INPUT_IMAGE:
      s = nullptr;
      tag->parsedtag_get_value(ATTR_SRC, &s);
      if (s) {
        Strcat(tmp, Sprintf("<img src=\"%s\"", html_quote(s)));
        if (p2)
          Strcat(tmp, Sprintf(" alt=\"%s\"", html_quote(p2)));
        if (tag->parsedtag_get_value(ATTR_WIDTH, &iw))
          Strcat(tmp, Sprintf(" width=\"%d\"", iw));
        if (tag->parsedtag_get_value(ATTR_HEIGHT, &ih))
          Strcat(tmp, Sprintf(" height=\"%d\"", ih));
        Strcat_charp(tmp, " pre_int>");
        Strcat_charp(tmp, "</input_alt></pre_int>");
        return tmp;
      }
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
    case FORM_INPUT_RESET:
      if (displayLinkNumber)
        Strcat(tmp, this->getLinkNumberStr(-1));
      Strcat_charp(tmp, "[");
      break;
    }
    switch (v) {
    case FORM_INPUT_PASSWORD:
      i = 0;
      if (q) {
        for (; i < qlen && i < size; i++)
          Strcat_char(tmp, '*');
      }
      for (; i < size; i++)
        Strcat_char(tmp, ' ');
      break;
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
      if (q)
        Strcat(tmp, textfieldrep(Strnew_charp(q), size));
      else {
        for (i = 0; i < size; i++)
          Strcat_char(tmp, ' ');
      }
      break;
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
      if (p2)
        Strcat_charp(tmp, html_quote(p2));
      else
        Strcat_charp(tmp, qq);
      break;
    case FORM_INPUT_RESET:
      Strcat_charp(tmp, qq);
      break;
    case FORM_INPUT_RADIO:
    case FORM_INPUT_CHECKBOX:
      if (x)
        Strcat_char(tmp, '*');
      else
        Strcat_char(tmp, ' ');
      break;
    }
    switch (v) {
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
      Strcat_charp(tmp, "</u>");
      break;
    case FORM_INPUT_IMAGE:
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
    case FORM_INPUT_RESET:
      Strcat_charp(tmp, "]");
    }
    Strcat_charp(tmp, "</input_alt>");
    switch (v) {
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
    case FORM_INPUT_CHECKBOX:
      Strcat_char(tmp, ']');
      break;
    case FORM_INPUT_RADIO:
      Strcat_char(tmp, ')');
    }
    Strcat_charp(tmp, "</pre_int>");
  }
  return tmp;
}

Str *HtmlParser::process_button(const std::shared_ptr<HtmlTag> &tag) {
  Str *tmp = nullptr;
  const char *p, *q, *r, *qq = "";
  int v;

  if (this->cur_form_id() < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp = this->process_form(HtmlTag::parse(&s, true));
  }
  if (tmp == nullptr)
    tmp = Strnew();

  p = "submit";
  tag->parsedtag_get_value(ATTR_TYPE, &p);
  q = nullptr;
  tag->parsedtag_get_value(ATTR_VALUE, &q);
  r = "";
  tag->parsedtag_get_value(ATTR_NAME, &r);

  v = formtype(p);
  if (v == FORM_UNKNOWN)
    return nullptr;

  switch (v) {
  case FORM_INPUT_SUBMIT:
  case FORM_INPUT_BUTTON:
  case FORM_INPUT_RESET:
    break;
  default:
    p = "submit";
    v = FORM_INPUT_SUBMIT;
    break;
  }

  if (!q) {
    switch (v) {
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
      q = "SUBMIT";
      break;
    case FORM_INPUT_RESET:
      q = "RESET";
      break;
    }
  }
  if (q) {
    qq = html_quote(q);
  }

  /*    Strcat_charp(tmp, "<pre_int>"); */
  Strcat(tmp, Sprintf("<input_alt hseq=\"%d\" fid=\"%d\" type=\"%s\" "
                      "name=\"%s\" value=\"%s\">",
                      this->cur_hseq++, this->cur_form_id(), html_quote(p),
                      html_quote(r), qq));
  return tmp;
}

Str *HtmlParser::process_hr(const std::shared_ptr<HtmlTag> &tag, int width,
                            int indent_width) {
  Str *tmp = Strnew_charp("<nobr>");
  int w = 0;
  int x = ALIGN_CENTER;

  if (width > indent_width)
    width -= indent_width;
  if (tag->parsedtag_get_value(ATTR_WIDTH, &w)) {
    if (w > HR_ATTR_WIDTH_MAX) {
      w = HR_ATTR_WIDTH_MAX;
    }
    w = REAL_WIDTH(w, width);
  } else {
    w = width;
  }

  tag->parsedtag_get_value(ATTR_ALIGN, &x);
  switch (x) {
  case ALIGN_CENTER:
    Strcat_charp(tmp, "<div_int align=center>");
    break;
  case ALIGN_RIGHT:
    Strcat_charp(tmp, "<div_int align=right>");
    break;
  case ALIGN_LEFT:
    Strcat_charp(tmp, "<div_int align=left>");
    break;
  }
  w /= symbol_width;
  if (w <= 0)
    w = 1;
  push_symbol(tmp, HR_SYMBOL, symbol_width, w);
  Strcat_charp(tmp, "</div_int></nobr>");
  return tmp;
}

int HtmlParser::pushHtmlTag(const std::shared_ptr<HtmlTag> &tag,
                            struct html_feed_environ *h_env) {

  struct environment *envs = h_env->envs.data();

  HtmlCommand cmd = tag->tagid;

  if (h_env->obuf.flag & RB_PRE) {
    switch (cmd) {
    case HTML_NOBR:
    case HTML_N_NOBR:
    case HTML_PRE_INT:
    case HTML_N_PRE_INT:
      return 1;

    default:
      break;
    }
  }

  switch (cmd) {
  case HTML_B:
    return h_env->HTML_B_enter();
  case HTML_N_B:
    return h_env->HTML_B_exit();
  case HTML_I:
    return h_env->HTML_I_enter();
  case HTML_N_I:
    return h_env->HTML_I_exit();
  case HTML_U:
    return h_env->HTML_U_enter();
  case HTML_N_U:
    return h_env->HTML_U_exit();
  case HTML_EM:
    HTMLlineproc1("<i>", h_env);
    return 1;
  case HTML_N_EM:
    HTMLlineproc1("</i>", h_env);
    return 1;
  case HTML_STRONG:
    HTMLlineproc1("<b>", h_env);
    return 1;
  case HTML_N_STRONG:
    HTMLlineproc1("</b>", h_env);
    return 1;
  case HTML_Q:
    HTMLlineproc1("`", h_env);
    return 1;
  case HTML_N_Q:
    HTMLlineproc1("'", h_env);
    return 1;
  case HTML_FIGURE:
  case HTML_N_FIGURE:
  case HTML_P:
  case HTML_N_P:
    return h_env->HTML_Paragraph(tag);
  case HTML_FIGCAPTION:
  case HTML_N_FIGCAPTION:
  case HTML_BR:
    flushline(h_env, envs[h_env->envc].indent, 1, h_env->limit);
    h_env->blank_lines = 0;
    return 1;
  case HTML_H:
    return h_env->HTML_H_enter(tag);
  case HTML_N_H:
    return h_env->HTML_H_exit();
  case HTML_UL:
  case HTML_OL:
  case HTML_BLQ:
    return h_env->HTML_List_enter(tag);
  case HTML_N_UL:
  case HTML_N_OL:
  case HTML_N_DL:
  case HTML_N_BLQ:
  case HTML_N_DD:
    return h_env->HTML_List_exit(tag);
  case HTML_DL:
    return h_env->HTML_DL_enter(tag);
  case HTML_LI:
    return h_env->HTML_LI_enter(tag);
  case HTML_DT:
    return h_env->HTML_DT_enter();
  case HTML_N_DT:
    return h_env->HTML_DT_exit();
  case HTML_DD:
    return h_env->HTML_DD_enter();
  case HTML_TITLE:
    return h_env->HTML_TITLE_enter(tag);
  case HTML_N_TITLE:
    return h_env->HTML_TITLE_exit(tag);
  case HTML_TITLE_ALT:
    return h_env->HTML_TITLE_ALT(tag);
  case HTML_FRAMESET:
    return h_env->HTML_FRAMESET_enter(tag);
  case HTML_N_FRAMESET:
    return h_env->HTML_FRAMESET_exit();
  case HTML_NOFRAMES:
    return h_env->HTML_NOFRAMES_enter();
  case HTML_N_NOFRAMES:
    return h_env->HTML_NOFRAMES_exit();
  case HTML_FRAME:
    return h_env->HTML_FRAME(tag);
  case HTML_HR:
    return h_env->HTML_HR(tag);
  case HTML_PRE:
    return h_env->HTML_PRE_enter(tag);
  case HTML_N_PRE:
    return h_env->HTML_PRE_exit();
  case HTML_PRE_INT:
    return h_env->HTML_PRE_INT_enter();
  case HTML_N_PRE_INT:
    return h_env->HTML_PRE_INT_exit();
  case HTML_NOBR:
    return h_env->HTML_NOBR_enter();
  case HTML_N_NOBR:
    return h_env->HTML_NOBR_exit();
  case HTML_PRE_PLAIN:
    return h_env->HTML_PRE_PLAIN_enter();
  case HTML_N_PRE_PLAIN:
    return h_env->HTML_PRE_PLAIN_exit();
  case HTML_LISTING:
  case HTML_XMP:
  case HTML_PLAINTEXT:
    return h_env->HTML_PLAINTEXT_enter(tag);
  case HTML_N_LISTING:
  case HTML_N_XMP:
    return h_env->HTML_LISTING_exit();
  case HTML_SCRIPT:
    h_env->obuf.flag |= RB_SCRIPT;
    h_env->obuf.end_tag = HTML_N_SCRIPT;
    return 1;
  case HTML_STYLE:
    h_env->obuf.flag |= RB_STYLE;
    h_env->obuf.end_tag = HTML_N_STYLE;
    return 1;
  case HTML_N_SCRIPT:
    h_env->obuf.flag &= ~RB_SCRIPT;
    h_env->obuf.end_tag = 0;
    return 1;
  case HTML_N_STYLE:
    h_env->obuf.flag &= ~RB_STYLE;
    h_env->obuf.end_tag = 0;
    return 1;
  case HTML_A:
    return h_env->HTML_A_enter(tag);
  case HTML_N_A:
    this->close_anchor(h_env);
    return 1;
  case HTML_IMG:
    return h_env->HTML_IMG_enter(tag);
  case HTML_IMG_ALT:
    return h_env->HTML_IMG_ALT_enter(tag);
  case HTML_N_IMG_ALT:
    return h_env->HTML_IMG_ALT_exit();
  case HTML_INPUT_ALT:
    return h_env->HTML_INPUT_ALT_enter(tag);
  case HTML_N_INPUT_ALT:
    return h_env->HTML_INPUT_ALT_exit();
  case HTML_TABLE:
    return h_env->HTML_TABLE_enter(tag);
  case HTML_N_TABLE:
    // should be processed in HTMLlineproc()
    return 1;
  case HTML_CENTER:
    return h_env->HTML_CENTER_enter();
  case HTML_N_CENTER:
    return h_env->HTML_CENTER_exit();
  case HTML_DIV:
    return h_env->HTML_DIV_enter(tag);
  case HTML_N_DIV:
    return h_env->HTML_DIV_exit();
  case HTML_DIV_INT:
    return h_env->HTML_DIV_INT_enter(tag);
  case HTML_N_DIV_INT:
    return h_env->HTML_DIV_INT_exit();
  case HTML_FORM:
    return h_env->HTML_FORM_enter(tag);
  case HTML_N_FORM:
    return h_env->HTML_FORM_exit();
  case HTML_INPUT:
    return h_env->HTML_INPUT_enter(tag);
  case HTML_BUTTON:
    return h_env->HTML_BUTTON_enter(tag);
  case HTML_N_BUTTON:
    return h_env->HTML_BUTTON_exit();
  case HTML_SELECT:
    return h_env->HTML_SELECT_enter(tag);
  case HTML_N_SELECT:
    return h_env->HTML_SELECT_exit();
  case HTML_OPTION:
    /* nothing */
    return 1;
  case HTML_TEXTAREA:
    return h_env->HTML_TEXTAREA_enter(tag);
  case HTML_N_TEXTAREA:
    return h_env->HTML_TEXTAREA_exit();
  case HTML_ISINDEX:
    return h_env->HTML_ISINDEX_enter(tag);
  case HTML_DOCTYPE:
    if (!tag->parsedtag_exists(ATTR_PUBLIC)) {
      h_env->obuf.flag |= RB_HTML5;
    }
    return 1;
  case HTML_META:
    return h_env->HTML_META_enter(tag);
  case HTML_BASE:
  case HTML_MAP:
  case HTML_N_MAP:
  case HTML_AREA:
    return 0;
  case HTML_DEL:
    return h_env->HTML_DEL_enter();
  case HTML_N_DEL:
    return h_env->HTML_DEL_exit();
  case HTML_S:
    return h_env->HTML_S_enter();
  case HTML_N_S:
    return h_env->HTML_S_exit();
  case HTML_INS:
    return h_env->HTML_INS_enter();
  case HTML_N_INS:
    return h_env->HTML_INS_exit();
  case HTML_SUP:
    if (!(h_env->obuf.flag & (RB_DEL | RB_S)))
      HTMLlineproc1("^", h_env);
    return 1;
  case HTML_N_SUP:
    return 1;
  case HTML_SUB:
    if (!(h_env->obuf.flag & (RB_DEL | RB_S)))
      HTMLlineproc1("[", h_env);
    return 1;
  case HTML_N_SUB:
    if (!(h_env->obuf.flag & (RB_DEL | RB_S)))
      HTMLlineproc1("]", h_env);
    return 1;
  case HTML_FONT:
  case HTML_N_FONT:
  case HTML_NOP:
    return 1;
  case HTML_BGSOUND:
    return h_env->HTML_BGSOUND_enter(tag);
  case HTML_EMBED:
    return h_env->HTML_EMBED_enter(tag);
  case HTML_APPLET:
    return h_env->HTML_APPLET_enter(tag);
  case HTML_BODY:
    return h_env->HTML_BODY_enter(tag);
  case HTML_N_HEAD:
    if (h_env->obuf.flag & RB_TITLE)
      HTMLlineproc1("</title>", h_env);
    return 1;
  case HTML_HEAD:
  case HTML_N_BODY:
    return 1;
  default:
    /* h_env->obuf.prevchar = '\0'; */
    return 0;
  }

  return 0;
}

void HtmlParser::proc_escape(struct readbuffer *obuf, const char **str_return) {
  const char *str = *str_return;
  char32_t ech = getescapechar(str_return);
  int n_add = *str_return - str;
  Lineprop mode = PC_ASCII;

  if (ech < 0) {
    *str_return = str;
    proc_mchar(obuf, obuf->flag & RB_SPECIAL, 1, str_return, PC_ASCII);
    return;
  }
  mode = IS_CNTRL(ech) ? PC_CTRL : PC_ASCII;

  auto estr = conv_entity(ech);
  check_breakpoint(obuf, obuf->flag & RB_SPECIAL, estr.c_str());
  auto width = get_strwidth(estr.c_str());
  if (width == 1 && ech == (char32_t)estr[0] && ech != '&' && ech != '<' &&
      ech != '>') {
    if (IS_CNTRL(ech))
      mode = PC_CTRL;
    push_charp(obuf, width, estr.c_str(), mode);
  } else {
    obuf->push_nchars(width, str, n_add, mode);
  }
  set_prevchar(obuf->prevchar, estr.c_str(), estr.size());
  obuf->prev_ctype = mode;
}

int HtmlParser::table_width(struct html_feed_environ *h_env, int table_level) {
  int width;
  if (table_level < 0)
    return 0;
  width = tables[table_level]->total_width;
  if (table_level > 0 || width > 0)
    return width;
  return h_env->limit - h_env->envs[h_env->envc].indent;
}

static void clear_ignore_p_flag(int cmd, struct readbuffer *obuf) {
  static int clear_flag_cmd[] = {HTML_HR, HTML_UNKNOWN};
  int i;

  for (i = 0; clear_flag_cmd[i] != HTML_UNKNOWN; i++) {
    if (cmd == clear_flag_cmd[i]) {
      obuf->flag &= ~RB_IGNORE_P;
      return;
    }
  }
}

static int need_flushline(struct html_feed_environ *h_env,
                          struct readbuffer *obuf, Lineprop mode) {
  char ch;

  if (obuf->flag & RB_PRE_INT) {
    if (obuf->pos > h_env->limit)
      return 1;
    else
      return 0;
  }

  ch = Strlastchar(obuf->line);
  /* if (ch == ' ' && obuf->tag_sp > 0) */
  if (ch == ' ')
    return 0;

  if (obuf->pos > h_env->limit)
    return 1;

  return 0;
}

void HtmlParser::parseLine(std::string_view _line,
                           struct html_feed_environ *h_env, bool internal) {
  struct table *tbl = nullptr;
  struct table_mode *tbl_mode = nullptr;
  int tbl_width = 0;

  std::string __line{_line.begin(), _line.end()};
  auto line = __line.c_str();

table_start:
  if (h_env->obuf.table_level >= 0) {
    int level = std::min<short>(h_env->obuf.table_level, MAX_TABLE - 1);
    tbl = tables[level];
    tbl_mode = &table_mode[level];
    tbl_width = table_width(h_env, level);
  }

  while (*line != '\0') {
    const char *str, *p;
    int is_tag = false;
    int pre_mode = (h_env->obuf.table_level >= 0 && tbl_mode)
                       ? tbl_mode->pre_mode
                       : h_env->obuf.flag;
    int end_tag = (h_env->obuf.table_level >= 0 && tbl_mode)
                      ? tbl_mode->end_tag
                      : h_env->obuf.end_tag;

    if (*line == '<' || h_env->obuf.status != R_ST_NORMAL) {
      /*
       * Tag processing
       */
      if (h_env->obuf.status == R_ST_EOL)
        h_env->obuf.status = R_ST_NORMAL;
      else {
        read_token(h_env->tagbuf, &line, &h_env->obuf.status,
                   pre_mode & RB_PREMODE, h_env->obuf.status != R_ST_NORMAL);
        if (h_env->obuf.status != R_ST_NORMAL)
          return;
      }
      if (h_env->tagbuf->length == 0)
        continue;
      str = h_env->tagbuf->Strdup()->ptr;
      if (*str == '<') {
        if (str[1] && REALLY_THE_BEGINNING_OF_A_TAG(str))
          is_tag = true;
        else if (!(pre_mode & (RB_PLAIN | RB_INTXTA | RB_INSELECT | RB_SCRIPT |
                               RB_STYLE | RB_TITLE))) {
          line = Strnew_m_charp(str + 1, line, nullptr)->ptr;
          str = "&lt;";
        }
      }
    } else {
      auto tokbuf = Strnew();
      read_token(tokbuf, &line, &h_env->obuf.status, pre_mode & RB_PREMODE, 0);
      if (h_env->obuf.status != R_ST_NORMAL) /* R_ST_AMP ? */
        h_env->obuf.status = R_ST_NORMAL;
      str = tokbuf->ptr;
      if (need_number) {
        str = Strnew_m_charp(getLinkNumberStr(-1)->ptr, str, nullptr)->ptr;
        need_number = 0;
      }
    }

    if (pre_mode & (RB_PLAIN | RB_INTXTA | RB_INSELECT | RB_SCRIPT | RB_STYLE |
                    RB_TITLE)) {
      if (is_tag) {
        p = str;
        if (auto tag = HtmlTag::parse(&p, internal)) {
          if (tag->tagid == end_tag ||
              (pre_mode & RB_INSELECT && tag->tagid == HTML_N_FORM) ||
              (pre_mode & RB_TITLE &&
               (tag->tagid == HTML_N_HEAD || tag->tagid == HTML_BODY)))
            goto proc_normal;
        }
      }
      /* title */
      if (pre_mode & RB_TITLE) {
        feed_title(str);
        continue;
      }
      /* select */
      if (pre_mode & RB_INSELECT) {
        if (h_env->obuf.table_level >= 0)
          goto proc_normal;
        this->feed_select(str);
        continue;
      }
      if (is_tag) {
        if (strncmp(str, "<!--", 4) && (p = strchr(str + 1, '<'))) {
          str = Strnew_charp_n(str, p - str)->ptr;
          line = Strnew_m_charp(p, line, nullptr)->ptr;
        }
        is_tag = false;
        continue;
      }
      if (h_env->obuf.table_level >= 0)
        goto proc_normal;
      /* textarea */
      if (pre_mode & RB_INTXTA) {
        feed_textarea(str);
        continue;
      }
      /* script */
      if (pre_mode & RB_SCRIPT)
        continue;
      /* style */
      if (pre_mode & RB_STYLE)
        continue;
    }

  proc_normal:
    if (h_env->obuf.table_level >= 0 && tbl && tbl_mode) {
      /*
       * within table: in <table>..</table>, all input tokens
       * are fed to the table renderer, and then the renderer
       * makes HTML output.
       */
      switch (tbl->feed_table(this, str, tbl_mode, tbl_width, internal)) {
      case 0:
        /* </table> tag */
        h_env->obuf.table_level--;
        if (h_env->obuf.table_level >= MAX_TABLE - 1)
          continue;
        tbl->end_table();
        if (h_env->obuf.table_level >= 0) {
          struct table *tbl0 = tables[h_env->obuf.table_level];
          str = Sprintf("<table_alt tid=%d>", tbl0->ntable)->ptr;
          if (tbl0->row < 0)
            continue;
          tbl0->pushTable(tbl);
          tbl = tbl0;
          tbl_mode = &table_mode[h_env->obuf.table_level];
          tbl_width = table_width(h_env, h_env->obuf.table_level);
          tbl->feed_table(this, str, tbl_mode, tbl_width, true);
          continue;
          /* continue to the next */
        }
        if (h_env->obuf.flag & RB_DEL)
          continue;
        /* all tables have been read */
        if (tbl->vspace > 0 && !(h_env->obuf.flag & RB_IGNORE_P)) {
          int indent = h_env->envs[h_env->envc].indent;
          flushline(h_env, indent, 0, h_env->limit);
          do_blankline(h_env, &h_env->obuf, indent, 0, h_env->limit);
        }
        save_fonteffect(h_env);
        initRenderTable();
        tbl->renderTable(this, tbl_width, h_env);
        restore_fonteffect(h_env);
        h_env->obuf.flag &= ~RB_IGNORE_P;
        if (tbl->vspace > 0) {
          int indent = h_env->envs[h_env->envc].indent;
          do_blankline(h_env, &h_env->obuf, indent, 0, h_env->limit);
          h_env->obuf.flag |= RB_IGNORE_P;
        }
        set_space_to_prevchar(h_env->obuf.prevchar);
        continue;
      case 1:
        /* <table> tag */
        break;
      default:
        continue;
      }
    }

    if (is_tag) {
      /*** Beginning of a new tag ***/
      HtmlCommand cmd;
      std::shared_ptr<HtmlTag> tag;
      if ((tag = HtmlTag::parse(&str, internal)))
        cmd = tag->tagid;
      else
        continue;
      /* process tags */
      if (pushHtmlTag(tag, h_env) == 0) {
        /* preserve the tag for second-stage processing */
        if (tag->parsedtag_need_reconstruct())
          h_env->tagbuf = tag->parsedtag2str();
        h_env->obuf.push_tag(h_env->tagbuf->ptr, cmd);
      }
      h_env->obuf.bp.init_flag = 1;
      clear_ignore_p_flag(cmd, &h_env->obuf);
      if (cmd == HTML_TABLE)
        goto table_start;
      else {
        if (displayLinkNumber && cmd == HTML_A && !internal)
          if (h_env->obuf.anchor.url.size()) {
            need_number = 1;
          }
        continue;
      }
    }

    if (h_env->obuf.flag & (RB_DEL | RB_S))
      continue;
    while (*str) {
      auto mode = get_mctype(str);
      int delta = get_mcwidth(str);
      if (h_env->obuf.flag & (RB_SPECIAL & ~RB_NOBR)) {
        char ch = *str;
        if (!(h_env->obuf.flag & RB_PLAIN) && (*str == '&')) {
          const char *p = str;
          int ech = getescapechar(&p);
          if (ech == '\n' || ech == '\r') {
            ch = '\n';
            str = p - 1;
          } else if (ech == '\t') {
            ch = '\t';
            str = p - 1;
          }
        }
        if (ch != '\n')
          h_env->obuf.flag &= ~RB_IGNORE_P;
        if (ch == '\n') {
          str++;
          if (h_env->obuf.flag & RB_IGNORE_P) {
            h_env->obuf.flag &= ~RB_IGNORE_P;
            continue;
          }
          if (h_env->obuf.flag & RB_PRE_INT)
            push_char(&h_env->obuf, h_env->obuf.flag & RB_SPECIAL, ' ');
          else
            flushline(h_env, h_env->envs[h_env->envc].indent, 1, h_env->limit);
        } else if (ch == '\t') {
          do {
            push_char(&h_env->obuf, h_env->obuf.flag & RB_SPECIAL, ' ');
          } while ((h_env->envs[h_env->envc].indent + h_env->obuf.pos) %
                       Tabstop !=
                   0);
          str++;
        } else if (h_env->obuf.flag & RB_PLAIN) {
          auto p = html_quote_char(*str);
          if (p) {
            push_charp(&h_env->obuf, 1, p, PC_ASCII);
            str++;
          } else {
            proc_mchar(&h_env->obuf, 1, delta, &str, mode);
          }
        } else {
          if (*str == '&')
            proc_escape(&h_env->obuf, &str);
          else
            proc_mchar(&h_env->obuf, 1, delta, &str, mode);
        }
        if (h_env->obuf.flag & (RB_SPECIAL & ~RB_PRE_INT))
          continue;
      } else {
        if (!IS_SPACE(*str))
          h_env->obuf.flag &= ~RB_IGNORE_P;
        if ((mode == PC_ASCII || mode == PC_CTRL) && IS_SPACE(*str)) {
          if (*h_env->obuf.prevchar->ptr != ' ') {
            push_char(&h_env->obuf, h_env->obuf.flag & RB_SPECIAL, ' ');
          }
          str++;
        } else {
          if (*str == '&')
            proc_escape(&h_env->obuf, &str);
          else
            proc_mchar(&h_env->obuf, h_env->obuf.flag & RB_SPECIAL, delta, &str,
                       mode);
        }
      }
      if (need_flushline(h_env, &h_env->obuf, mode)) {
        char *bp = h_env->obuf.line->ptr + h_env->obuf.bp.len;
        char *tp = bp - h_env->obuf.bp.tlen;
        int i = 0;

        if (tp > h_env->obuf.line->ptr && tp[-1] == ' ')
          i = 1;

        int indent = h_env->envs[h_env->envc].indent;
        if (h_env->obuf.bp.pos - i > indent) {
          Str *line;
          h_env->obuf.append_tags(); /* may reallocate the buffer */
          bp = h_env->obuf.line->ptr + h_env->obuf.bp.len;
          line = Strnew_charp(bp);
          Strshrink(h_env->obuf.line,
                    h_env->obuf.line->length - h_env->obuf.bp.len);
          if (h_env->obuf.pos - i > h_env->limit)
            h_env->obuf.flag |= RB_FILL;
          h_env->obuf.back_to_breakpoint();
          flushline(h_env, indent, 0, h_env->limit);
          h_env->obuf.flag &= ~RB_FILL;
          HTMLlineproc1(line->ptr, h_env);
        }
      }
    }
  }
  if (!(h_env->obuf.flag & (RB_SPECIAL | RB_INTXTA | RB_INSELECT))) {
    char *tp;
    int i = 0;

    if (h_env->obuf.bp.pos == h_env->obuf.pos) {
      tp = &h_env->obuf.line->ptr[h_env->obuf.bp.len - h_env->obuf.bp.tlen];
    } else {
      tp = &h_env->obuf.line->ptr[h_env->obuf.line->length];
    }

    if (tp > h_env->obuf.line->ptr && tp[-1] == ' ')
      i = 1;
    int indent = h_env->envs[h_env->envc].indent;
    if (h_env->obuf.pos - i > h_env->limit) {
      h_env->obuf.flag |= RB_FILL;
      flushline(h_env, indent, 0, h_env->limit);
      h_env->obuf.flag &= ~RB_FILL;
    }
  }
}

Str *HtmlParser::process_n_button() {
  Str *tmp = Strnew();
  Strcat_charp(tmp, "</input_alt>");
  /*    Strcat_charp(tmp, "</pre_int>"); */
  return tmp;
}

Str *HtmlParser::process_select(const std::shared_ptr<HtmlTag> &tag) {
  Str *tmp = nullptr;

  if (cur_form_id() < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp = process_form(HtmlTag::parse(&s, true));
  }

  auto p = "";
  tag->parsedtag_get_value(ATTR_NAME, &p);
  cur_select = Strnew_charp(p);
  select_is_multiple = tag->parsedtag_exists(ATTR_MULTIPLE);

  select_str = Strnew();
  cur_option = nullptr;
  cur_status = R_ST_NORMAL;
  n_selectitem = 0;
  return tmp;
}

Str *HtmlParser::process_n_select() {
  if (cur_select == nullptr)
    return nullptr;
  this->process_option();
  Strcat_charp(select_str, "<br>");
  cur_select = nullptr;
  n_selectitem = 0;
  return select_str;
}

void HtmlParser::feed_select(const char *str) {
  Str *tmp = Strnew();
  int prev_status = cur_status;
  static int prev_spaces = -1;
  const char *p;

  if (cur_select == nullptr)
    return;
  while (read_token(tmp, &str, &cur_status, 0, 0)) {
    if (cur_status != R_ST_NORMAL || prev_status != R_ST_NORMAL)
      continue;
    p = tmp->ptr;
    if (tmp->ptr[0] == '<' && Strlastchar(tmp) == '>') {
      std::shared_ptr<HtmlTag> tag;
      char *q;
      if (!(tag = HtmlTag::parse(&p, false)))
        continue;
      switch (tag->tagid) {
      case HTML_OPTION:
        this->process_option();
        cur_option = Strnew();
        if (tag->parsedtag_get_value(ATTR_VALUE, &q))
          cur_option_value = Strnew_charp(q);
        else
          cur_option_value = nullptr;
        if (tag->parsedtag_get_value(ATTR_LABEL, &q))
          cur_option_label = Strnew_charp(q);
        else
          cur_option_label = nullptr;
        cur_option_selected = tag->parsedtag_exists(ATTR_SELECTED);
        prev_spaces = -1;
        break;
      case HTML_N_OPTION:
        /* do nothing */
        break;
      default:
        /* never happen */
        break;
      }
    } else if (cur_option) {
      while (*p) {
        if (IS_SPACE(*p) && prev_spaces != 0) {
          p++;
          if (prev_spaces > 0)
            prev_spaces++;
        } else {
          if (IS_SPACE(*p))
            prev_spaces = 1;
          else
            prev_spaces = 0;
          if (*p == '&')
            Strcat(cur_option, getescapecmd(&p));
          else
            Strcat_char(cur_option, *(p++));
        }
      }
    }
  }
}

void HtmlParser::process_option() {
  char begin_char = '[', end_char = ']';

  if (cur_select == nullptr || cur_option == nullptr)
    return;
  while (cur_option->length > 0 && IS_SPACE(Strlastchar(cur_option)))
    Strshrink(cur_option, 1);
  if (cur_option_value == nullptr)
    cur_option_value = cur_option;
  if (cur_option_label == nullptr)
    cur_option_label = cur_option;
  if (!select_is_multiple) {
    begin_char = '(';
    end_char = ')';
  }
  Strcat(select_str, Sprintf("<br><pre_int>%c<input_alt hseq=\"%d\" "
                             "fid=\"%d\" type=%s name=\"%s\" value=\"%s\"",
                             begin_char, this->cur_hseq++, cur_form_id(),
                             select_is_multiple ? "checkbox" : "radio",
                             html_quote(cur_select->ptr),
                             html_quote(cur_option_value->ptr)));
  if (cur_option_selected)
    Strcat_charp(select_str, " checked>*</input_alt>");
  else
    Strcat_charp(select_str, "> </input_alt>");
  Strcat_char(select_str, end_char);
  Strcat_charp(select_str, html_quote(cur_option_label->ptr));
  Strcat_charp(select_str, "</pre_int>");
  n_selectitem++;
}

void HtmlParser::completeHTMLstream(struct html_feed_environ *h_env) {
  auto obuf = &h_env->obuf;
  this->close_anchor(h_env);
  if (obuf->img_alt) {
    obuf->push_tag("</img_alt>", HTML_N_IMG_ALT);
    obuf->img_alt = nullptr;
  }
  if (obuf->input_alt.in) {
    obuf->push_tag("</input_alt>", HTML_N_INPUT_ALT);
    obuf->input_alt.hseq = 0;
    obuf->input_alt.fid = -1;
    obuf->input_alt.in = 0;
    obuf->input_alt.type = nullptr;
    obuf->input_alt.name = nullptr;
    obuf->input_alt.value = nullptr;
  }
  if (obuf->fontstat.in_bold) {
    obuf->push_tag("</b>", HTML_N_B);
    obuf->fontstat.in_bold = 0;
  }
  if (obuf->fontstat.in_italic) {
    obuf->push_tag("</i>", HTML_N_I);
    obuf->fontstat.in_italic = 0;
  }
  if (obuf->fontstat.in_under) {
    obuf->push_tag("</u>", HTML_N_U);
    obuf->fontstat.in_under = 0;
  }
  if (obuf->fontstat.in_strike) {
    obuf->push_tag("</s>", HTML_N_S);
    obuf->fontstat.in_strike = 0;
  }
  if (obuf->fontstat.in_ins) {
    obuf->push_tag("</ins>", HTML_N_INS);
    obuf->fontstat.in_ins = 0;
  }
  if (obuf->flag & RB_INTXTA)
    this->HTMLlineproc1("</textarea>", h_env);
  /* for unbalanced select tag */
  if (obuf->flag & RB_INSELECT)
    this->HTMLlineproc1("</select>", h_env);
  if (obuf->flag & RB_TITLE)
    this->HTMLlineproc1("</title>", h_env);

  /* for unbalanced table tag */
  if (obuf->table_level >= MAX_TABLE)
    obuf->table_level = MAX_TABLE - 1;

  while (obuf->table_level >= 0) {
    int tmp = obuf->table_level;
    table_mode[obuf->table_level].pre_mode &=
        ~(TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN);
    this->HTMLlineproc1("</table>", h_env);
    if (obuf->table_level >= tmp)
      break;
  }
}
