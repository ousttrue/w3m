#include "html_feed_env.h"
#include "form.h"
#include "stringtoken.h"
#include "ctrlcode.h"
#include "html_tag.h"
#include "option_param.h"
#include "myctype.h"
#include "line_layout.h"
#include "quote.h"
#include "entity.h"
#include "url_quote.h"
#include "cmp.h"
#include "push_symbol.h"
#include "html_token.h"
#include "html_renderer.h"
#include "html_table_status.h"
#include "html_table.h"
#include "html_meta.h"
#include <assert.h>
#include <sstream>

#define MAX_UL_LEVEL 9
#define MAX_CELLSPACING 1000
#define MAX_CELLPADDING 1000
#define MAX_VSPACE 1000

struct html_impl {};

html_feed_environ::html_feed_environ(int nenv, int limit_width, int indent,
                                     const std::shared_ptr<GeneralList> &_buf)
    : _impl(new html_impl), buf(_buf), _width(limit_width) {
  assert(nenv);
  envs.resize(nenv);
  envs[0].indent = indent;

  this->prevchar = " ";
  this->flag = RB_IGNORE_P;
  this->status = R_ST_NORMAL;
  this->prev_ctype = PC_ASCII;
  this->bp.init_flag = 1;
  this->set_breakpoint(0);
}

html_feed_environ::~html_feed_environ() { delete _impl; }

void html_feed_environ::purgeline() {
  if (this->buf == NULL || this->blank_lines == 0)
    return;

  std::shared_ptr<TextLine> tl;
  if (!(tl = this->buf->rpopValue()))
    return;

  const char *p = tl->line.c_str();
  std::stringstream tmp;
  while (*p) {
    stringtoken st(p);
    auto token = st.sloppy_parse_line();
    p = st.ptr();
    if (token) {
      tmp << *token;
    }
  }
  this->buf->_list.push_back(std::make_shared<TextLine>(tmp.str()));
  this->blank_lines--;
}

void html_feed_environ::POP_ENV() {
  if (this->envc_real-- < (int)this->envs.size()) {
    this->envc--;
  }
}

void html_feed_environ::PUSH_ENV_NOINDENT(HtmlCommand cmd) {
  if (++this->envc_real < (int)this->envs.size()) {
    ++this->envc;
    envs[this->envc].env = cmd;
    envs[this->envc].count = 0;
    envs[this->envc].indent = envs[this->envc - 1].indent;
  }
}

void html_feed_environ::PUSH_ENV(HtmlCommand cmd) {
  if (++this->envc_real < (int)this->envs.size()) {
    ++this->envc;
    envs[this->envc].env = cmd;
    envs[this->envc].count = 0;
    if (this->envc <= MAX_INDENT_LEVEL)
      envs[this->envc].indent = envs[this->envc - 1].indent + INDENT_INCR;
    else
      envs[this->envc].indent = envs[this->envc - 1].indent;
  }
}

#define FORM_I_TEXT_DEFAULT_SIZE 40
std::shared_ptr<FormItem>
html_feed_environ::createFormItem(const std::shared_ptr<HtmlTag> &tag) {
  auto item = std::make_shared<FormItem>();
  item->type = FORM_UNKNOWN;
  item->size = -1;
  item->rows = 0;
  item->checked = item->init_checked = 0;
  item->accept = 0;
  item->value = item->init_value = "";
  item->readonly = 0;

  if (auto value = tag->getAttr(ATTR_TYPE)) {
    item->type = formtype(*value);
    if (item->size < 0 &&
        (item->type == FORM_INPUT_TEXT || item->type == FORM_INPUT_FILE ||
         item->type == FORM_INPUT_PASSWORD))
      item->size = FORM_I_TEXT_DEFAULT_SIZE;
  }
  if (auto value = tag->getAttr(ATTR_NAME)) {
    item->name = *value;
  }
  if (auto value = tag->getAttr(ATTR_VALUE)) {
    item->value = item->init_value = *value;
  }
  item->checked = item->init_checked = tag->existsAttr(ATTR_CHECKED);
  item->accept = tag->existsAttr(ATTR_ACCEPT);
  if (auto value = tag->getAttr(ATTR_SIZE)) {
    item->size = stoi(*value);
  }
  if (auto value = tag->getAttr(ATTR_MAXLENGTH)) {
    item->maxlength = stoi(*value);
  }
  item->readonly = tag->existsAttr(ATTR_READONLY);

  if (auto value = tag->getAttr(ATTR_TEXTAREANUMBER)) {
    auto i = stoi(*value);
    if (i >= 0 && i < (int)this->textarea_str.size()) {
      item->value = item->init_value = this->textarea_str[i];
    }
  }
  if (auto value = tag->getAttr(ATTR_ROWS)) {
    item->rows = stoi(*value);
  }
  if (item->type == FORM_UNKNOWN) {
    /* type attribute is missing. Ignore the tag. */
    return NULL;
  }
  if (item->type == FORM_INPUT_FILE && item->value.size()) {
    /* security hole ! */
    assert(false);
    return NULL;
  }
  return item;
}

std::shared_ptr<LineData>
loadHTMLstream(int width, const Url &currentURL, std::string_view body,
               const std::shared_ptr<AnchorList<FormAnchor>> &old,
               bool internal) {
  html_feed_environ htmlenv1(MAX_ENV_LEVEL, width, 0,
                             GeneralList::newGeneralList());
  htmlenv1.parse(body, internal);
  htmlenv1.status = R_ST_NORMAL;
  htmlenv1.completeHTMLstream();
  return HtmlRenderer().render(currentURL, &htmlenv1, old);
}

bool pseudoInlines = true;
bool ignore_null_img_alt = true;
int pixel_per_char_i = static_cast<int>(DEFAULT_PIXEL_PER_CHAR);
bool displayLinkNumber = false;
int IndentIncr = 4;
bool DisplayBorders = false;
int displayInsDel = DISPLAY_INS_DEL_NORMAL;
bool view_unseenobject = true;
bool MetaRefresh = false;

#define FORMSTACK_SIZE 10
#define FRAMESTACK_SIZE 10

static const char *_size_unit[] = {"b",  "kb", "Mb", "Gb", "Tb", "Pb",
                                   "Eb", "Zb", "Bb", "Yb", NULL};

// char *convert_size(long long size, int usefloat) {
//   float csize;
//   int sizepos = 0;
//   auto sizes = _size_unit;
//
//   csize = (float)size;
//   while (csize >= 999.495 && sizes[sizepos + 1]) {
//     csize = csize / 1024.0;
//     sizepos++;
//   }
//   return Sprintf(usefloat ? "%.3g%s" : "%.0f%s",
//                  floor(csize * 100.0 + 0.5) / 100.0, sizes[sizepos])
//       ->ptr;
// }

// char *convert_size2(long long size1, long long size2, int usefloat) {
//   auto sizes = _size_unit;
//   float csize, factor = 1;
//   int sizepos = 0;
//
//   csize = (float)((size1 > size2) ? size1 : size2);
//   while (csize / factor >= 999.495 && sizes[sizepos + 1]) {
//     factor *= 1024.0;
//     sizepos++;
//   }
//   return Sprintf(usefloat ? "%.3g/%.3g%s" : "%.0f/%.0f%s",
//                  floor(size1 / factor * 100.0 + 0.5) / 100.0,
//                  floor(size2 / factor * 100.0 + 0.5) / 100.0, sizes[sizepos])
//       ->ptr;
// }

void html_feed_environ::set_alignment(ReadBufferFlags flag) {
  this->RB_SAVE_FLAG();
  if (flag != (ReadBufferFlags)-1) {
    this->RB_SET_ALIGN(flag);
  }
}

void html_feed_environ::set_breakpoint(int tag_length) {
  this->bp.len = this->line.size();
  this->bp.pos = this->pos;
  this->bp.tlen = tag_length;
  this->bp.flag = this->flag;
  this->bp.flag &= ~RB_FILL;
  this->bp.top_margin = this->top_margin;
  this->bp.bottom_margin = this->bottom_margin;

  if (!this->bp.init_flag)
    return;

  this->bp.img_alt = this->img_alt;
  this->bp.input_alt = this->input_alt;
  this->bp.fontstat = this->fontstat;
  this->bp.nobr_level = this->nobr_level;
  this->bp.prev_ctype = this->prev_ctype;
  this->bp.init_flag = 0;
}

void html_feed_environ::push_link(HtmlCommand cmd, int offset, int pos) {
  link_stack.push_front({
      .cmd = cmd,
      .offset = (short)offset,
      .pos = (short)pos,
  });
  auto p = &link_stack.front();
  if (p->offset < 0)
    p->offset = 0;
  if (p->pos < 0)
    p->pos = 0;
}

void html_feed_environ::append_tags() {
  int len = this->line.size();
  bool set_bp = false;
  for (auto &tag : this->tag_stack) {
    switch (tag->cmd) {
    case HTML_A:
    case HTML_IMG_ALT:
    case HTML_B:
    case HTML_U:
    case HTML_I:
    case HTML_S:
      this->push_link(tag->cmd, this->line.size(), this->pos);
      break;
    }
    this->line += tag->cmdname;
    switch (tag->cmd) {
    case HTML_NOBR:
      if (this->nobr_level > 1)
        break;
    case HTML_WBR:
      set_bp = 1;
      break;
    }
  }
  this->tag_stack.clear();
  if (set_bp) {
    this->set_breakpoint(this->line.size() - len);
  }
}

void html_feed_environ::passthrough(const std::string &_str, bool back) {

  // if (back) {
  //   std::string str_save = str;
  //   Strshrink(this->line, this->line.c_str() + this->line.size() - str);
  //   str = str_save;
  // }

  HtmlCommand cmd;
  auto str = _str.c_str();
  while (*str) {
    auto str_bak = str;
    stringtoken st(str);
    auto token = st.sloppy_parse_line();
    str = (char *)st.ptr();
    if (token) {
      const char *q = str_bak;
      cmd = gethtmlcmd(&q);
      if (back) {
        for (auto p = link_stack.begin(); p != link_stack.end(); ++p) {
          if (p->cmd == cmd) {
            for (auto i = link_stack.begin(); i != p;) {
              i = link_stack.erase(i);
            }
            // link_stack = p->next;
            break;
          }
        }
        back = 0;
      } else {
        this->push_tag(*token, cmd);
      }
    } else {
      this->push_nchars(0, str_bak, str - str_bak, this->prev_ctype);
    }
  }
}

void html_feed_environ::push_tag(std::string_view cmdname, HtmlCommand cmd) {
  auto tag = std::make_shared<cmdtable>();
  tag->cmdname = cmdname;
  tag->cmd = cmd;
  tag_stack.push_front(tag);
  if (this->flag & (RB_SPECIAL & ~RB_NOBR)) {
    this->append_tags();
  }
}

void html_feed_environ::push_nchars(int width, const char *str, int len,
                                    Lineprop mode) {
  this->append_tags();
  this->line += std::string(str, len);
  this->pos += width;
  if (width > 0) {
    this->prevchar = std::string(str, len);
    this->prev_ctype = mode;
  }
  this->flag |= RB_NFLUSHED;
}

void html_feed_environ::proc_mchar(int pre_mode, int width, const char **str,
                                   Lineprop mode) {
  this->check_breakpoint(pre_mode, *str);
  this->pos += width;
  this->line += std::string(*str, get_mclen(*str));
  if (width > 0) {
    this->prevchar = std::string(*str, 1);
    if (**str != ' ')
      this->prev_ctype = mode;
  }
  (*str) += get_mclen(*str);
  this->flag |= RB_NFLUSHED;
}

void html_feed_environ::flushline(FlushLineMode force) {
  if (!(this->flag & (RB_SPECIAL & ~RB_NOBR)) && line.size() &&
      line.back() == ' ') {
    line.pop_back();
    this->pos--;
  }

  this->append_tags();

  auto hidden = pop_hidden();

  std::string pass;
  if (hidden.str) {
    pass = hidden.str;
    Strshrink(line, line.c_str() + line.size() - hidden.str);
  }

  if (!(this->flag & (RB_SPECIAL & ~RB_NOBR)) && this->pos > this->_width) {
    char *tp = &line[this->bp.len - this->bp.tlen];
    char *ep = &line[line.size()];

    if (this->bp.pos == this->pos && tp <= ep && tp > line.c_str() &&
        tp[-1] == ' ') {
      memcpy(tp - 1, tp, ep - tp + 1);
      line.pop_back();
      this->pos--;
    }
  }

  line += hidden.suffix;

  flush_top_margin(force);

  if (force == FlushLineMode::Force || this->flag & RB_NFLUSHED) {
    // line completed
    if (auto textline =
            make_textline(this->envs[this->envc].indent, this->_width)) {
      if (buf) {
        buf->_list.push_back(textline);
      }
    }
  } else if (force == FlushLineMode::Append) {
    // append
    for (const char *p = line.c_str(); *p;) {
      stringtoken st(p);
      auto token = st.sloppy_parse_line();
      p = st.ptr();
      if (token && buf) {
        buf->_list.push_back(std::make_shared<TextLine>(*token, 0));
      }
    }
    if (pass.size() && buf) {
      buf->_list.push_back(std::make_shared<TextLine>(pass));
    }
    pass = {};
  } else {
    //
    std::stringstream tmp2;
    for (const char *p = line.c_str(); *p;) {
      stringtoken st(p);
      auto token = st.sloppy_parse_line();
      p = st.ptr();
      if (token) {
        if (force == FlushLineMode::Append) {
          if (buf) {
            buf->_list.push_back(std::make_shared<TextLine>(*token, 0));
          }
        } else {
          tmp2 << *token;
        }
      }
    }

    if (force == FlushLineMode::Append) {
      if (pass.size()) {
        if (buf) {
          buf->_list.push_back(std::make_shared<TextLine>(pass));
        }
      }
      pass = {};
    } else {
      if (pass.size())
        tmp2 << pass;
      pass = tmp2.str();
    }
  }

  flush_bottom_margin(force);

  if (this->top_margin < 0 || this->bottom_margin < 0) {
    return;
  }

  flush_end(hidden, pass);
}

std::shared_ptr<TextLine> html_feed_environ::make_textline(int indent,
                                                           int width) {
  auto lbuf = std::make_shared<TextLine>(this->line, this->pos);
  if (this->RB_GET_ALIGN() == RB_CENTER) {
    lbuf->align(width, ALIGN_CENTER);
  } else if (this->RB_GET_ALIGN() == RB_RIGHT) {
    lbuf->align(width, ALIGN_RIGHT);
  } else if (this->RB_GET_ALIGN() == RB_LEFT && this->flag & RB_INTABLE) {
    lbuf->align(width, ALIGN_LEFT);
  } else if (this->flag & RB_FILL) {
    const char *p;
    int rest, rrest;
    int nspace, d, i;

    rest = width - get_strwidth(line.c_str());
    if (rest > 1) {
      nspace = 0;
      for (p = line.c_str() + indent; *p; p++) {
        if (*p == ' ')
          nspace++;
      }
      if (nspace > 0) {
        int indent_here = 0;
        d = rest / nspace;
        p = line.c_str();
        while (IS_SPACE(*p)) {
          p++;
          indent_here++;
        }
        rrest = rest - d * nspace;
        line = {};
        for (i = 0; i < indent_here; i++)
          line += ' ';
        for (; *p; p++) {
          line += *p;
          if (*p == ' ') {
            for (i = 0; i < d; i++)
              line += ' ';
            if (rrest > 0) {
              line += ' ';
              rrest--;
            }
          }
        }
        lbuf = std::make_shared<TextLine>(line.c_str(), width);
      }
    }
  }

  setMaxLimit(lbuf->pos);

  if (this->flag & RB_SPECIAL || this->flag & RB_NFLUSHED) {
    this->blank_lines = 0;
  } else {
    this->blank_lines++;
  }

  return lbuf;
}

void html_feed_environ::flush_end(const Hidden &hidden,
                                  const std::string &pass) {

  this->line = {};
  this->pos = 0;
  this->top_margin = 0;
  this->bottom_margin = 0;
  this->prevchar = " ";
  this->bp.init_flag = 1;
  this->flag &= ~RB_NFLUSHED;
  this->set_breakpoint(0);
  this->prev_ctype = PC_ASCII;
  this->link_stack.clear();
  this->fillline();
  if (pass.size())
    this->passthrough(pass, 0);
  if (!hidden.anchor && this->anchor.url.size()) {
    if (this->anchor.hseq > 0)
      this->anchor.hseq = -this->anchor.hseq;
    std::stringstream tmp;
    tmp << "<A HSEQ=\"" << this->anchor.hseq << "\" HREF=\"";
    tmp << html_quote(this->anchor.url);
    if (this->anchor.target.size()) {
      tmp << "\" TARGET=\"";
      tmp << html_quote(this->anchor.target);
    }
    if (this->anchor.option.use_referer()) {
      tmp << "\" REFERER=\"";
      tmp << html_quote(this->anchor.option.referer);
    }
    if (this->anchor.title.size()) {
      tmp << "\" TITLE=\"";
      tmp << html_quote(this->anchor.title);
    }
    if (this->anchor.accesskey) {
      auto c = html_quote_char(this->anchor.accesskey);
      tmp << "\" ACCESSKEY=\"";
      if (c)
        tmp << c;
      else
        tmp << this->anchor.accesskey;
    }
    tmp << "\">";
    this->push_tag(tmp.str(), HTML_A);
  }
  if (!hidden.img && this->img_alt.size()) {
    std::string tmp = "<IMG_ALT SRC=\"";
    tmp += html_quote(this->img_alt);
    tmp += "\">";
    this->push_tag(tmp, HTML_IMG_ALT);
  }
  if (!hidden.input && this->input_alt.in) {
    if (this->input_alt.hseq > 0)
      this->input_alt.hseq = -this->input_alt.hseq;
    std::stringstream tmp;
    tmp << "<INPUT_ALT hseq=\"" << this->input_alt.hseq << "\" fid=\""
        << this->input_alt.fid << "\" name=\"" << this->input_alt.name
        << "\" type=\"" << this->input_alt.type << "\" "
        << "value=\"" << this->input_alt.value << "\">";
    this->push_tag(tmp.str(), HTML_INPUT_ALT);
  }
  if (!hidden.bold && this->fontstat.in_bold)
    this->push_tag("<B>", HTML_B);
  if (!hidden.italic && this->fontstat.in_italic)
    this->push_tag("<I>", HTML_I);
  if (!hidden.under && this->fontstat.in_under)
    this->push_tag("<U>", HTML_U);
  if (!hidden.strike && this->fontstat.in_strike)
    this->push_tag("<S>", HTML_S);
  if (!hidden.ins && this->fontstat.in_ins)
    this->push_tag("<INS>", HTML_INS);
}

void html_feed_environ::CLOSE_P() {
  if (this->flag & RB_P) {
    this->flushline();
    this->RB_RESTORE_FLAG();
    this->flag &= ~RB_P;
  }
}

void html_feed_environ::parse_end() {
  if (!(this->flag & (RB_SPECIAL | RB_INTXTA | RB_INSELECT))) {
    char *tp;
    if (this->bp.pos == this->pos) {
      tp = &this->line[this->bp.len - this->bp.tlen];
    } else {
      tp = &this->line[this->line.size()];
    }

    int i = 0;
    if (tp > this->line.c_str() && tp[-1] == ' ')
      i = 1;
    if (this->pos - i > this->_width) {
      this->flag |= RB_FILL;
      this->flushline();
      this->flag &= ~RB_FILL;
    }
  }
}

void html_feed_environ::process_title() {
  if (pre_title.size()) {
    return;
  }
  cur_title = "";
}

std::string html_feed_environ::process_n_title() {
  if (pre_title.size())
    return {};
  if (cur_title.empty())
    return {};

  Strremovefirstspaces(cur_title);
  Strremovetrailingspaces(cur_title);

  std::stringstream tmp;
  tmp << "<title_alt title=\"" << html_quote(cur_title) << "\">";
  pre_title = cur_title;
  cur_title = nullptr;
  return tmp.str();
}

void html_feed_environ::feed_title(const std::string &_str) {
  if (pre_title.size())
    return;
  if (cur_title.empty())
    return;
  auto str = _str.c_str();
  while (*str) {
    if (*str == '&') {
      cur_title += getescapecmd(&str);
    } else if (*str == '\n' || *str == '\r') {
      cur_title.push_back(' ');
      str++;
    } else {
      cur_title += *(str++);
    }
  }
}

#define HR_ATTR_WIDTH_MAX 65535
#define MAX_INPUT_SIZE 80 /* TODO - max should be screen line length */

#define IMG_SYMBOL 32 + (12)
#define HR_SYMBOL 26
#define TEXTAREA_ATTR_COL_MAX 4096
#define TEXTAREA_ATTR_ROWS_MAX 4096
#define INITIAL_FORM_SIZE 10

void html_feed_environ::CLOSE_DT() {
  if (this->flag & RB_IN_DT) {
    this->flag &= ~RB_IN_DT;
    parse("</b>");
  }
}

void html_feed_environ::CLOSE_A() {
  this->CLOSE_P();
  if (!(this->flag & RB_HTML5)) {
    this->close_anchor();
  }
}

void html_feed_environ::HTML5_CLOSE_A() {
  if (this->flag & RB_HTML5) {
    this->close_anchor();
  }
}

std::string html_feed_environ::getLinkNumberStr(int correction) const {
  std::stringstream ss;
  ss << "[" << (cur_hseq() + correction) << "]";
  return ss.str();
}

void html_feed_environ::push_render_image(const std::string &str, int width,
                                          int limit) {
  this->push_spaces(1, (limit - width) / 2);
  this->push_str(width, str, PC_ASCII);
  this->push_spaces(1, (limit - width + 1) / 2);
  if (width > 0) {
    this->flushline();
  }
}

void html_feed_environ::close_anchor() {
  if (this->anchor.url.size()) {
    const char *p = nullptr;
    int is_erased = 0;

    auto found = this->find_stack([](auto tag) { return tag->cmd == HTML_A; });

    if (found == this->tag_stack.end() && this->anchor.hseq > 0 &&
        this->line.back() == ' ') {
      Strshrink(this->line, 1);
      this->pos--;
      is_erased = 1;
    }

    if (found != this->tag_stack.end() || (p = this->has_hidden_link(HTML_A))) {
      if (this->anchor.hseq > 0) {
        this->parse(ANSP);
        this->prevchar = " ";
      } else {
        if (found != this->tag_stack.end()) {
          this->tag_stack.erase(found);
        } else {
          this->passthrough(p, 1);
        }
        this->anchor = {};
        return;
      }
      is_erased = 0;
    }
    if (is_erased) {
      this->line.push_back(' ');
      this->pos++;
    }

    this->push_tag("</a>", HTML_N_A);
  }
  this->anchor = {};
}

void html_feed_environ::save_fonteffect() {
  if (this->fontstat_sp < FONT_STACK_SIZE) {
    this->fontstat_stack[this->fontstat_sp] = this->fontstat;
  }
  if (this->fontstat_sp < INT_MAX) {
    this->fontstat_sp++;
  }

  if (this->fontstat.in_bold)
    this->push_tag("</b>", HTML_N_B);
  if (this->fontstat.in_italic)
    this->push_tag("</i>", HTML_N_I);
  if (this->fontstat.in_under)
    this->push_tag("</u>", HTML_N_U);
  if (this->fontstat.in_strike)
    this->push_tag("</s>", HTML_N_S);
  if (this->fontstat.in_ins)
    this->push_tag("</ins>", HTML_N_INS);
  this->fontstat = {};
}

void html_feed_environ::restore_fonteffect() {
  if (this->fontstat_sp > 0)
    this->fontstat_sp--;
  if (this->fontstat_sp < FONT_STACK_SIZE) {
    this->fontstat = this->fontstat_stack[this->fontstat_sp];
  }

  if (this->fontstat.in_bold)
    this->push_tag("<b>", HTML_B);
  if (this->fontstat.in_italic)
    this->push_tag("<i>", HTML_I);
  if (this->fontstat.in_under)
    this->push_tag("<u>", HTML_U);
  if (this->fontstat.in_strike)
    this->push_tag("<s>", HTML_S);
  if (this->fontstat.in_ins)
    this->push_tag("<ins>", HTML_INS);
}

std::string
html_feed_environ::process_textarea(const std::shared_ptr<HtmlTag> &tag,
                                    int width) {
  std::stringstream tmp;

  if (cur_form_id() < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp << process_form(parseHtmlTag(&s, true));
  }

  cur_textarea = "";
  if (auto value = tag->getAttr(ATTR_NAME)) {
    cur_textarea = *value;
  }
  cur_textarea_size = 20;
  if (auto value = tag->getAttr(ATTR_COLS)) {
    cur_textarea_size = stoi(*value);
    if (cur_textarea.size() && cur_textarea.back() == '%')
      cur_textarea_size = width * cur_textarea_size / 100 - 2;
    if (cur_textarea_size <= 0) {
      cur_textarea_size = 20;
    } else if (cur_textarea_size > TEXTAREA_ATTR_COL_MAX) {
      cur_textarea_size = TEXTAREA_ATTR_COL_MAX;
    }
  }
  cur_textarea_rows = 1;
  if (auto value = tag->getAttr(ATTR_ROWS)) {
    cur_textarea_rows = stoi(*value);
    if (cur_textarea_rows <= 0) {
      cur_textarea_rows = 1;
    } else if (cur_textarea_rows > TEXTAREA_ATTR_ROWS_MAX) {
      cur_textarea_rows = TEXTAREA_ATTR_ROWS_MAX;
    }
  }
  cur_textarea_readonly = tag->existsAttr(ATTR_READONLY);
  // if (n_textarea >= max_textarea) {
  //   max_textarea *= 2;
  // textarea_str = (Str **)New_Reuse(Str *, textarea_str, max_textarea);
  // }
  ignore_nl_textarea = true;

  return tmp.str();
}

std::string html_feed_environ::process_n_textarea() {
  if (cur_textarea.empty()) {
    return nullptr;
  }

  std::stringstream tmp;
  tmp << "<pre_int>[<input_alt hseq=\"" << this->cur_hseq() << "\" fid=\""
      << cur_form_id()
      << "\" "
         "type=textarea name=\""
      << html_quote(cur_textarea) << "\" size=" << cur_textarea_size
      << " rows=" << cur_textarea_rows << " "
      << "top_margin=" << (cur_textarea_rows - 1)
      << " textareanumber=" << n_textarea;

  if (cur_textarea_readonly)
    tmp << " readonly";
  tmp << "><u>";
  for (int i = 0; i < cur_textarea_size; i++) {
    tmp << ' ';
  }
  tmp << "</u></input_alt>]</pre_int>\n";
  this->hseqAndIncrement();
  textarea_str.push_back({});
  a_textarea.push_back({});
  n_textarea++;
  cur_textarea = nullptr;

  return tmp.str();
}

void html_feed_environ::feed_textarea(const std::string &_str) {
  if (cur_textarea.empty())
    return;

  auto str = _str.c_str();
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

std::string
html_feed_environ::process_form_int(const std::shared_ptr<HtmlTag> &tag,
                                    int fid) {
  std::string p = "get";
  if (auto value = tag->getAttr(ATTR_METHOD)) {
    p = *value;
  }
  std::string q = "!CURRENT_URL!";
  if (auto value = tag->getAttr(ATTR_ACTION)) {
    q = url_quote(remove_space(*value));
  }
  std::string s = "";
  if (auto value = tag->getAttr(ATTR_ENCTYPE)) {
    s = *value;
  }
  std::string tg = "";
  if (auto value = tag->getAttr(ATTR_TARGET)) {
    tg = *value;
  }
  std::string n = "";
  if (auto value = tag->getAttr(ATTR_NAME)) {
    n = *value;
  }

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
  return {};
}

std::string html_feed_environ::process_n_form(void) {
  if (form_sp >= 0)
    form_sp--;
  return {};
}

static std::string textfieldrep(const std::string &s, int width) {
  Lineprop c_type;
  std::stringstream n;
  int i, j, k, c_len;

  j = 0;
  for (i = 0; i < s.size(); i += c_len) {
    c_type = get_mctype(&s[i]);
    c_len = get_mclen(&s[i]);
    if (s[i] == '\r')
      continue;
    k = j + get_mcwidth(&s[i]);
    if (k > width)
      break;
    if (c_type == PC_CTRL)
      n << ' ';
    else if (s[i] == '&')
      n << "&amp;";
    else if (s[i] == '<')
      n << "&lt;";
    else if (s[i] == '>')
      n << "&gt;";
    else
      n << std::string_view(&s[i], c_len);
    j = k;
  }
  for (; j < width; j++)
    n << ' ';
  return n.str();
}

std::string html_feed_environ::process_img(const std::shared_ptr<HtmlTag> &tag,
                                           int width) {
  int pre_int = false, ext_pre_int = false;

  auto value = tag->getAttr(ATTR_SRC);
  if (!value)
    return {};
  auto _p = url_quote(remove_space(*value));

  std::string _q;
  if (auto value = tag->getAttr(ATTR_ALT)) {
    _q = *value;
  }
  if (!pseudoInlines && (_q.empty() && ignore_null_img_alt)) {
    return {};
  }

  auto t = _q;
  if (auto value = tag->getAttr(ATTR_TITLE)) {
    t = *value;
  }

  int w = -1;
  if (auto value = tag->getAttr(ATTR_WIDTH)) {
    w = stoi(*value);
    if (w < 0) {
      if (width > 0)
        w = (int)(-width * pixel_per_char * w / 100 + 0.5);
      else
        w = -1;
    }
  }

  int i = -1;
  if (auto value = tag->getAttr(ATTR_HEIGHT)) {
    i = stoi(*value);
  }

  std::string r;
  if (auto value = tag->getAttr(ATTR_USEMAP)) {
    r = *value;
  }

  if (tag->existsAttr(ATTR_PRE_INT))
    ext_pre_int = true;

  std::stringstream tmp;
  if (r.size()) {
    auto r2 = strchr(r.c_str(), '#');
    auto s = "<form_int method=internal action=map>";
    tmp << this->process_form(parseHtmlTag(&s, true));
    tmp << "<input_alt fid=\"" << this->cur_form_id() << "\" "
        << "type=hidden name=link value=\"";
    tmp << html_quote((r2) ? r2 + 1 : r);
    tmp << "\"><input_alt hseq=\"" << this->hseqAndIncrement() << "\" fid=\""
        << this->cur_form_id() << "\" "
        << "type=submit no_effect=true>";
  }
  int nw = 0;
  {
    if (w < 0) {
      w = static_cast<int>(12 * pixel_per_char);
    }
    nw = w ? (int)((w - 1) / pixel_per_char + 1) : 1;
    if (r.size()) {
      tmp << "<pre_int>";
      pre_int = true;
    }
    tmp << "<img_alt src=\"";
  }
  tmp << html_quote(_p);
  tmp << "\"";
  if (t.size()) {
    tmp << " title=\"";
    tmp << html_quote(t);
    tmp << "\"";
  }
  tmp << ">";
  int n = 1;
  if (_q.size()) {
    n = get_strwidth(_q);
    tmp << html_quote(_q);
    tmp << "</img_alt>";
    if (pre_int && !ext_pre_int)
      tmp << "</pre_int>";
    if (r.size()) {
      tmp << "</input_alt>";
      this->process_n_form();
    }
    return tmp.str();
  }
  if (w > 0 && i > 0) {
    /* guess what the image is! */
    if (w < 32 && i < 48) {
      /* must be an icon or space */
      n = 1;
      if (strcasestr(_p.c_str(), "space") || strcasestr(_p.c_str(), "blank"))
        tmp << "_";
      else {
        if (w * i < 8 * 16)
          tmp << "*";
        else {
          if (!pre_int) {
            tmp << "<pre_int>";
            pre_int = true;
          }
          push_symbol(tmp, IMG_SYMBOL, 1, 1);
          n = 1;
        }
      }
      if (pre_int && !ext_pre_int)
        tmp << "</pre_int>";
      if (r.size()) {
        tmp << "</input_alt>";
        this->process_n_form();
      }
      return tmp.str();
    }
    if (w > 200 && i < 13) {
      /* must be a horizontal line */
      if (!pre_int) {
        tmp << "<pre_int>";
        pre_int = true;
      }
      w = static_cast<int>(w / pixel_per_char);
      if (w <= 0)
        w = 1;
      push_symbol(tmp, HR_SYMBOL, 1, w);
      n = w;
      tmp << "</img_alt>";
      if (pre_int && !ext_pre_int)
        tmp << "</pre_int>";
      if (r.size()) {
        tmp << "</input_alt>";
        this->process_n_form();
      }
      return tmp.str();
    }
  }
  const char *p = _p.data();
  const char *q;
  for (q = _p.data(); *q; q++)
    ;
  while (q > p && *q != '/')
    q--;
  if (*q == '/')
    q++;
  tmp << '[';
  n = 1;
  p = q;
  for (; *q; q++) {
    if (!IS_ALNUM(*q) && *q != '_' && *q != '-') {
      break;
    }
    tmp << *q;
    n++;
    if (n + 1 >= nw)
      break;
  }
  tmp << ']';
  n++;
  tmp << "</img_alt>";
  if (pre_int && !ext_pre_int)
    tmp << "</pre_int>";
  if (r.size()) {
    tmp << "</input_alt>";
    this->process_n_form();
  }
  return tmp.str();
}

std::string
html_feed_environ::process_anchor(const std::shared_ptr<HtmlTag> &tag,
                                  const std::string &tagbuf) {
  if (tag->needRreconstruct()) {
    std::stringstream ss;
    ss << this->hseqAndIncrement();
    tag->setAttr(ATTR_HSEQ, ss.str());
    return tag->to_str();
  } else {
    std::stringstream tmp;
    tmp << "<a hseq=\"" << this->hseqAndIncrement() << "\"";
    tmp << tagbuf.substr(2);
    return tmp.str();
  }
}

std::string
html_feed_environ::process_input(const std::shared_ptr<HtmlTag> &tag) {
  int v, x, y, z;
  int qlen = 0;

  std::stringstream tmp;
  if (this->cur_form_id() < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp << this->process_form(parseHtmlTag(&s, true));
  }

  std::string p = "text";
  if (auto value = tag->getAttr(ATTR_TYPE)) {
    p = *value;
  }

  std::string q;
  if (auto value = tag->getAttr(ATTR_VALUE)) {
    q = *value;
  }

  std::string r = "";
  if (auto value = tag->getAttr(ATTR_NAME)) {
    r = *value;
  }

  int size = 20;
  if (auto value = tag->getAttr(ATTR_SIZE)) {
    size = stoi(*value);
  }
  if (size > MAX_INPUT_SIZE)
    size = MAX_INPUT_SIZE;

  int i = 20;
  if (auto value = tag->getAttr(ATTR_MAXLENGTH)) {
    i = stoi(*value);
  }

  std::string p2;
  if (auto value = tag->getAttr(ATTR_ALT)) {
    p2 = *value;
  }

  x = tag->existsAttr(ATTR_CHECKED);
  y = tag->existsAttr(ATTR_ACCEPT);
  z = tag->existsAttr(ATTR_READONLY);

  v = formtype(p);
  if (v == FORM_UNKNOWN)
    return nullptr;

  if (q.empty()) {
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

  std::string qq = "";
  if (q.size()) {
    qq = html_quote(q);
    qlen = get_strwidth(q);
  }

  tmp << "<pre_int>";
  switch (v) {
  case FORM_INPUT_PASSWORD:
  case FORM_INPUT_TEXT:
  case FORM_INPUT_FILE:
  case FORM_INPUT_CHECKBOX:
    if (displayLinkNumber)
      tmp << this->getLinkNumberStr(0);
    tmp << '[';
    break;
  case FORM_INPUT_RADIO:
    if (displayLinkNumber)
      tmp << this->getLinkNumberStr(0);
    tmp << '(';
  }
  tmp << "<input_alt hseq=\"" << this->hseqAndIncrement() << "\" fid=\""
      << this->cur_form_id() << "\" type=\"" << html_quote(p) << "\" "
      << "name=\"" << html_quote(r) << "\" width=" << size << " maxlength=" << i
      << " value=\"" << qq << "\"";
  if (x)
    tmp << " checked";
  if (y)
    tmp << " accept";
  if (z)
    tmp << " readonly";
  tmp << '>';

  if (v == FORM_INPUT_HIDDEN)
    tmp << "</input_alt></pre_int>";
  else {
    switch (v) {
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
      tmp << "<u>";
      break;
    case FORM_INPUT_IMAGE: {
      std::string s;
      if (auto value = tag->getAttr(ATTR_SRC)) {
        s = *value;
      }
      if (s.size()) {
        tmp << "<img src=\"" << html_quote(s) << "\"";
        if (p2.size())
          tmp << " alt=\"" << html_quote(p2) << "\"";
        if (auto value = tag->getAttr(ATTR_WIDTH))
          tmp << " width=\"" << stoi(*value) << "\"";
        if (auto value = tag->getAttr(ATTR_HEIGHT))
          tmp << " height=\"" << stoi(*value) << "\"";
        tmp << " pre_int>";
        tmp << "</input_alt></pre_int>";
        return tmp.str();
      }
    }
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
    case FORM_INPUT_RESET:
      if (displayLinkNumber)
        tmp << this->getLinkNumberStr(-1);
      tmp << "[";
      break;
    }
    switch (v) {
    case FORM_INPUT_PASSWORD:
      i = 0;
      if (q.size()) {
        for (; i < qlen && i < size; i++)
          tmp << '*';
      }
      for (; i < size; i++)
        tmp << ' ';
      break;
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
      if (q.size())
        tmp << textfieldrep(q, size);
      else {
        for (i = 0; i < size; i++)
          tmp << ' ';
      }
      break;
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
      if (p2.size())
        tmp << html_quote(p2);
      else
        tmp << qq;
      break;
    case FORM_INPUT_RESET:
      tmp << qq;
      break;
    case FORM_INPUT_RADIO:
    case FORM_INPUT_CHECKBOX:
      if (x)
        tmp << '*';
      else
        tmp << ' ';
      break;
    }
    switch (v) {
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
      tmp << "</u>";
      break;
    case FORM_INPUT_IMAGE:
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
    case FORM_INPUT_RESET:
      tmp << "]";
    }
    tmp << "</input_alt>";
    switch (v) {
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
    case FORM_INPUT_CHECKBOX:
      tmp << ']';
      break;
    case FORM_INPUT_RADIO:
      tmp << ')';
    }
    tmp << "</pre_int>";
  }
  return tmp.str();
}

std::string
html_feed_environ::process_button(const std::shared_ptr<HtmlTag> &tag) {
  int v;

  std::stringstream tmp;
  if (this->cur_form_id() < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp << this->process_form(parseHtmlTag(&s, true));
  }

  std::string p = "submit";
  if (auto value = tag->getAttr(ATTR_TYPE)) {
    p = *value;
  }

  std::string q;
  if (auto value = tag->getAttr(ATTR_VALUE)) {
    q = *value;
  }

  std::string r = "";
  if (auto value = tag->getAttr(ATTR_NAME)) {
    r = *value;
  }

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

  if (q.empty()) {
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

  std::string qq = "";
  if (q.size()) {
    qq = html_quote(q);
  }

  /*    Strcat_charp(tmp, "<pre_int>"); */
  tmp << "<input_alt hseq=\"" << this->hseqAndIncrement() << "\" fid=\""
      << this->cur_form_id() << "\" type=\"" << html_quote(p) << "\" "
      << "name=\"" << html_quote(r) << "\" value=\"" << qq << "\">";
  return tmp.str();
}

std::string html_feed_environ::process_hr(const std::shared_ptr<HtmlTag> &tag,
                                          int width, int indent_width) {
  std::stringstream tmp;
  tmp << "<nobr>";

  if (width > indent_width)
    width -= indent_width;

  int w = 0;
  if (auto value = tag->getAttr(ATTR_WIDTH)) {
    w = stoi(*value);
    if (w > HR_ATTR_WIDTH_MAX) {
      w = HR_ATTR_WIDTH_MAX;
    }
    w = REAL_WIDTH(w, width);
  } else {
    w = width;
  }

  int x = ALIGN_CENTER;
  if (auto value = tag->getAttr(ATTR_ALIGN)) {
    x = stoi(*value);
  }
  switch (x) {
  case ALIGN_CENTER:
    tmp << "<div_int align=center>";
    break;
  case ALIGN_RIGHT:
    tmp << "<div_int align=right>";
    break;
  case ALIGN_LEFT:
    tmp << "<div_int align=left>";
    break;
  }
  if (w <= 0)
    w = 1;
  push_symbol(tmp, HR_SYMBOL, 1, w);
  tmp << "</div_int></nobr>";
  return tmp.str();
}

void html_feed_environ::proc_escape(const char **str_return) {
  const char *str = *str_return;
  char32_t ech = getescapechar(str_return);
  int n_add = *str_return - str;
  Lineprop mode = PC_ASCII;

  if (ech == -1) {
    *str_return = str;
    this->proc_mchar(this->flag & RB_SPECIAL, 1, str_return, PC_ASCII);
    return;
  }
  mode = IS_CNTRL(ech) ? PC_CTRL : PC_ASCII;

  auto estr = conv_entity(ech);
  this->check_breakpoint(this->flag & RB_SPECIAL, estr.c_str());
  auto width = get_strwidth(estr.c_str());
  if (width == 1 && ech == (char32_t)estr[0] && ech != '&' && ech != '<' &&
      ech != '>') {
    if (IS_CNTRL(ech))
      mode = PC_CTRL;
    this->push_charp(width, estr.c_str(), mode);
  } else {
    this->push_nchars(width, str, n_add, mode);
  }
  this->prevchar = estr;
  this->prev_ctype = mode;
}

int html_feed_environ::table_width(int table_level) {
  if (table_level < 0)
    return 0;

  int width = this->tables[table_level]->total_width();
  if (table_level > 0 || width > 0)
    return width;

  return this->_width - this->envs[this->envc].indent;
}

static void clear_ignore_p_flag(html_feed_environ *h_env, int cmd) {
  static int clear_flag_cmd[] = {HTML_HR, HTML_UNKNOWN};
  int i;

  for (i = 0; clear_flag_cmd[i] != HTML_UNKNOWN; i++) {
    if (cmd == clear_flag_cmd[i]) {
      h_env->flag &= ~RB_IGNORE_P;
      return;
    }
  }
}

static int need_flushline(html_feed_environ *h_env, Lineprop mode) {
  if (h_env->flag & RB_PRE_INT) {
    if (h_env->pos > h_env->width())
      return 1;
    else
      return 0;
  }

  /* if (ch == ' ' && h_env->tag_sp > 0) */
  if (h_env->line.size() && h_env->line.back() == ' ') {
    return 0;
  }

  if (h_env->pos > h_env->width())
    return 1;

  return 0;
}

void html_feed_environ::parse(std::string_view html, bool internal) {

  Tokenizer tokenizer(html);
  TableStatus t;
  while (auto token = tokenizer.getToken(this, t, internal)) {
    process_token(t, *token);
  }

  this->parse_end();
}

void html_feed_environ::process_token(TableStatus &t, const Token &token) {
  if (t.is_active(this)) {
    /*
     * within table: in <table>..</table>, all input tokens
     * are fed to the table renderer, and then the renderer
     * makes HTML output.
     */
    switch (t.feed(this, token.str, internal)) {
    case 0:
      /* </table> tag */
      this->table_level--;
      if (this->table_level >= MAX_TABLE - 1)
        return;
      t.tbl->end_table();
      if (this->table_level >= 0) {
        auto tbl0 = tables[this->table_level];
        std::stringstream ss;
        ss << "<table_alt tid=" << tbl0->ntable() << ">";
        if (tbl0->row() < 0)
          return;
        tbl0->pushTable(t.tbl);
        t.tbl = tbl0;
        t.tbl_mode = &table_mode[this->table_level];
        t.tbl_width = table_width(this->table_level);
        t.feed(this, ss.str(), true);
        return;
        /* continue to the next */
      }
      if (this->flag & RB_DEL)
        return;
      /* all tables have been read */
      if (t.tbl->vspace() > 0 && !(this->flag & RB_IGNORE_P)) {
        this->flushline();
        this->do_blankline();
      }
      this->save_fonteffect();
      initRenderTable();
      t.tbl->renderTable(this, t.tbl_width);
      this->restore_fonteffect();
      this->flag &= ~RB_IGNORE_P;
      if (t.tbl->vspace() > 0) {
        this->do_blankline();
        this->flag |= RB_IGNORE_P;
      }
      this->prevchar = " ";
      return;
    case 1:
      /* <table> tag */
      break;
    default:
      return;
    }
  }

  if (token.is_tag) {
    /*** Beginning of a new tag ***/
    std::shared_ptr<HtmlTag> tag = parseHtmlTag(token.str, internal);
    if (!tag) {
      return;
    }

    // process tags
    if (process(tag) == 0) {
      // preserve the tag for second-stage processing
      if (tag->needRreconstruct()) {
        this->setToken(tag->to_str());
      }
      this->push_tag(this->tagbuf(), tag->tagid);
    }
    this->bp.init_flag = 1;
    clear_ignore_p_flag(this, tag->tagid);
    if (tag->tagid == HTML_TABLE) {
      if (this->table_level >= 0) {
        int level = std::min<short>(this->table_level, MAX_TABLE - 1);
        t.tbl = tables[level];
        t.tbl_mode = &table_mode[level];
        t.tbl_width = this->table_width(level);
      }
    }
    return;
  }

  if (this->flag & (RB_DEL | RB_S)) {
    return;
  }

  auto pp = token.str.c_str();
  while (*pp) {
    auto mode = get_mctype(pp);
    int delta = get_mcwidth(pp);
    if (this->flag & (RB_SPECIAL & ~RB_NOBR)) {
      char ch = *pp;
      if (!(this->flag & RB_PLAIN) && (*pp == '&')) {
        const char *p = pp;
        int ech = getescapechar(&p);
        if (ech == '\n' || ech == '\r') {
          ch = '\n';
          pp = p - 1;
        } else if (ech == '\t') {
          ch = '\t';
          pp = p - 1;
        }
      }
      if (ch != '\n')
        this->flag &= ~RB_IGNORE_P;
      if (ch == '\n') {
        pp++;
        if (this->flag & RB_IGNORE_P) {
          this->flag &= ~RB_IGNORE_P;
          return;
        }
        if (this->flag & RB_PRE_INT) {
          this->push_char(this->flag & RB_SPECIAL, ' ');
        } else {
          this->flushline(FlushLineMode::Force);
        }
      } else if (ch == '\t') {
        do {
          this->push_char(this->flag & RB_SPECIAL, ' ');
        } while ((this->envs[this->envc].indent + this->pos) % Tabstop != 0);
        pp++;
      } else if (this->flag & RB_PLAIN) {
        auto p = html_quote_char(*pp);
        if (p) {
          this->push_charp(1, p, PC_ASCII);
          pp++;
        } else {
          this->proc_mchar(1, delta, &pp, mode);
        }
      } else {
        if (*pp == '&')
          this->proc_escape(&pp);
        else
          this->proc_mchar(1, delta, &pp, mode);
      }
      if (this->flag & (RB_SPECIAL & ~RB_PRE_INT))
        return;
    } else {
      if (!IS_SPACE(*pp))
        this->flag &= ~RB_IGNORE_P;
      if ((mode == PC_ASCII || mode == PC_CTRL) && IS_SPACE(*pp)) {
        if (this->prevchar[0] != ' ') {
          this->push_char(this->flag & RB_SPECIAL, ' ');
        }
        pp++;
      } else {
        if (*pp == '&')
          this->proc_escape(&pp);
        else
          this->proc_mchar(this->flag & RB_SPECIAL, delta, &pp, mode);
      }
    }
    if (need_flushline(this, mode)) {
      auto bp = this->line.c_str() + this->bp.len;
      auto tp = bp - this->bp.tlen;
      int i = 0;

      if (tp > this->line.c_str() && tp[-1] == ' ')
        i = 1;

      int indent = this->envs[this->envc].indent;
      if (this->bp.pos - i > indent) {
        this->append_tags(); /* may reallocate the buffer */
        bp = this->line.c_str() + this->bp.len;
        std::string line = bp;
        Strshrink(this->line, this->line.size() - this->bp.len);
        if (this->pos - i > this->_width)
          this->flag |= RB_FILL;
        this->back_to_breakpoint();
        this->flushline();
        this->flag &= ~RB_FILL;
        parse(line);
      }
    }
  }
}

std::string html_feed_environ::process_n_button() { return "</input_alt>"; }

std::string
html_feed_environ::process_select(const std::shared_ptr<HtmlTag> &tag) {
  std::stringstream tmp;

  if (cur_form_id() < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp << process_form(parseHtmlTag(&s, true));
  }

  std::string p = "";
  if (auto value = tag->getAttr(ATTR_NAME)) {
    p = *value;
  }
  cur_select = p;
  select_is_multiple = tag->existsAttr(ATTR_MULTIPLE);

  select_str = {};
  cur_option = nullptr;
  cur_status = R_ST_NORMAL;
  n_selectitem = 0;
  return tmp.str();
}

std::string html_feed_environ::process_n_select() {
  if (cur_select.empty())
    return {};
  this->process_option();
  select_str += "<br>";
  cur_select = {};
  n_selectitem = 0;
  return select_str;
}

void html_feed_environ::feed_select(const std::string &_str) {
  int prev_status = cur_status;
  static int prev_spaces = -1;

  if (cur_select.empty())
    return;
  auto str = _str.c_str();
  while (auto tmp = read_token(&str, &cur_status, 0)) {
    if (cur_status != R_ST_NORMAL || prev_status != R_ST_NORMAL)
      continue;
    auto p = tmp->c_str();
    if ((*tmp)[0] == '<' && tmp->back() == '>') {
      std::shared_ptr<HtmlTag> tag;

      if (!(tag = parseHtmlTag(&p, false)))
        continue;
      switch (tag->tagid) {
      case HTML_OPTION:
        this->process_option();
        cur_option = {};
        if (auto value = tag->getAttr(ATTR_VALUE))
          cur_option_value = *value;
        else
          cur_option_value = {};
        if (auto value = tag->getAttr(ATTR_LABEL))
          cur_option_label = *value;
        else
          cur_option_label = {};
        cur_option_selected = tag->existsAttr(ATTR_SELECTED);
        prev_spaces = -1;
        break;
      case HTML_N_OPTION:
        /* do nothing */
        break;
      default:
        /* never happen */
        break;
      }
    } else if (cur_option.size()) {
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
            cur_option += getescapecmd(&p);
          else
            cur_option += *(p++);
        }
      }
    }
  }
}

void html_feed_environ::process_option() {
  char begin_char = '[', end_char = ']';

  if (cur_select.empty() || cur_option.empty())
    return;
  while (cur_option.size() && IS_SPACE(cur_option.back()))
    Strshrink(cur_option, 1);
  if (cur_option_value.empty())
    cur_option_value = cur_option;
  if (cur_option_label.empty())
    cur_option_label = cur_option;
  if (!select_is_multiple) {
    begin_char = '(';
    end_char = ')';
  }
  std::stringstream ss;
  ss << "<br><pre_int>" << begin_char << "<input_alt hseq=\""
     << this->hseqAndIncrement()
     << "\" "
        "fid=\""
     << cur_form_id()
     << "\" type=" << (select_is_multiple ? "checkbox" : "radio") << " name=\""
     << html_quote(cur_select) << "\" value=\"" << html_quote(cur_option_value)
     << "\"";

  if (cur_option_selected)
    ss << " checked>*</input_alt>";
  else
    ss << "> </input_alt>";
  ss << end_char;
  ss << html_quote(cur_option_label);
  ss << "</pre_int>";

  select_str += ss.str();
  n_selectitem++;
}

void html_feed_environ::completeHTMLstream() {
  this->close_anchor();
  if (this->img_alt.size()) {
    this->push_tag("</img_alt>", HTML_N_IMG_ALT);
    this->img_alt = nullptr;
  }
  if (this->input_alt.in) {
    this->push_tag("</input_alt>", HTML_N_INPUT_ALT);
    this->input_alt.hseq = 0;
    this->input_alt.fid = -1;
    this->input_alt.in = 0;
    this->input_alt.type = nullptr;
    this->input_alt.name = nullptr;
    this->input_alt.value = nullptr;
  }
  if (this->fontstat.in_bold) {
    this->push_tag("</b>", HTML_N_B);
    this->fontstat.in_bold = 0;
  }
  if (this->fontstat.in_italic) {
    this->push_tag("</i>", HTML_N_I);
    this->fontstat.in_italic = 0;
  }
  if (this->fontstat.in_under) {
    this->push_tag("</u>", HTML_N_U);
    this->fontstat.in_under = 0;
  }
  if (this->fontstat.in_strike) {
    this->push_tag("</s>", HTML_N_S);
    this->fontstat.in_strike = 0;
  }
  if (this->fontstat.in_ins) {
    this->push_tag("</ins>", HTML_N_INS);
    this->fontstat.in_ins = 0;
  }
  if (this->flag & RB_INTXTA)
    this->parse("</textarea>");
  /* for unbalanced select tag */
  if (this->flag & RB_INSELECT)
    this->parse("</select>");
  if (this->flag & RB_TITLE)
    this->parse("</title>");

  /* for unbalanced table tag */
  if (this->table_level >= MAX_TABLE)
    this->table_level = MAX_TABLE - 1;

  while (this->table_level >= 0) {
    int tmp = this->table_level;
    table_mode[this->table_level].pre_mode &=
        ~(TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN);
    this->parse("</table>");
    if (this->table_level >= tmp)
      break;
  }

  flushline(FlushLineMode::Append);
}

int html_feed_environ::append_token(const char **instr, bool pre) {
  if (**instr == '\0')
    return 0;

  ReadBufferStatus prev_status;
  const char *p;
  for (p = *instr; *p; p++) {
    /* Drop Unicode soft hyphen */
    if (*(unsigned char *)p == 0210 && *(unsigned char *)(p + 1) == 0200 &&
        *(unsigned char *)(p + 2) == 0201 &&
        *(unsigned char *)(p + 3) == 0255) {
      p += 3;
      continue;
    }

    prev_status = this->status;
    next_status(*p, &this->status);
    switch (this->status) {
    case R_ST_NORMAL:
      if (prev_status == R_ST_AMP && *p != ';') {
        p--;
        break;
      }
      if (prev_status == R_ST_NCMNT2 || prev_status == R_ST_NCMNT3 ||
          prev_status == R_ST_IRRTAG || prev_status == R_ST_CMNT1) {
        if (pre) {
          _tagbuf.push_back(*p);
        }
        p++;

        *instr = p;
        return 1;
      }
      _tagbuf.push_back((!pre && IS_SPACE(*p)) ? ' ' : *p);
      if (ST_IS_REAL_TAG(prev_status)) {
        *instr = p + 1;
        if (_tagbuf.size() < 2 || _tagbuf[_tagbuf.size() - 2] != '<' ||
            _tagbuf.back() != '>')
          return 1;
        Strshrink(_tagbuf, 2);
      }
      break;
    case R_ST_TAG0:
    case R_ST_TAG:
      if (prev_status == R_ST_NORMAL && p != *instr) {
        *instr = p;
        this->status = prev_status;
        return 1;
      }
      if (this->status == R_ST_TAG0 && !REALLY_THE_BEGINNING_OF_A_TAG(p)) {
        /* it seems that this '<' is not a beginning of a tag */
        /*
         * Strcat_charp(buf, "&lt;");
         */
        _tagbuf.push_back('<');
        this->status = R_ST_NORMAL;
      } else {
        _tagbuf.push_back(*p);
      }
      break;
    case R_ST_EQL:
    case R_ST_QUOTE:
    case R_ST_DQUOTE:
    case R_ST_VALUE:
    case R_ST_AMP:
      _tagbuf.push_back(*p);
      break;
    case R_ST_CMNT:
    case R_ST_IRRTAG:
      if (pre)
        _tagbuf.push_back(*p);
      break;
    case R_ST_CMNT1:
    case R_ST_CMNT2:
    case R_ST_NCMNT1:
    case R_ST_NCMNT2:
    case R_ST_NCMNT3:
      /* do nothing */
      if (pre)
        _tagbuf.push_back(*p);
      break;

    case R_ST_EOL:
      break;
    }
  }

  *instr = p;
  return 1;
}

int html_feed_environ::process(const std::shared_ptr<HtmlTag> &tag) {
  if (this->flag & RB_PRE) {
    switch (tag->tagid) {
    case HTML_NOBR:
    case HTML_N_NOBR:
    case HTML_PRE_INT:
    case HTML_N_PRE_INT:
      return 1;

    default:
      break;
    }
  }

  switch (tag->tagid) {
  case HTML_B:
    return this->HTML_B_enter(tag);
  case HTML_N_B:
    return this->HTML_B_exit(tag);
  case HTML_I:
    return this->HTML_I_enter(tag);
  case HTML_N_I:
    return this->HTML_I_exit(tag);
  case HTML_U:
    return this->HTML_U_enter(tag);
  case HTML_N_U:
    return this->HTML_U_exit(tag);
  case HTML_EM:
    this->parse("<i>");
    return 1;
  case HTML_N_EM:
    this->parse("</i>");
    return 1;
  case HTML_STRONG:
    this->parse("<b>");
    return 1;
  case HTML_N_STRONG:
    this->parse("</b>");
    return 1;
  case HTML_Q:
    this->parse("`");
    return 1;
  case HTML_N_Q:
    this->parse("'");
    return 1;
  case HTML_FIGURE:
  case HTML_N_FIGURE:
  case HTML_P:
  case HTML_N_P:
    return this->HTML_Paragraph(tag);
  case HTML_FIGCAPTION:
  case HTML_N_FIGCAPTION:
  case HTML_BR:
    this->flushline(FlushLineMode::Force);
    this->clearBlankLines();
    return 1;
  case HTML_H:
    return this->HTML_H_enter(tag);
  case HTML_N_H:
    return this->HTML_H_exit(tag);
  case HTML_UL:
  case HTML_OL:
  case HTML_BLQ:
    return this->HTML_List_enter(tag);
  case HTML_N_UL:
  case HTML_N_OL:
  case HTML_N_DL:
  case HTML_N_BLQ:
  case HTML_N_DD:
    return this->HTML_List_exit(tag);
  case HTML_DL:
    return this->HTML_DL_enter(tag);
  case HTML_LI:
    return this->HTML_LI_enter(tag);
  case HTML_DT:
    return this->HTML_DT_enter(tag);
  case HTML_N_DT:
    return this->HTML_DT_exit(tag);
  case HTML_DD:
    return this->HTML_DD_enter(tag);
  case HTML_TITLE:
    return this->HTML_TITLE_enter(tag);
  case HTML_N_TITLE:
    return this->HTML_TITLE_exit(tag);
  case HTML_TITLE_ALT:
    return this->HTML_TITLE_ALT_enter(tag);
  case HTML_FRAMESET:
    return this->HTML_FRAMESET_enter(tag);
  case HTML_N_FRAMESET:
    return this->HTML_FRAMESET_exit(tag);
  case HTML_NOFRAMES:
    return this->HTML_NOFRAMES_enter(tag);
  case HTML_N_NOFRAMES:
    return this->HTML_NOFRAMES_exit(tag);
  case HTML_FRAME:
    return this->HTML_FRAME_enter(tag);
  case HTML_HR:
    return this->HTML_HR_enter(tag);
  case HTML_PRE:
    return this->HTML_PRE_enter(tag);
  case HTML_N_PRE:
    return this->HTML_PRE_exit(tag);
  case HTML_PRE_INT:
    return this->HTML_PRE_INT_enter(tag);
  case HTML_N_PRE_INT:
    return this->HTML_PRE_INT_exit(tag);
  case HTML_NOBR:
    return this->HTML_NOBR_enter(tag);
  case HTML_N_NOBR:
    return this->HTML_NOBR_exit(tag);
  case HTML_PRE_PLAIN:
    return this->HTML_PRE_PLAIN_enter(tag);
  case HTML_N_PRE_PLAIN:
    return this->HTML_PRE_PLAIN_exit(tag);
  case HTML_LISTING:
  case HTML_XMP:
  case HTML_PLAINTEXT:
    return this->HTML_PLAINTEXT_enter(tag);
  case HTML_N_LISTING:
  case HTML_N_XMP:
    return this->HTML_LISTING_exit(tag);
  case HTML_SCRIPT:
    this->flag |= RB_SCRIPT;
    this->end_tag = HTML_N_SCRIPT;
    return 1;
  case HTML_STYLE:
    this->flag |= RB_STYLE;
    this->end_tag = HTML_N_STYLE;
    return 1;
  case HTML_N_SCRIPT:
    this->flag &= ~RB_SCRIPT;
    this->end_tag = HTML_UNKNOWN;
    return 1;
  case HTML_N_STYLE:
    this->flag &= ~RB_STYLE;
    this->end_tag = HTML_UNKNOWN;
    return 1;
  case HTML_A:
    return this->HTML_A_enter(tag);
  case HTML_N_A:
    this->close_anchor();
    return 1;
  case HTML_IMG:
    return this->HTML_IMG_enter(tag);
  case HTML_IMG_ALT:
    return this->HTML_IMG_ALT_enter(tag);
  case HTML_N_IMG_ALT:
    return this->HTML_IMG_ALT_exit(tag);
  case HTML_INPUT_ALT:
    return this->HTML_INPUT_ALT_enter(tag);
  case HTML_N_INPUT_ALT:
    return this->HTML_INPUT_ALT_exit(tag);
  case HTML_TABLE:
    return this->HTML_TABLE_enter(tag);
  case HTML_N_TABLE:
    // should be processed in HTMLlineproc()
    return 1;
  case HTML_CENTER:
    return this->HTML_CENTER_enter(tag);
  case HTML_N_CENTER:
    return this->HTML_CENTER_exit(tag);
  case HTML_DIV:
    return this->HTML_DIV_enter(tag);
  case HTML_N_DIV:
    return this->HTML_DIV_exit(tag);
  case HTML_DIV_INT:
    return this->HTML_DIV_INT_enter(tag);
  case HTML_N_DIV_INT:
    return this->HTML_DIV_INT_exit(tag);
  case HTML_FORM:
    return this->HTML_FORM_enter(tag);
  case HTML_N_FORM:
    return this->HTML_FORM_exit(tag);
  case HTML_INPUT:
    return this->HTML_INPUT_enter(tag);
  case HTML_BUTTON:
    return this->HTML_BUTTON_enter(tag);
  case HTML_N_BUTTON:
    return this->HTML_BUTTON_exit(tag);
  case HTML_SELECT:
    return this->HTML_SELECT_enter(tag);
  case HTML_N_SELECT:
    return this->HTML_SELECT_exit(tag);
  case HTML_OPTION:
    /* nothing */
    return 1;
  case HTML_TEXTAREA:
    return this->HTML_TEXTAREA_enter(tag);
  case HTML_N_TEXTAREA:
    return this->HTML_TEXTAREA_exit(tag);
  case HTML_ISINDEX:
    return this->HTML_ISINDEX_enter(tag);
  case HTML_DOCTYPE:
    if (!tag->existsAttr(ATTR_PUBLIC)) {
      this->flag |= RB_HTML5;
    }
    return 1;
  case HTML_META:
    return this->HTML_META_enter(tag);
  case HTML_BASE:
  case HTML_MAP:
  case HTML_N_MAP:
  case HTML_AREA:
    return 0;
  case HTML_DEL:
    return this->HTML_DEL_enter(tag);
  case HTML_N_DEL:
    return this->HTML_DEL_exit(tag);
  case HTML_S:
    return this->HTML_S_enter(tag);
  case HTML_N_S:
    return this->HTML_S_exit(tag);
  case HTML_INS:
    return this->HTML_INS_enter(tag);
  case HTML_N_INS:
    return this->HTML_INS_exit(tag);
  case HTML_SUP:
    if (!(this->flag & (RB_DEL | RB_S)))
      this->parse("^");
    return 1;
  case HTML_N_SUP:
    return 1;
  case HTML_SUB:
    if (!(this->flag & (RB_DEL | RB_S)))
      this->parse("[");
    return 1;
  case HTML_N_SUB:
    if (!(this->flag & (RB_DEL | RB_S)))
      this->parse("]");
    return 1;
  case HTML_FONT:
  case HTML_N_FONT:
  case HTML_NOP:
    return 1;
  case HTML_BGSOUND:
    return this->HTML_BGSOUND_enter(tag);
  case HTML_EMBED:
    return this->HTML_EMBED_enter(tag);
  case HTML_APPLET:
    return this->HTML_APPLET_enter(tag);
  case HTML_BODY:
    return this->HTML_BODY_enter(tag);
  case HTML_N_HEAD:
    if (this->flag & RB_TITLE)
      this->parse("</title>");
    return 1;
  case HTML_HEAD:
  case HTML_N_BODY:
    return 1;
  default:
    /* this->prevchar = '\0'; */
    return 0;
  }

  return 0;
}

int html_feed_environ::HTML_Paragraph(const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_A();
  if (!(this->flag & RB_IGNORE_P)) {
    this->flushline(FlushLineMode::Force);
    this->do_blankline();
  }
  this->flag |= RB_IGNORE_P;
  if (tag->tagid == HTML_P) {
    this->set_alignment(tag->alignFlag());
    this->flag |= RB_P;
  }
  return 1;
}

int html_feed_environ::HTML_H_enter(const std::shared_ptr<HtmlTag> &tag) {
  if (!(this->flag & (RB_PREMODE | RB_IGNORE_P))) {
    this->flushline();
    this->do_blankline();
  }
  this->parse("<b>");
  this->set_alignment(tag->alignFlag());
  return 1;
}

int html_feed_environ::HTML_H_exit(const std::shared_ptr<HtmlTag> &tag) {
  this->parse("</b>");
  if (!(this->flag & RB_PREMODE)) {
    this->flushline();
  }
  this->do_blankline();
  this->RB_RESTORE_FLAG();
  this->close_anchor();
  this->flag |= RB_IGNORE_P;
  return 1;
}

int html_feed_environ::HTML_List_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_A();
  if (!(this->flag & RB_IGNORE_P)) {
    this->flushline();
    if (!(this->flag & RB_PREMODE) &&
        (this->envc == 0 || tag->tagid == HTML_BLQ)) {
      this->do_blankline();
    }
  }
  this->PUSH_ENV(tag->tagid);
  if (tag->tagid == HTML_UL || tag->tagid == HTML_OL) {
    if (auto value = tag->getAttr(ATTR_START)) {
      this->envs[this->envc].count = stoi(*value) - 1;
    }
  }
  if (tag->tagid == HTML_OL) {
    this->envs[this->envc].type = '1';
    if (auto value = tag->getAttr(ATTR_TYPE)) {
      this->envs[this->envc].type = (*value)[0];
    }
  }
  if (tag->tagid == HTML_UL) {
    // this->envs[this->envc].type = tag->ul_type(0);
    //  int HtmlTag::ul_type(int default_type) const {
    //    if (auto value = tag->getAttr(ATTR_TYPE)) {
    //      if (!strcasecmp(*value, "disc"))
    //        return (int)'d';
    //      else if (!strcasecmp(*value, "circle"))
    //        return (int)'c';
    //      else if (!strcasecmp(*value, "square"))
    //        return (int)'s';
    //    }
    //    return default_type;
    //  }
  }
  this->flushline();
  return 1;
}

int html_feed_environ::HTML_List_exit(const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_DT();
  this->CLOSE_A();
  if (this->envc > 0) {
    this->flushline();
    this->POP_ENV();
    if (!(this->flag & RB_PREMODE) &&
        (this->envc == 0 || tag->tagid == HTML_N_BLQ)) {
      this->do_blankline();
      this->flag |= RB_IGNORE_P;
    }
  }
  this->close_anchor();
  return 1;
}

int html_feed_environ::HTML_DL_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_A();
  if (!(this->flag & RB_IGNORE_P)) {
    this->flushline();
    if (!(this->flag & RB_PREMODE) && this->envs[this->envc].env != HTML_DL &&
        this->envs[this->envc].env != HTML_DL_COMPACT &&
        this->envs[this->envc].env != HTML_DD) {
      this->do_blankline();
    }
  }
  this->PUSH_ENV_NOINDENT(tag->tagid);
  if (tag->existsAttr(ATTR_COMPACT)) {
    this->envs[this->envc].env = HTML_DL_COMPACT;
  }
  this->flag |= RB_IGNORE_P;
  return 1;
}

const int N_GRAPH_SYMBOL = 32;
#define UL_SYMBOL(x) (N_GRAPH_SYMBOL + (x))
enum UlSymbolType {
  UL_SYMBOL_DISC = 32 + 9,
  UL_SYMBOL_CIRCLE = 32 + 10,
  UL_SYMBOL_SQUARE = 32 + 11,
};

int html_feed_environ::HTML_LI_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_A();
  this->CLOSE_DT();
  if (this->envc > 0) {
    this->flushline();
    this->envs[this->envc].count++;
    if (auto value = tag->getAttr(ATTR_VALUE)) {
      int count = stoi(*value);
      if (count > 0)
        this->envs[this->envc].count = count;
      else
        this->envs[this->envc].count = 0;
    }

    std::string num;
    switch (this->envs[this->envc].env) {
    case HTML_UL: {
      // this->envs[this->envc].type =
      // this->ul_type(this->envs[this->envc].type);
      //  int HtmlTag::ul_type(int default_type) const {
      //    if (auto value = tag->getAttr(ATTR_TYPE)) {
      //      if (!strcasecmp(*value, "disc"))
      //        return (int)'d';
      //      else if (!strcasecmp(*value, "circle"))
      //        return (int)'c';
      //      else if (!strcasecmp(*value, "square"))
      //        return (int)'s';
      //    }
      //    return default_type;
      //  }
      for (int i = 0; i < INDENT_INCR - 3; i++)
        this->push_charp(1, NBSP, PC_ASCII);
      std::stringstream tmp;
      switch (this->envs[this->envc].type) {
      case 'd':
        push_symbol(tmp, UL_SYMBOL_DISC, 1, 1);
        break;
      case 'c':
        push_symbol(tmp, UL_SYMBOL_CIRCLE, 1, 1);
        break;
      case 's':
        push_symbol(tmp, UL_SYMBOL_SQUARE, 1, 1);
        break;
      default:
        push_symbol(tmp, UL_SYMBOL((this->envc_real - 1) % MAX_UL_LEVEL), 1, 1);
        break;
      }
      this->push_charp(1, NBSP, PC_ASCII);
      this->push_str(1, tmp.str(), PC_ASCII);
      this->push_charp(1, NBSP, PC_ASCII);
      this->prevchar = " ";
      break;
    }

    case HTML_OL:
      if (auto value = tag->getAttr(ATTR_TYPE))
        this->envs[this->envc].type = (*value)[0];
      switch ((this->envs[this->envc].count > 0) ? this->envs[this->envc].type
                                                 : '1') {
      case 'i':
        num = romanNumeral(this->envs[this->envc].count);
        break;
      case 'I':
        num = romanNumeral(this->envs[this->envc].count);
        Strupper(num);
        break;
      case 'a':
        num = romanAlphabet(this->envs[this->envc].count);
        break;
      case 'A':
        num = romanAlphabet(this->envs[this->envc].count);
        Strupper(num);
        break;
      default: {
        std::stringstream ss;
        ss << this->envs[this->envc].count;
        num = ss.str();
        break;
      }
      }
      if (INDENT_INCR >= 4)
        num += ". ";
      else
        num.push_back('.');
      this->push_spaces(1, INDENT_INCR - num.size());
      this->push_str(num.size(), num, PC_ASCII);
      if (INDENT_INCR >= 4)
        this->prevchar = " ";
      break;
    default:
      this->push_spaces(1, INDENT_INCR);
      break;
    }
  } else {
    this->flushline();
  }
  this->flag |= RB_IGNORE_P;
  return 1;
}

int html_feed_environ::HTML_DT_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_A();
  if (this->envc == 0 || (this->envc_real < (int)this->envs.size() &&
                          this->envs[this->envc].env != HTML_DL &&
                          this->envs[this->envc].env != HTML_DL_COMPACT)) {
    this->PUSH_ENV_NOINDENT(HTML_DL);
  }
  if (this->envc > 0) {
    this->flushline();
  }
  if (!(this->flag & RB_IN_DT)) {
    this->parse("<b>");
    this->flag |= RB_IN_DT;
  }
  this->flag |= RB_IGNORE_P;
  return 1;
}

int html_feed_environ::HTML_DT_exit(const std::shared_ptr<HtmlTag> &tag) {
  if (!(this->flag & RB_IN_DT)) {
    return 1;
  }
  this->flag &= ~RB_IN_DT;
  this->parse("</b>");
  if (this->envc > 0 && this->envs[this->envc].env == HTML_DL) {
    this->flushline();
  }
  return 1;
}

int html_feed_environ::HTML_DD_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_A();
  this->CLOSE_DT();
  if (this->envs[this->envc].env == HTML_DL ||
      this->envs[this->envc].env == HTML_DL_COMPACT) {
    this->PUSH_ENV(HTML_DD);
  }

  if (this->envc > 0 && this->envs[this->envc - 1].env == HTML_DL_COMPACT) {
    if (this->pos > this->envs[this->envc].indent) {
      this->flushline();
    } else {
      this->push_spaces(1, this->envs[this->envc].indent - this->pos);
    }
  } else {
    this->flushline();
  }
  /* this->flag |= RB_IGNORE_P; */
  return 1;
}

int html_feed_environ::HTML_TITLE_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->close_anchor();
  this->process_title();
  this->flag |= RB_TITLE;
  this->end_tag = HTML_N_TITLE;
  return 1;
}

int html_feed_environ::HTML_TITLE_exit(const std::shared_ptr<HtmlTag> &tag) {
  if (!(this->flag & RB_TITLE))
    return 1;
  this->flag &= ~RB_TITLE;
  this->end_tag = {};
  auto tmp = this->process_n_title();
  if (tmp.size())
    this->parse(tmp);
  return 1;
}

int html_feed_environ::HTML_TITLE_ALT_enter(
    const std::shared_ptr<HtmlTag> &tag) {
  if (auto value = tag->getAttr(ATTR_TITLE))
    this->_title = html_unquote(*value);
  return 0;
}

int html_feed_environ::HTML_FRAMESET_enter(
    const std::shared_ptr<HtmlTag> &tag) {
  this->PUSH_ENV(tag->tagid);
  this->push_charp(9, "--FRAME--", PC_ASCII);
  this->flushline();
  return 0;
}

int html_feed_environ::HTML_FRAMESET_exit(const std::shared_ptr<HtmlTag> &tag) {
  if (this->envc > 0) {
    this->POP_ENV();
    this->flushline();
  }
  return 0;
}

int html_feed_environ::HTML_NOFRAMES_enter(
    const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_A();
  this->flushline();
  this->flag |= (RB_NOFRAMES | RB_IGNORE_P);
  /* istr = str; */
  return 1;
}

int html_feed_environ::HTML_NOFRAMES_exit(const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_A();
  this->flushline();
  this->flag &= ~RB_NOFRAMES;
  return 1;
}

int html_feed_environ::HTML_FRAME_enter(const std::shared_ptr<HtmlTag> &tag) {
  std::string q;
  if (auto value = tag->getAttr(ATTR_SRC))
    q = *value;
  std::string r;
  if (auto value = tag->getAttr(ATTR_NAME))
    r = *value;
  if (q.size()) {
    q = html_quote(q);
    std::stringstream ss;
    ss << "<a hseq=\"" << this->hseqAndIncrement() << "\" href=\"" << q
       << "\">";
    this->push_tag(ss.str(), HTML_A);
    if (r.size())
      q = html_quote(r);
    this->push_charp(get_strwidth(q), q.c_str(), PC_ASCII);
    this->push_tag("</a>", HTML_N_A);
  }
  this->flushline();
  return 0;
}

int html_feed_environ::HTML_HR_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->close_anchor();
  auto tmp = this->process_hr(tag, this->_width, this->envs[this->envc].indent);
  this->parse(tmp);
  this->prevchar = " ";
  return 1;
}

int html_feed_environ::HTML_PRE_enter(const std::shared_ptr<HtmlTag> &tag) {
  int x = tag->existsAttr(ATTR_FOR_TABLE);
  this->CLOSE_A();
  if (!(this->flag & RB_IGNORE_P)) {
    this->flushline();
    if (!x) {
      this->do_blankline();
    }
  } else {
    this->fillline();
  }
  this->flag |= (RB_PRE | RB_IGNORE_P);
  /* istr = str; */
  return 1;
}

int html_feed_environ::HTML_PRE_exit(const std::shared_ptr<HtmlTag> &tag) {
  this->flushline();
  if (!(this->flag & RB_IGNORE_P)) {
    this->do_blankline();
    this->flag |= RB_IGNORE_P;
    this->incrementBlankLines();
  }
  this->flag &= ~RB_PRE;
  this->close_anchor();
  return 1;
}

int html_feed_environ::HTML_PRE_PLAIN_enter(
    const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_A();
  if (!(this->flag & RB_IGNORE_P)) {
    this->flushline();
    this->do_blankline();
  }
  this->flag |= (RB_PRE | RB_IGNORE_P);
  return 1;
}

int html_feed_environ::HTML_PRE_PLAIN_exit(
    const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_A();
  if (!(this->flag & RB_IGNORE_P)) {
    this->flushline();
    this->do_blankline();
    this->flag |= RB_IGNORE_P;
  }
  this->flag &= ~RB_PRE;
  return 1;
}

int html_feed_environ::HTML_PLAINTEXT_enter(
    const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_A();
  if (!(this->flag & RB_IGNORE_P)) {
    this->flushline();
    this->do_blankline();
  }
  this->flag |= (RB_PLAIN | RB_IGNORE_P);
  switch (tag->tagid) {
  case HTML_LISTING:
    this->end_tag = HTML_N_LISTING;
    break;
  case HTML_XMP:
    this->end_tag = HTML_N_XMP;
    break;
  case HTML_PLAINTEXT:
    this->end_tag = MAX_HTMLTAG;
    break;

  default:
    break;
  }
  return 1;
}

int html_feed_environ::HTML_LISTING_exit(const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_A();
  if (!(this->flag & RB_IGNORE_P)) {
    this->flushline();
    this->do_blankline();
    this->flag |= RB_IGNORE_P;
  }
  this->flag &= ~RB_PLAIN;
  this->end_tag = {};
  return 1;
}

int html_feed_environ::HTML_A_enter(const std::shared_ptr<HtmlTag> &tag) {
  if (this->anchor.url.size()) {
    this->close_anchor();
  }

  if (auto value = tag->getAttr(ATTR_HREF))
    this->anchor.url = *value;
  if (auto value = tag->getAttr(ATTR_TARGET))
    this->anchor.target = *value;
  if (auto value = tag->getAttr(ATTR_REFERER))
    this->anchor.option = {.referer = *value};
  if (auto value = tag->getAttr(ATTR_TITLE))
    this->anchor.title = *value;
  if (auto value = tag->getAttr(ATTR_ACCESSKEY))
    this->anchor.accesskey = (*value)[0];
  if (auto value = tag->getAttr(ATTR_HSEQ))
    this->anchor.hseq = stoi(*value);

  if (this->anchor.hseq == 0 && this->anchor.url.size()) {
    this->anchor.hseq = this->cur_hseq();
    auto tmp = this->process_anchor(tag, this->tagbuf());
    this->push_tag(tmp.c_str(), HTML_A);
    return 1;
  }
  return 0;
}

int html_feed_environ::HTML_IMG_enter(const std::shared_ptr<HtmlTag> &tag) {
  if (tag->existsAttr(ATTR_USEMAP))
    this->HTML5_CLOSE_A();
  auto tmp = this->process_img(tag, this->_width);
  this->parse(tmp);
  return 1;
}

int html_feed_environ::HTML_IMG_ALT_enter(const std::shared_ptr<HtmlTag> &tag) {
  if (auto value = tag->getAttr(ATTR_SRC))
    this->img_alt = *value;
  return 0;
}

int html_feed_environ::HTML_IMG_ALT_exit(const std::shared_ptr<HtmlTag> &tag) {
  if (this->img_alt.size()) {
    if (!this->close_effect0(HTML_IMG_ALT))
      this->push_tag("</img_alt>", HTML_N_IMG_ALT);
    this->img_alt = {};
  }
  return 1;
}

int html_feed_environ::HTML_TABLE_enter(const std::shared_ptr<HtmlTag> &tag) {
  int cols = this->_width;
  this->close_anchor();
  if (this->table_level + 1 >= MAX_TABLE) {
    return -1;
  }
  this->table_level++;

  HtmlTableBorderMode w = BORDER_NONE;
  /* x: cellspacing, y: cellpadding */
  int width = 0;
  if (tag->existsAttr(ATTR_BORDER)) {
    if (auto value = tag->getAttr(ATTR_BORDER)) {
      w = (HtmlTableBorderMode)stoi(*value);
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
  if (auto value = tag->getAttr(ATTR_WIDTH)) {
    if (this->table_level == 0)
      width = REAL_WIDTH(stoi(*value),
                         this->_width - this->envs[this->envc].indent);
    else
      width = RELATIVE_WIDTH(stoi(*value));
  }
  if (tag->existsAttr(ATTR_HBORDER))
    w = BORDER_NOWIN;

  int x = 2;
  if (auto value = tag->getAttr(ATTR_CELLSPACING)) {
    x = stoi(*value);
  }
  int y = 1;
  if (auto value = tag->getAttr(ATTR_CELLPADDING)) {
    y = stoi(*value);
  }
  int z = 0;
  if (auto value = tag->getAttr(ATTR_VSPACE)) {
    z = stoi(*value);
  }
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

  this->tables[this->table_level] = table::begin_table(w, x, y, z, cols, width);
  this->table_mode[this->table_level].pre_mode = {};
  this->table_mode[this->table_level].indent_level = 0;
  this->table_mode[this->table_level].nobr_level = 0;
  this->table_mode[this->table_level].caption = 0;
  this->table_mode[this->table_level].end_tag = HTML_UNKNOWN;
  return 1;
}

int html_feed_environ::HTML_CENTER_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_A();
  if (!(this->flag & (RB_PREMODE | RB_IGNORE_P)))
    this->flushline();
  this->RB_SAVE_FLAG();
  if (DisableCenter) {
    this->RB_SET_ALIGN(RB_LEFT);
  } else {
    this->RB_SET_ALIGN(RB_CENTER);
  }
  return 1;
}

int html_feed_environ::HTML_CENTER_exit(const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_A();
  if (!(this->flag & RB_PREMODE)) {
    this->flushline();
  }
  this->RB_RESTORE_FLAG();
  return 1;
}

int html_feed_environ::HTML_DIV_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_A();
  if (!(this->flag & RB_IGNORE_P)) {
    this->flushline();
  }
  this->set_alignment(tag->alignFlag());
  return 1;
}

int html_feed_environ::HTML_DIV_exit(const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_A();
  this->flushline();
  this->RB_RESTORE_FLAG();
  return 1;
}

int html_feed_environ::HTML_DIV_INT_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_P();
  if (!(this->flag & RB_IGNORE_P)) {
    this->flushline();
  }
  this->set_alignment(tag->alignFlag());
  return 1;
}

int html_feed_environ::HTML_DIV_INT_exit(const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_P();
  this->flushline();
  this->RB_RESTORE_FLAG();
  return 1;
}

int html_feed_environ::HTML_FORM_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_A();
  if (!(this->flag & RB_IGNORE_P)) {
    this->flushline();
  }
  auto tmp = this->process_form(tag);
  if (tmp.size())
    this->parse(tmp);
  return 1;
}

int html_feed_environ::HTML_FORM_exit(const std::shared_ptr<HtmlTag> &tag) {
  this->CLOSE_A();
  this->flushline();
  this->flag |= RB_IGNORE_P;
  this->process_n_form();
  return 1;
}

int html_feed_environ::HTML_INPUT_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->close_anchor();
  auto tmp = this->process_input(tag);
  if (tmp.size())
    this->parse(tmp);
  return 1;
}

int html_feed_environ::HTML_BUTTON_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->HTML5_CLOSE_A();
  auto tmp = this->process_button(tag);
  if (tmp.size())
    this->parse(tmp);
  return 1;
}

int html_feed_environ::HTML_BUTTON_exit(const std::shared_ptr<HtmlTag> &tag) {
  auto tmp = this->process_n_button();
  if (tmp.size())
    this->parse(tmp);
  return 1;
}

int html_feed_environ::HTML_SELECT_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->close_anchor();
  auto tmp = this->process_select(tag);
  if (tmp.size())
    this->parse(tmp);
  this->flag |= RB_INSELECT;
  this->end_tag = HTML_N_SELECT;
  return 1;
}

int html_feed_environ::HTML_SELECT_exit(const std::shared_ptr<HtmlTag> &tag) {
  this->flag &= ~RB_INSELECT;
  this->end_tag = HTML_UNKNOWN;
  auto tmp = this->process_n_select();
  if (tmp.size())
    this->parse(tmp);
  return 1;
}

int html_feed_environ::HTML_TEXTAREA_enter(
    const std::shared_ptr<HtmlTag> &tag) {
  this->close_anchor();
  auto tmp = this->process_textarea(tag, this->_width);
  if (tmp.size()) {
    this->parse(tmp);
  }
  this->flag |= RB_INTXTA;
  this->end_tag = HTML_N_TEXTAREA;
  return 1;
}

int html_feed_environ::HTML_TEXTAREA_exit(const std::shared_ptr<HtmlTag> &tag) {
  this->flag &= ~RB_INTXTA;
  this->end_tag = HTML_UNKNOWN;
  auto tmp = this->process_n_textarea();
  if (tmp.size()) {
    this->parse(tmp);
  }
  return 1;
}

int html_feed_environ::HTML_ISINDEX_enter(const std::shared_ptr<HtmlTag> &tag) {
  std::string p = "";
  if (auto value = tag->getAttr(ATTR_PROMPT)) {
    p = *value;
  }
  std::string q = "!CURRENT_URL!";
  if (auto value = tag->getAttr(ATTR_ACTION)) {
    q = *value;
  }
  std::stringstream tmp;
  tmp << "<form method=get action=\"" << html_quote(q) << "\">" << html_quote(p)
      << "<input type=text name=\"\" accept></form>";
  this->parse(tmp.str());
  return 1;
}

int html_feed_environ::HTML_META_enter(const std::shared_ptr<HtmlTag> &tag) {
  std::string p;
  if (auto value = tag->getAttr(ATTR_HTTP_EQUIV)) {
    p = *value;
  }
  std::string q;
  if (auto value = tag->getAttr(ATTR_CONTENT)) {
    q = *value;
  }
  if (p.size() && q.size() && !strcasecmp(p, "refresh")) {
    auto meta = getMetaRefreshParam(q);
    std::stringstream ss;
    if (meta.url.size()) {
      auto qq = html_quote(meta.url);
      ss << "Refresh (" << meta.interval << " sec) <a href=\"" << qq << "\">"
         << qq << "</a>";
    } else if (meta.interval > 0) {
      ss << "Refresh (" << meta.interval << " sec)";
    }
    auto tmp = ss.str();
    if (tmp.size()) {
      this->parse(tmp);
      this->do_blankline();
    }
  }
  return 1;
}

int html_feed_environ::HTML_DEL_enter(const std::shared_ptr<HtmlTag> &tag) {
  switch (displayInsDel) {
  case DISPLAY_INS_DEL_SIMPLE:
    this->flag |= RB_DEL;
    break;
  case DISPLAY_INS_DEL_NORMAL:
    this->parse("<U>[DEL:</U>");
    break;
  case DISPLAY_INS_DEL_FONTIFY:
    if (this->fontstat.in_strike < FONTSTAT_MAX)
      this->fontstat.in_strike++;
    if (this->fontstat.in_strike == 1) {
      this->push_tag("<s>", HTML_S);
    }
    break;
  }
  return 1;
}

int html_feed_environ::HTML_DEL_exit(const std::shared_ptr<HtmlTag> &tag) {
  switch (displayInsDel) {
  case DISPLAY_INS_DEL_SIMPLE:
    this->flag &= ~RB_DEL;
    break;
  case DISPLAY_INS_DEL_NORMAL:
    this->parse("<U>:DEL]</U>");
  case DISPLAY_INS_DEL_FONTIFY:
    if (this->fontstat.in_strike == 0)
      return 1;
    if (this->fontstat.in_strike == 1 && this->close_effect0(HTML_S))
      this->fontstat.in_strike = 0;
    if (this->fontstat.in_strike > 0) {
      this->fontstat.in_strike--;
      if (this->fontstat.in_strike == 0) {
        this->push_tag("</s>", HTML_N_S);
      }
    }
    break;
  }
  return 1;
}

int html_feed_environ::HTML_S_enter(const std::shared_ptr<HtmlTag> &tag) {
  switch (displayInsDel) {
  case DISPLAY_INS_DEL_SIMPLE:
    this->flag |= RB_S;
    break;
  case DISPLAY_INS_DEL_NORMAL:
    this->parse("<U>[S:</U>");
    break;
  case DISPLAY_INS_DEL_FONTIFY:
    if (this->fontstat.in_strike < FONTSTAT_MAX)
      this->fontstat.in_strike++;
    if (this->fontstat.in_strike == 1) {
      this->push_tag("<s>", HTML_S);
    }
    break;
  }
  return 1;
}

int html_feed_environ::HTML_S_exit(const std::shared_ptr<HtmlTag> &tag) {
  switch (displayInsDel) {
  case DISPLAY_INS_DEL_SIMPLE:
    this->flag &= ~RB_S;
    break;
  case DISPLAY_INS_DEL_NORMAL:
    this->parse("<U>:S]</U>");
    break;
  case DISPLAY_INS_DEL_FONTIFY:
    if (this->fontstat.in_strike == 0)
      return 1;
    if (this->fontstat.in_strike == 1 && this->close_effect0(HTML_S))
      this->fontstat.in_strike = 0;
    if (this->fontstat.in_strike > 0) {
      this->fontstat.in_strike--;
      if (this->fontstat.in_strike == 0) {
        this->push_tag("</s>", HTML_N_S);
      }
    }
  }
  return 1;
}

int html_feed_environ::HTML_INS_enter(const std::shared_ptr<HtmlTag> &tag) {
  switch (displayInsDel) {
  case DISPLAY_INS_DEL_SIMPLE:
    break;
  case DISPLAY_INS_DEL_NORMAL:
    this->parse("<U>[INS:</U>");
    break;
  case DISPLAY_INS_DEL_FONTIFY:
    if (this->fontstat.in_ins < FONTSTAT_MAX)
      this->fontstat.in_ins++;
    if (this->fontstat.in_ins == 1) {
      this->push_tag("<ins>", HTML_INS);
    }
    break;
  }
  return 1;
}

int html_feed_environ::HTML_INS_exit(const std::shared_ptr<HtmlTag> &tag) {
  switch (displayInsDel) {
  case DISPLAY_INS_DEL_SIMPLE:
    break;
  case DISPLAY_INS_DEL_NORMAL:
    this->parse("<U>:INS]</U>");
    break;
  case DISPLAY_INS_DEL_FONTIFY:
    if (this->fontstat.in_ins == 0)
      return 1;
    if (this->fontstat.in_ins == 1 && this->close_effect0(HTML_INS))
      this->fontstat.in_ins = 0;
    if (this->fontstat.in_ins > 0) {
      this->fontstat.in_ins--;
      if (this->fontstat.in_ins == 0) {
        this->push_tag("</ins>", HTML_N_INS);
      }
    }
    break;
  }
  return 1;
}

int html_feed_environ::HTML_BGSOUND_enter(const std::shared_ptr<HtmlTag> &tag) {
  if (view_unseenobject) {
    if (auto value = tag->getAttr(ATTR_SRC)) {
      auto q = html_quote(*value);
      std::stringstream s;
      s << "<A HREF=\"" << q << "\">bgsound(" << q << ")</A>";
      this->parse(s.str());
    }
  }
  return 1;
}

int html_feed_environ::HTML_EMBED_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->HTML5_CLOSE_A();
  if (view_unseenobject) {
    if (auto value = tag->getAttr(ATTR_SRC)) {
      auto q = html_quote(*value);
      std::stringstream s;
      s << "<A HREF=\"" << q << "\">embed(" << q << ")</A>";
      this->parse(s.str());
    }
  }
  return 1;
}

int html_feed_environ::HTML_APPLET_enter(const std::shared_ptr<HtmlTag> &tag) {
  if (view_unseenobject) {
    if (auto value = tag->getAttr(ATTR_ARCHIVE)) {
      auto q = html_quote(*value);
      std::stringstream s;
      s << "<A HREF=\"" << q << "\">applet archive(" << q << ")</A>";
      this->parse(s.str());
    }
  }
  return 1;
}

int html_feed_environ::HTML_BODY_enter(const std::shared_ptr<HtmlTag> &tag) {
  if (view_unseenobject) {
    if (auto value = tag->getAttr(ATTR_BACKGROUND)) {
      auto q = html_quote(*value);
      std::stringstream s;
      s << "<IMG SRC=\"" << q << "\" ALT=\"bg image(" << q << ")\"><BR>";
      this->parse(s.str());
    }
  }
  return 1;
}

int html_feed_environ::HTML_INPUT_ALT_enter(
    const std::shared_ptr<HtmlTag> &tag) {
  if (auto value = tag->getAttr(ATTR_TOP_MARGIN)) {
    this->setTopMargin(stoi(*value));
  }
  if (auto value = tag->getAttr(ATTR_BOTTOM_MARGIN)) {
    this->setBottomMargin(stoi(*value));
  }
  if (auto value = tag->getAttr(ATTR_HSEQ)) {
    this->input_alt.hseq = stoi(*value);
  }
  if (auto value = tag->getAttr(ATTR_FID)) {
    this->input_alt.fid = stoi(*value);
  }
  if (auto value = tag->getAttr(ATTR_TYPE)) {
    this->input_alt.type = *value;
  }
  if (auto value = tag->getAttr(ATTR_VALUE)) {
    this->input_alt.value = *value;
  }
  if (auto value = tag->getAttr(ATTR_NAME)) {
    this->input_alt.name = *value;
  }
  this->input_alt.in = 1;
  return 0;
}

int html_feed_environ::HTML_INPUT_ALT_exit(
    const std::shared_ptr<HtmlTag> &tag) {
  if (this->input_alt.in) {
    if (!this->close_effect0(HTML_INPUT_ALT))
      this->push_tag("</input_alt>", HTML_N_INPUT_ALT);
    this->input_alt.hseq = 0;
    this->input_alt.fid = -1;
    this->input_alt.in = 0;
    this->input_alt.type = {};
    this->input_alt.name = {};
    this->input_alt.value = {};
  }
  return 1;
}

int html_feed_environ::HTML_B_enter(const std::shared_ptr<HtmlTag> &tag) {
  if (this->fontstat.in_bold < FONTSTAT_MAX)
    this->fontstat.in_bold++;
  if (this->fontstat.in_bold > 1)
    return 1;
  return 0;
}

int html_feed_environ::HTML_B_exit(const std::shared_ptr<HtmlTag> &tag) {
  if (this->fontstat.in_bold == 1 && this->close_effect0(HTML_B))
    this->fontstat.in_bold = 0;
  if (this->fontstat.in_bold > 0) {
    this->fontstat.in_bold--;
    if (this->fontstat.in_bold == 0)
      return 0;
  }
  return 1;
}

int html_feed_environ::HTML_I_enter(const std::shared_ptr<HtmlTag> &tag) {
  if (this->fontstat.in_italic < FONTSTAT_MAX)
    this->fontstat.in_italic++;
  if (this->fontstat.in_italic > 1)
    return 1;
  return 0;
}

int html_feed_environ::HTML_I_exit(const std::shared_ptr<HtmlTag> &tag) {
  if (this->fontstat.in_italic == 1 && this->close_effect0(HTML_I))
    this->fontstat.in_italic = 0;
  if (this->fontstat.in_italic > 0) {
    this->fontstat.in_italic--;
    if (this->fontstat.in_italic == 0)
      return 0;
  }
  return 1;
}

int html_feed_environ::HTML_U_enter(const std::shared_ptr<HtmlTag> &tag) {
  if (this->fontstat.in_under < FONTSTAT_MAX)
    this->fontstat.in_under++;
  if (this->fontstat.in_under > 1)
    return 1;
  return 0;
}

int html_feed_environ::HTML_U_exit(const std::shared_ptr<HtmlTag> &tag) {
  if (this->fontstat.in_under == 1 && this->close_effect0(HTML_U))
    this->fontstat.in_under = 0;
  if (this->fontstat.in_under > 0) {
    this->fontstat.in_under--;
    if (this->fontstat.in_under == 0)
      return 0;
  }
  return 1;
}

int html_feed_environ::HTML_PRE_INT_enter(const std::shared_ptr<HtmlTag> &tag) {
  int i = this->line.size();
  // this->append_tags();
  if (!(this->flag & RB_SPECIAL)) {
    this->set_breakpoint(this->line.size() - i);
  }
  this->flag |= RB_PRE_INT;
  return 0;
}

int html_feed_environ::HTML_PRE_INT_exit(const std::shared_ptr<HtmlTag> &tag) {
  this->push_tag("</pre_int>", HTML_N_PRE_INT);
  this->flag &= ~RB_PRE_INT;
  if (!(this->flag & RB_SPECIAL) && this->pos > this->bp.pos) {
    this->prevchar = "";
    this->prev_ctype = PC_CTRL;
  }
  return 1;
}

int html_feed_environ::HTML_NOBR_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->flag |= RB_NOBR;
  this->nobr_level++;
  return 0;
}

int html_feed_environ::HTML_NOBR_exit(const std::shared_ptr<HtmlTag> &tag) {
  if (this->nobr_level > 0)
    this->nobr_level--;
  if (this->nobr_level == 0)
    this->flag &= ~RB_NOBR;
  return 0;
}
