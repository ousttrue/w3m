#include "html_tag.h"
#include "form.h"
#include "html_feed_env.h"
#include "ctrlcode.h"
#include "push_symbol.h"
#include "quote.h"
#include "cmp.h"
#include "utf8.h"
#include "option_param.h"
#include "html_meta.h"
#include "html_table_border_mode.h"
#include "html_table.h"
#include <sstream>

#define MAX_UL_LEVEL 9
#define MAX_CELLSPACING 1000
#define MAX_CELLPADDING 1000
#define MAX_VSPACE 1000

bool HtmlTag::setAttr(HtmlTagAttr id, const std::string &value) {
  if (!this->acceptsAttr(id))
    return false;
  auto i = this->map[id];
  this->attrid[i] = id;
  this->value[i] = value;
  this->need_reconstruct = true;
  return true;
}

std::optional<std::string> HtmlTag::getAttr(HtmlTagAttr id) const {
  if (this->map.empty()) {
    return {};
  }
  int i = this->map[id];
  if (!this->existsAttr(id) || this->value[i].empty()) {
    return {};
  }
  return this->value[i];
}

int HtmlTag::process(html_feed_environ *h_env) {
  if (h_env->flag & RB_PRE) {
    switch (this->tagid) {
    case HTML_NOBR:
    case HTML_N_NOBR:
    case HTML_PRE_INT:
    case HTML_N_PRE_INT:
      return 1;

    default:
      break;
    }
  }

  switch (this->tagid) {
  case HTML_B:
    return this->HTML_B_enter(h_env);
  case HTML_N_B:
    return this->HTML_B_exit(h_env);
  case HTML_I:
    return this->HTML_I_enter(h_env);
  case HTML_N_I:
    return this->HTML_I_exit(h_env);
  case HTML_U:
    return this->HTML_U_enter(h_env);
  case HTML_N_U:
    return this->HTML_U_exit(h_env);
  case HTML_EM:
    h_env->parse("<i>");
    return 1;
  case HTML_N_EM:
    h_env->parse("</i>");
    return 1;
  case HTML_STRONG:
    h_env->parse("<b>");
    return 1;
  case HTML_N_STRONG:
    h_env->parse("</b>");
    return 1;
  case HTML_Q:
    h_env->parse("`");
    return 1;
  case HTML_N_Q:
    h_env->parse("'");
    return 1;
  case HTML_FIGURE:
  case HTML_N_FIGURE:
  case HTML_P:
  case HTML_N_P:
    return this->HTML_Paragraph(h_env);
  case HTML_FIGCAPTION:
  case HTML_N_FIGCAPTION:
  case HTML_BR:
    h_env->flushline(FlushLineMode::Force);
    h_env->blank_lines = 0;
    return 1;
  case HTML_H:
    return this->HTML_H_enter(h_env);
  case HTML_N_H:
    return this->HTML_H_exit(h_env);
  case HTML_UL:
  case HTML_OL:
  case HTML_BLQ:
    return this->HTML_List_enter(h_env);
  case HTML_N_UL:
  case HTML_N_OL:
  case HTML_N_DL:
  case HTML_N_BLQ:
  case HTML_N_DD:
    return this->HTML_List_exit(h_env);
  case HTML_DL:
    return this->HTML_DL_enter(h_env);
  case HTML_LI:
    return this->HTML_LI_enter(h_env);
  case HTML_DT:
    return this->HTML_DT_enter(h_env);
  case HTML_N_DT:
    return this->HTML_DT_exit(h_env);
  case HTML_DD:
    return this->HTML_DD_enter(h_env);
  case HTML_TITLE:
    return this->HTML_TITLE_enter(h_env);
  case HTML_N_TITLE:
    return this->HTML_TITLE_exit(h_env);
  case HTML_TITLE_ALT:
    return this->HTML_TITLE_ALT_enter(h_env);
  case HTML_FRAMESET:
    return this->HTML_FRAMESET_enter(h_env);
  case HTML_N_FRAMESET:
    return this->HTML_FRAMESET_exit(h_env);
  case HTML_NOFRAMES:
    return this->HTML_NOFRAMES_enter(h_env);
  case HTML_N_NOFRAMES:
    return this->HTML_NOFRAMES_exit(h_env);
  case HTML_FRAME:
    return this->HTML_FRAME_enter(h_env);
  case HTML_HR:
    return this->HTML_HR_enter(h_env);
  case HTML_PRE:
    return this->HTML_PRE_enter(h_env);
  case HTML_N_PRE:
    return this->HTML_PRE_exit(h_env);
  case HTML_PRE_INT:
    return this->HTML_PRE_INT_enter(h_env);
  case HTML_N_PRE_INT:
    return this->HTML_PRE_INT_exit(h_env);
  case HTML_NOBR:
    return this->HTML_NOBR_enter(h_env);
  case HTML_N_NOBR:
    return this->HTML_NOBR_exit(h_env);
  case HTML_PRE_PLAIN:
    return this->HTML_PRE_PLAIN_enter(h_env);
  case HTML_N_PRE_PLAIN:
    return this->HTML_PRE_PLAIN_exit(h_env);
  case HTML_LISTING:
  case HTML_XMP:
  case HTML_PLAINTEXT:
    return this->HTML_PLAINTEXT_enter(h_env);
  case HTML_N_LISTING:
  case HTML_N_XMP:
    return this->HTML_LISTING_exit(h_env);
  case HTML_SCRIPT:
    h_env->flag |= RB_SCRIPT;
    h_env->end_tag = HTML_N_SCRIPT;
    return 1;
  case HTML_STYLE:
    h_env->flag |= RB_STYLE;
    h_env->end_tag = HTML_N_STYLE;
    return 1;
  case HTML_N_SCRIPT:
    h_env->flag &= ~RB_SCRIPT;
    h_env->end_tag = HTML_UNKNOWN;
    return 1;
  case HTML_N_STYLE:
    h_env->flag &= ~RB_STYLE;
    h_env->end_tag = HTML_UNKNOWN;
    return 1;
  case HTML_A:
    return this->HTML_A_enter(h_env);
  case HTML_N_A:
    h_env->close_anchor();
    return 1;
  case HTML_IMG:
    return this->HTML_IMG_enter(h_env);
  case HTML_IMG_ALT:
    return this->HTML_IMG_ALT_enter(h_env);
  case HTML_N_IMG_ALT:
    return this->HTML_IMG_ALT_exit(h_env);
  case HTML_INPUT_ALT:
    return this->HTML_INPUT_ALT_enter(h_env);
  case HTML_N_INPUT_ALT:
    return this->HTML_INPUT_ALT_exit(h_env);
  case HTML_TABLE:
    return this->HTML_TABLE_enter(h_env);
  case HTML_N_TABLE:
    // should be processed in HTMLlineproc()
    return 1;
  case HTML_CENTER:
    return this->HTML_CENTER_enter(h_env);
  case HTML_N_CENTER:
    return this->HTML_CENTER_exit(h_env);
  case HTML_DIV:
    return this->HTML_DIV_enter(h_env);
  case HTML_N_DIV:
    return this->HTML_DIV_exit(h_env);
  case HTML_DIV_INT:
    return this->HTML_DIV_INT_enter(h_env);
  case HTML_N_DIV_INT:
    return this->HTML_DIV_INT_exit(h_env);
  case HTML_FORM:
    return this->HTML_FORM_enter(h_env);
  case HTML_N_FORM:
    return this->HTML_FORM_exit(h_env);
  case HTML_INPUT:
    return this->HTML_INPUT_enter(h_env);
  case HTML_BUTTON:
    return this->HTML_BUTTON_enter(h_env);
  case HTML_N_BUTTON:
    return this->HTML_BUTTON_exit(h_env);
  case HTML_SELECT:
    return this->HTML_SELECT_enter(h_env);
  case HTML_N_SELECT:
    return this->HTML_SELECT_exit(h_env);
  case HTML_OPTION:
    /* nothing */
    return 1;
  case HTML_TEXTAREA:
    return this->HTML_TEXTAREA_enter(h_env);
  case HTML_N_TEXTAREA:
    return this->HTML_TEXTAREA_exit(h_env);
  case HTML_ISINDEX:
    return this->HTML_ISINDEX_enter(h_env);
  case HTML_DOCTYPE:
    if (!this->existsAttr(ATTR_PUBLIC)) {
      h_env->flag |= RB_HTML5;
    }
    return 1;
  case HTML_META:
    return this->HTML_META_enter(h_env);
  case HTML_BASE:
  case HTML_MAP:
  case HTML_N_MAP:
  case HTML_AREA:
    return 0;
  case HTML_DEL:
    return this->HTML_DEL_enter(h_env);
  case HTML_N_DEL:
    return this->HTML_DEL_exit(h_env);
  case HTML_S:
    return this->HTML_S_enter(h_env);
  case HTML_N_S:
    return this->HTML_S_exit(h_env);
  case HTML_INS:
    return this->HTML_INS_enter(h_env);
  case HTML_N_INS:
    return this->HTML_INS_exit(h_env);
  case HTML_SUP:
    if (!(h_env->flag & (RB_DEL | RB_S)))
      h_env->parse("^");
    return 1;
  case HTML_N_SUP:
    return 1;
  case HTML_SUB:
    if (!(h_env->flag & (RB_DEL | RB_S)))
      h_env->parse("[");
    return 1;
  case HTML_N_SUB:
    if (!(h_env->flag & (RB_DEL | RB_S)))
      h_env->parse("]");
    return 1;
  case HTML_FONT:
  case HTML_N_FONT:
  case HTML_NOP:
    return 1;
  case HTML_BGSOUND:
    return this->HTML_BGSOUND_enter(h_env);
  case HTML_EMBED:
    return this->HTML_EMBED_enter(h_env);
  case HTML_APPLET:
    return this->HTML_APPLET_enter(h_env);
  case HTML_BODY:
    return this->HTML_BODY_enter(h_env);
  case HTML_N_HEAD:
    if (h_env->flag & RB_TITLE)
      h_env->parse("</title>");
    return 1;
  case HTML_HEAD:
  case HTML_N_BODY:
    return 1;
  default:
    /* h_env->prevchar = '\0'; */
    return 0;
  }

  return 0;
}

ReadBufferFlags HtmlTag::alignFlag() const {
  ReadBufferFlags flag = (ReadBufferFlags)-1;
  if (auto value = this->getAttr(ATTR_ALIGN)) {
    switch (stoi(*value)) {
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
  return flag;
}

int HtmlTag::HTML_Paragraph(html_feed_environ *h_env) {
  h_env->CLOSE_A();
  if (!(h_env->flag & RB_IGNORE_P)) {
    h_env->flushline(FlushLineMode::Force);
    h_env->do_blankline();
  }
  h_env->flag |= RB_IGNORE_P;
  if (this->tagid == HTML_P) {
    h_env->set_alignment(this->alignFlag());
    h_env->flag |= RB_P;
  }
  return 1;
}

int HtmlTag::HTML_H_enter(html_feed_environ *h_env) {
  if (!(h_env->flag & (RB_PREMODE | RB_IGNORE_P))) {
    h_env->flushline();
    h_env->do_blankline();
  }
  h_env->parse("<b>");
  h_env->set_alignment(this->alignFlag());
  return 1;
}

int HtmlTag::HTML_H_exit(html_feed_environ *h_env) {
  h_env->parse("</b>");
  if (!(h_env->flag & RB_PREMODE)) {
    h_env->flushline();
  }
  h_env->do_blankline();
  h_env->RB_RESTORE_FLAG();
  h_env->close_anchor();
  h_env->flag |= RB_IGNORE_P;
  return 1;
}

int HtmlTag::HTML_List_enter(html_feed_environ *h_env) {
  h_env->CLOSE_A();
  if (!(h_env->flag & RB_IGNORE_P)) {
    h_env->flushline();
    if (!(h_env->flag & RB_PREMODE) &&
        (h_env->envc == 0 || this->tagid == HTML_BLQ)) {
      h_env->do_blankline();
    }
  }
  h_env->PUSH_ENV(this->tagid);
  if (this->tagid == HTML_UL || this->tagid == HTML_OL) {
    if (auto value = this->getAttr(ATTR_START)) {
      h_env->envs[h_env->envc].count = stoi(*value) - 1;
    }
  }
  if (this->tagid == HTML_OL) {
    h_env->envs[h_env->envc].type = '1';
    if (auto value = this->getAttr(ATTR_TYPE)) {
      h_env->envs[h_env->envc].type = (*value)[0];
    }
  }
  if (this->tagid == HTML_UL) {
    // h_env->envs[this->envc].type = tag->ul_type(0);
    //  int HtmlTag::ul_type(int default_type) const {
    //    if (auto value = this->getAttr(ATTR_TYPE)) {
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
  h_env->flushline();
  return 1;
}

int HtmlTag::HTML_List_exit(html_feed_environ *h_env) {
  h_env->CLOSE_DT();
  h_env->CLOSE_A();
  if (h_env->envc > 0) {
    h_env->flushline();
    h_env->POP_ENV();
    if (!(h_env->flag & RB_PREMODE) &&
        (h_env->envc == 0 || this->tagid == HTML_N_BLQ)) {
      h_env->do_blankline();
      h_env->flag |= RB_IGNORE_P;
    }
  }
  h_env->close_anchor();
  return 1;
}

int HtmlTag::HTML_DL_enter(html_feed_environ *h_env) {
  h_env->CLOSE_A();
  if (!(h_env->flag & RB_IGNORE_P)) {
    h_env->flushline();
    if (!(h_env->flag & RB_PREMODE) &&
        h_env->envs[h_env->envc].env != HTML_DL &&
        h_env->envs[h_env->envc].env != HTML_DL_COMPACT &&
        h_env->envs[h_env->envc].env != HTML_DD) {
      h_env->do_blankline();
    }
  }
  h_env->PUSH_ENV_NOINDENT(this->tagid);
  if (this->existsAttr(ATTR_COMPACT)) {
    h_env->envs[h_env->envc].env = HTML_DL_COMPACT;
  }
  h_env->flag |= RB_IGNORE_P;
  return 1;
}

const int N_GRAPH_SYMBOL = 32;
#define UL_SYMBOL(x) (N_GRAPH_SYMBOL + (x))
enum UlSymbolType {
  UL_SYMBOL_DISC = 32 + 9,
  UL_SYMBOL_CIRCLE = 32 + 10,
  UL_SYMBOL_SQUARE = 32 + 11,
};

int HtmlTag::HTML_LI_enter(html_feed_environ *h_env) {
  h_env->CLOSE_A();
  h_env->CLOSE_DT();
  if (h_env->envc > 0) {
    h_env->flushline();
    h_env->envs[h_env->envc].count++;
    if (auto value = this->getAttr(ATTR_VALUE)) {
      int count = stoi(*value);
      if (count > 0)
        h_env->envs[h_env->envc].count = count;
      else
        h_env->envs[h_env->envc].count = 0;
    }

    std::string num;
    switch (h_env->envs[h_env->envc].env) {
    case HTML_UL: {
      // h_env->envs[h_env->envc].type =
      // h_env->ul_type(h_env->envs[h_env->envc].type);
      //  int HtmlTag::ul_type(int default_type) const {
      //    if (auto value = this->getAttr(ATTR_TYPE)) {
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
        h_env->push_charp(1, NBSP, PC_ASCII);
      std::stringstream tmp;
      switch (h_env->envs[h_env->envc].type) {
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
        push_symbol(tmp, UL_SYMBOL((h_env->envc_real - 1) % MAX_UL_LEVEL), 1,
                    1);
        break;
      }
      h_env->push_charp(1, NBSP, PC_ASCII);
      h_env->push_str(1, tmp.str(), PC_ASCII);
      h_env->push_charp(1, NBSP, PC_ASCII);
      h_env->prevchar = " ";
      break;
    }

    case HTML_OL:
      if (auto value = this->getAttr(ATTR_TYPE))
        h_env->envs[h_env->envc].type = (*value)[0];
      switch ((h_env->envs[h_env->envc].count > 0)
                  ? h_env->envs[h_env->envc].type
                  : '1') {
      case 'i':
        num = romanNumeral(h_env->envs[h_env->envc].count);
        break;
      case 'I':
        num = romanNumeral(h_env->envs[h_env->envc].count);
        Strupper(num);
        break;
      case 'a':
        num = romanAlphabet(h_env->envs[h_env->envc].count);
        break;
      case 'A':
        num = romanAlphabet(h_env->envs[h_env->envc].count);
        Strupper(num);
        break;
      default: {
        std::stringstream ss;
        ss << h_env->envs[h_env->envc].count;
        num = ss.str();
        break;
      }
      }
      if (INDENT_INCR >= 4)
        num += ". ";
      else
        num.push_back('.');
      h_env->push_spaces(1, INDENT_INCR - num.size());
      h_env->push_str(num.size(), num, PC_ASCII);
      if (INDENT_INCR >= 4)
        h_env->prevchar = " ";
      break;
    default:
      h_env->push_spaces(1, INDENT_INCR);
      break;
    }
  } else {
    h_env->flushline();
  }
  h_env->flag |= RB_IGNORE_P;
  return 1;
}

int HtmlTag::HTML_DT_enter(html_feed_environ *h_env) {
  h_env->CLOSE_A();
  if (h_env->envc == 0 || (h_env->envc_real < (int)h_env->envs.size() &&
                           h_env->envs[h_env->envc].env != HTML_DL &&
                           h_env->envs[h_env->envc].env != HTML_DL_COMPACT)) {
    h_env->PUSH_ENV_NOINDENT(HTML_DL);
  }
  if (h_env->envc > 0) {
    h_env->flushline();
  }
  if (!(h_env->flag & RB_IN_DT)) {
    h_env->parse("<b>");
    h_env->flag |= RB_IN_DT;
  }
  h_env->flag |= RB_IGNORE_P;
  return 1;
}

int HtmlTag::HTML_DT_exit(html_feed_environ *h_env) {
  if (!(h_env->flag & RB_IN_DT)) {
    return 1;
  }
  h_env->flag &= ~RB_IN_DT;
  h_env->parse("</b>");
  if (h_env->envc > 0 && h_env->envs[h_env->envc].env == HTML_DL) {
    h_env->flushline();
  }
  return 1;
}

int HtmlTag::HTML_DD_enter(html_feed_environ *h_env) {
  h_env->CLOSE_A();
  h_env->CLOSE_DT();
  if (h_env->envs[h_env->envc].env == HTML_DL ||
      h_env->envs[h_env->envc].env == HTML_DL_COMPACT) {
    h_env->PUSH_ENV(HTML_DD);
  }

  if (h_env->envc > 0 && h_env->envs[h_env->envc - 1].env == HTML_DL_COMPACT) {
    if (h_env->pos > h_env->envs[h_env->envc].indent) {
      h_env->flushline();
    } else {
      h_env->push_spaces(1, h_env->envs[h_env->envc].indent - h_env->pos);
    }
  } else {
    h_env->flushline();
  }
  /* h_env->flag |= RB_IGNORE_P; */
  return 1;
}

int HtmlTag::HTML_TITLE_enter(html_feed_environ *h_env) {
  h_env->close_anchor();
  h_env->process_title();
  h_env->flag |= RB_TITLE;
  h_env->end_tag = HTML_N_TITLE;
  return 1;
}

int HtmlTag::HTML_TITLE_exit(html_feed_environ *h_env) {
  if (!(h_env->flag & RB_TITLE))
    return 1;
  h_env->flag &= ~RB_TITLE;
  h_env->end_tag = {};
  auto tmp = h_env->process_n_title();
  if (tmp.size())
    h_env->parse(tmp);
  return 1;
}

int HtmlTag::HTML_TITLE_ALT_enter(html_feed_environ *h_env) {
  if (auto value = this->getAttr(ATTR_TITLE))
    h_env->title = html_unquote(*value);
  return 0;
}

int HtmlTag::HTML_FRAMESET_enter(html_feed_environ *h_env) {
  h_env->PUSH_ENV(this->tagid);
  h_env->push_charp(9, "--FRAME--", PC_ASCII);
  h_env->flushline();
  return 0;
}

int HtmlTag::HTML_FRAMESET_exit(html_feed_environ *h_env) {
  if (h_env->envc > 0) {
    h_env->POP_ENV();
    h_env->flushline();
  }
  return 0;
}

int HtmlTag::HTML_NOFRAMES_enter(html_feed_environ *h_env) {
  h_env->CLOSE_A();
  h_env->flushline();
  h_env->flag |= (RB_NOFRAMES | RB_IGNORE_P);
  /* istr = str; */
  return 1;
}

int HtmlTag::HTML_NOFRAMES_exit(html_feed_environ *h_env) {
  h_env->CLOSE_A();
  h_env->flushline();
  h_env->flag &= ~RB_NOFRAMES;
  return 1;
}

int HtmlTag::HTML_FRAME_enter(html_feed_environ *h_env) {
  std::string q;
  if (auto value = this->getAttr(ATTR_SRC))
    q = *value;
  std::string r;
  if (auto value = this->getAttr(ATTR_NAME))
    r = *value;
  if (q.size()) {
    q = html_quote(q);
    std::stringstream ss;
    ss << "<a hseq=\"" << h_env->cur_hseq++ << "\" href=\"" << q << "\">";
    h_env->push_tag(ss.str(), HTML_A);
    if (r.size())
      q = html_quote(r);
    h_env->push_charp(get_strwidth(q), q.c_str(), PC_ASCII);
    h_env->push_tag("</a>", HTML_N_A);
  }
  h_env->flushline();
  return 0;
}

int HtmlTag::HTML_HR_enter(html_feed_environ *h_env) {
  h_env->close_anchor();
  auto tmp =
      h_env->process_hr(this, h_env->_width, h_env->envs[h_env->envc].indent);
  h_env->parse(tmp);
  h_env->prevchar = " ";
  return 1;
}

int HtmlTag::HTML_PRE_enter(html_feed_environ *h_env) {
  int x = this->existsAttr(ATTR_FOR_TABLE);
  h_env->CLOSE_A();
  if (!(h_env->flag & RB_IGNORE_P)) {
    h_env->flushline();
    if (!x) {
      h_env->do_blankline();
    }
  } else {
    h_env->fillline();
  }
  h_env->flag |= (RB_PRE | RB_IGNORE_P);
  /* istr = str; */
  return 1;
}

int HtmlTag::HTML_PRE_exit(html_feed_environ *h_env) {
  h_env->flushline();
  if (!(h_env->flag & RB_IGNORE_P)) {
    h_env->do_blankline();
    h_env->flag |= RB_IGNORE_P;
    h_env->blank_lines++;
  }
  h_env->flag &= ~RB_PRE;
  h_env->close_anchor();
  return 1;
}

int HtmlTag::HTML_PRE_PLAIN_enter(html_feed_environ *h_env) {
  h_env->CLOSE_A();
  if (!(h_env->flag & RB_IGNORE_P)) {
    h_env->flushline();
    h_env->do_blankline();
  }
  h_env->flag |= (RB_PRE | RB_IGNORE_P);
  return 1;
}

int HtmlTag::HTML_PRE_PLAIN_exit(html_feed_environ *h_env) {
  h_env->CLOSE_A();
  if (!(h_env->flag & RB_IGNORE_P)) {
    h_env->flushline();
    h_env->do_blankline();
    h_env->flag |= RB_IGNORE_P;
  }
  h_env->flag &= ~RB_PRE;
  return 1;
}

int HtmlTag::HTML_PLAINTEXT_enter(html_feed_environ *h_env) {
  h_env->CLOSE_A();
  if (!(h_env->flag & RB_IGNORE_P)) {
    h_env->flushline();
    h_env->do_blankline();
  }
  h_env->flag |= (RB_PLAIN | RB_IGNORE_P);
  switch (this->tagid) {
  case HTML_LISTING:
    h_env->end_tag = HTML_N_LISTING;
    break;
  case HTML_XMP:
    h_env->end_tag = HTML_N_XMP;
    break;
  case HTML_PLAINTEXT:
    h_env->end_tag = MAX_HTMLTAG;
    break;

  default:
    break;
  }
  return 1;
}

int HtmlTag::HTML_LISTING_exit(html_feed_environ *h_env) {
  h_env->CLOSE_A();
  if (!(h_env->flag & RB_IGNORE_P)) {
    h_env->flushline();
    h_env->do_blankline();
    h_env->flag |= RB_IGNORE_P;
  }
  h_env->flag &= ~RB_PLAIN;
  h_env->end_tag = {};
  return 1;
}

int HtmlTag::HTML_A_enter(html_feed_environ *h_env) {
  if (h_env->anchor.url.size()) {
    h_env->close_anchor();
  }

  if (auto value = this->getAttr(ATTR_HREF))
    h_env->anchor.url = *value;
  if (auto value = this->getAttr(ATTR_TARGET))
    h_env->anchor.target = *value;
  if (auto value = this->getAttr(ATTR_REFERER))
    h_env->anchor.option = {.referer = *value};
  if (auto value = this->getAttr(ATTR_TITLE))
    h_env->anchor.title = *value;
  if (auto value = this->getAttr(ATTR_ACCESSKEY))
    h_env->anchor.accesskey = (*value)[0];
  if (auto value = this->getAttr(ATTR_HSEQ))
    h_env->anchor.hseq = stoi(*value);

  if (h_env->anchor.hseq == 0 && h_env->anchor.url.size()) {
    h_env->anchor.hseq = h_env->cur_hseq;
    auto tmp = h_env->process_anchor(this, h_env->tagbuf);
    h_env->push_tag(tmp.c_str(), HTML_A);
    return 1;
  }
  return 0;
}

int HtmlTag::HTML_IMG_enter(html_feed_environ *h_env) {
  if (this->existsAttr(ATTR_USEMAP))
    h_env->HTML5_CLOSE_A();
  auto tmp = h_env->process_img(this, h_env->_width);
  h_env->parse(tmp);
  return 1;
}

int HtmlTag::HTML_IMG_ALT_enter(html_feed_environ *h_env) {
  if (auto value = this->getAttr(ATTR_SRC))
    h_env->img_alt = *value;
  return 0;
}

int HtmlTag::HTML_IMG_ALT_exit(html_feed_environ *h_env) {
  if (h_env->img_alt.size()) {
    if (!h_env->close_effect0(HTML_IMG_ALT))
      h_env->push_tag("</img_alt>", HTML_N_IMG_ALT);
    h_env->img_alt = {};
  }
  return 1;
}

int HtmlTag::HTML_TABLE_enter(html_feed_environ *h_env) {
  int cols = h_env->_width;
  h_env->close_anchor();
  if (h_env->table_level + 1 >= MAX_TABLE) {
    return -1;
  }
  h_env->table_level++;

  HtmlTableBorderMode w = BORDER_NONE;
  /* x: cellspacing, y: cellpadding */
  int width = 0;
  if (this->existsAttr(ATTR_BORDER)) {
    if (auto value = this->getAttr(ATTR_BORDER)) {
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
  if (auto value = this->getAttr(ATTR_WIDTH)) {
    if (h_env->table_level == 0)
      width = REAL_WIDTH(stoi(*value),
                         h_env->_width - h_env->envs[h_env->envc].indent);
    else
      width = RELATIVE_WIDTH(stoi(*value));
  }
  if (this->existsAttr(ATTR_HBORDER))
    w = BORDER_NOWIN;

  int x = 2;
  if (auto value = this->getAttr(ATTR_CELLSPACING)) {
    x = stoi(*value);
  }
  int y = 1;
  if (auto value = this->getAttr(ATTR_CELLPADDING)) {
    y = stoi(*value);
  }
  int z = 0;
  if (auto value = this->getAttr(ATTR_VSPACE)) {
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

  h_env->tables[h_env->table_level] =
      table::begin_table(w, x, y, z, cols, width);
  h_env->table_mode[h_env->table_level].pre_mode = {};
  h_env->table_mode[h_env->table_level].indent_level = 0;
  h_env->table_mode[h_env->table_level].nobr_level = 0;
  h_env->table_mode[h_env->table_level].caption = 0;
  h_env->table_mode[h_env->table_level].end_tag = HTML_UNKNOWN;
  return 1;
}

int HtmlTag::HTML_CENTER_enter(html_feed_environ *h_env) {
  h_env->CLOSE_A();
  if (!(h_env->flag & (RB_PREMODE | RB_IGNORE_P)))
    h_env->flushline();
  h_env->RB_SAVE_FLAG();
  if (DisableCenter) {
    h_env->RB_SET_ALIGN(RB_LEFT);
  } else {
    h_env->RB_SET_ALIGN(RB_CENTER);
  }
  return 1;
}

int HtmlTag::HTML_CENTER_exit(html_feed_environ *h_env) {
  h_env->CLOSE_A();
  if (!(h_env->flag & RB_PREMODE)) {
    h_env->flushline();
  }
  h_env->RB_RESTORE_FLAG();
  return 1;
}

int HtmlTag::HTML_DIV_enter(html_feed_environ *h_env) {
  h_env->CLOSE_A();
  if (!(h_env->flag & RB_IGNORE_P)) {
    h_env->flushline();
  }
  h_env->set_alignment(this->alignFlag());
  return 1;
}

int HtmlTag::HTML_DIV_exit(html_feed_environ *h_env) {
  h_env->CLOSE_A();
  h_env->flushline();
  h_env->RB_RESTORE_FLAG();
  return 1;
}

int HtmlTag::HTML_DIV_INT_enter(html_feed_environ *h_env) {
  h_env->CLOSE_P();
  if (!(h_env->flag & RB_IGNORE_P)) {
    h_env->flushline();
  }
  h_env->set_alignment(this->alignFlag());
  return 1;
}

int HtmlTag::HTML_DIV_INT_exit(html_feed_environ *h_env) {
  h_env->CLOSE_P();
  h_env->flushline();
  h_env->RB_RESTORE_FLAG();
  return 1;
}

int HtmlTag::HTML_FORM_enter(html_feed_environ *h_env) {
  h_env->CLOSE_A();
  if (!(h_env->flag & RB_IGNORE_P)) {
    h_env->flushline();
  }
  auto tmp = h_env->process_form(this);
  if (tmp.size())
    h_env->parse(tmp);
  return 1;
}

int HtmlTag::HTML_FORM_exit(html_feed_environ *h_env) {
  h_env->CLOSE_A();
  h_env->flushline();
  h_env->flag |= RB_IGNORE_P;
  h_env->process_n_form();
  return 1;
}

int HtmlTag::HTML_INPUT_enter(html_feed_environ *h_env) {
  h_env->close_anchor();
  auto tmp = h_env->process_input(this);
  if (tmp.size())
    h_env->parse(tmp);
  return 1;
}

int HtmlTag::HTML_BUTTON_enter(html_feed_environ *h_env) {
  h_env->HTML5_CLOSE_A();
  auto tmp = h_env->process_button(this);
  if (tmp.size())
    h_env->parse(tmp);
  return 1;
}

int HtmlTag::HTML_BUTTON_exit(html_feed_environ *h_env) {
  auto tmp = h_env->process_n_button();
  if (tmp.size())
    h_env->parse(tmp);
  return 1;
}

int HtmlTag::HTML_SELECT_enter(html_feed_environ *h_env) {
  h_env->close_anchor();
  auto tmp = h_env->process_select(this);
  if (tmp.size())
    h_env->parse(tmp);
  h_env->flag |= RB_INSELECT;
  h_env->end_tag = HTML_N_SELECT;
  return 1;
}

int HtmlTag::HTML_SELECT_exit(html_feed_environ *h_env) {
  h_env->flag &= ~RB_INSELECT;
  h_env->end_tag = HTML_UNKNOWN;
  auto tmp = h_env->process_n_select();
  if (tmp.size())
    h_env->parse(tmp);
  return 1;
}

int HtmlTag::HTML_TEXTAREA_enter(html_feed_environ *h_env) {
  h_env->close_anchor();
  auto tmp = h_env->process_textarea(this, h_env->_width);
  if (tmp.size()) {
    h_env->parse(tmp);
  }
  h_env->flag |= RB_INTXTA;
  h_env->end_tag = HTML_N_TEXTAREA;
  return 1;
}

int HtmlTag::HTML_TEXTAREA_exit(html_feed_environ *h_env) {
  h_env->flag &= ~RB_INTXTA;
  h_env->end_tag = HTML_UNKNOWN;
  auto tmp = h_env->process_n_textarea();
  if (tmp.size()) {
    h_env->parse(tmp);
  }
  return 1;
}

int HtmlTag::HTML_ISINDEX_enter(html_feed_environ *h_env) {
  std::string p = "";
  if (auto value = this->getAttr(ATTR_PROMPT)) {
    p = *value;
  }
  std::string q = "!CURRENT_URL!";
  if (auto value = this->getAttr(ATTR_ACTION)) {
    q = *value;
  }
  std::stringstream tmp;
  tmp << "<form method=get action=\"" << html_quote(q) << "\">" << html_quote(p)
      << "<input type=text name=\"\" accept></form>";
  h_env->parse(tmp.str());
  return 1;
}

int HtmlTag::HTML_META_enter(html_feed_environ *h_env) {
  std::string p;
  if (auto value = this->getAttr(ATTR_HTTP_EQUIV)) {
    p = *value;
  }
  std::string q;
  if (auto value = this->getAttr(ATTR_CONTENT)) {
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
      h_env->parse(tmp);
      h_env->do_blankline();
    }
  }
  return 1;
}

int HtmlTag::HTML_DEL_enter(html_feed_environ *h_env) {
  switch (displayInsDel) {
  case DISPLAY_INS_DEL_SIMPLE:
    h_env->flag |= RB_DEL;
    break;
  case DISPLAY_INS_DEL_NORMAL:
    h_env->parse("<U>[DEL:</U>");
    break;
  case DISPLAY_INS_DEL_FONTIFY:
    if (h_env->fontstat.in_strike < FONTSTAT_MAX)
      h_env->fontstat.in_strike++;
    if (h_env->fontstat.in_strike == 1) {
      h_env->push_tag("<s>", HTML_S);
    }
    break;
  }
  return 1;
}

int HtmlTag::HTML_DEL_exit(html_feed_environ *h_env) {
  switch (displayInsDel) {
  case DISPLAY_INS_DEL_SIMPLE:
    h_env->flag &= ~RB_DEL;
    break;
  case DISPLAY_INS_DEL_NORMAL:
    h_env->parse("<U>:DEL]</U>");
  case DISPLAY_INS_DEL_FONTIFY:
    if (h_env->fontstat.in_strike == 0)
      return 1;
    if (h_env->fontstat.in_strike == 1 && h_env->close_effect0(HTML_S))
      h_env->fontstat.in_strike = 0;
    if (h_env->fontstat.in_strike > 0) {
      h_env->fontstat.in_strike--;
      if (h_env->fontstat.in_strike == 0) {
        h_env->push_tag("</s>", HTML_N_S);
      }
    }
    break;
  }
  return 1;
}

int HtmlTag::HTML_S_enter(html_feed_environ *h_env) {
  switch (displayInsDel) {
  case DISPLAY_INS_DEL_SIMPLE:
    h_env->flag |= RB_S;
    break;
  case DISPLAY_INS_DEL_NORMAL:
    h_env->parse("<U>[S:</U>");
    break;
  case DISPLAY_INS_DEL_FONTIFY:
    if (h_env->fontstat.in_strike < FONTSTAT_MAX)
      h_env->fontstat.in_strike++;
    if (h_env->fontstat.in_strike == 1) {
      h_env->push_tag("<s>", HTML_S);
    }
    break;
  }
  return 1;
}

int HtmlTag::HTML_S_exit(html_feed_environ *h_env) {
  switch (displayInsDel) {
  case DISPLAY_INS_DEL_SIMPLE:
    h_env->flag &= ~RB_S;
    break;
  case DISPLAY_INS_DEL_NORMAL:
    h_env->parse("<U>:S]</U>");
    break;
  case DISPLAY_INS_DEL_FONTIFY:
    if (h_env->fontstat.in_strike == 0)
      return 1;
    if (h_env->fontstat.in_strike == 1 && h_env->close_effect0(HTML_S))
      h_env->fontstat.in_strike = 0;
    if (h_env->fontstat.in_strike > 0) {
      h_env->fontstat.in_strike--;
      if (h_env->fontstat.in_strike == 0) {
        h_env->push_tag("</s>", HTML_N_S);
      }
    }
  }
  return 1;
}

int HtmlTag::HTML_INS_enter(html_feed_environ *h_env) {
  switch (displayInsDel) {
  case DISPLAY_INS_DEL_SIMPLE:
    break;
  case DISPLAY_INS_DEL_NORMAL:
    h_env->parse("<U>[INS:</U>");
    break;
  case DISPLAY_INS_DEL_FONTIFY:
    if (h_env->fontstat.in_ins < FONTSTAT_MAX)
      h_env->fontstat.in_ins++;
    if (h_env->fontstat.in_ins == 1) {
      h_env->push_tag("<ins>", HTML_INS);
    }
    break;
  }
  return 1;
}

int HtmlTag::HTML_INS_exit(html_feed_environ *h_env) {
  switch (displayInsDel) {
  case DISPLAY_INS_DEL_SIMPLE:
    break;
  case DISPLAY_INS_DEL_NORMAL:
    h_env->parse("<U>:INS]</U>");
    break;
  case DISPLAY_INS_DEL_FONTIFY:
    if (h_env->fontstat.in_ins == 0)
      return 1;
    if (h_env->fontstat.in_ins == 1 && h_env->close_effect0(HTML_INS))
      h_env->fontstat.in_ins = 0;
    if (h_env->fontstat.in_ins > 0) {
      h_env->fontstat.in_ins--;
      if (h_env->fontstat.in_ins == 0) {
        h_env->push_tag("</ins>", HTML_N_INS);
      }
    }
    break;
  }
  return 1;
}

int HtmlTag::HTML_BGSOUND_enter(html_feed_environ *h_env) {
  if (view_unseenobject) {
    if (auto value = this->getAttr(ATTR_SRC)) {
      auto q = html_quote(*value);
      std::stringstream s;
      s << "<A HREF=\"" << q << "\">bgsound(" << q << ")</A>";
      h_env->parse(s.str());
    }
  }
  return 1;
}

int HtmlTag::HTML_EMBED_enter(html_feed_environ *h_env) {
  h_env->HTML5_CLOSE_A();
  if (view_unseenobject) {
    if (auto value = this->getAttr(ATTR_SRC)) {
      auto q = html_quote(*value);
      std::stringstream s;
      s << "<A HREF=\"" << q << "\">embed(" << q << ")</A>";
      h_env->parse(s.str());
    }
  }
  return 1;
}

int HtmlTag::HTML_APPLET_enter(html_feed_environ *h_env) {
  if (view_unseenobject) {
    if (auto value = this->getAttr(ATTR_ARCHIVE)) {
      auto q = html_quote(*value);
      std::stringstream s;
      s << "<A HREF=\"" << q << "\">applet archive(" << q << ")</A>";
      h_env->parse(s.str());
    }
  }
  return 1;
}

int HtmlTag::HTML_BODY_enter(html_feed_environ *h_env) {
  if (view_unseenobject) {
    if (auto value = this->getAttr(ATTR_BACKGROUND)) {
      auto q = html_quote(*value);
      std::stringstream s;
      s << "<IMG SRC=\"" << q << "\" ALT=\"bg image(" << q << ")\"><BR>";
      h_env->parse(s.str());
    }
  }
  return 1;
}

int HtmlTag::HTML_INPUT_ALT_enter(html_feed_environ *h_env) {
  if (auto value = this->getAttr(ATTR_TOP_MARGIN)) {
    int i = stoi(*value);
    if ((short)i > h_env->top_margin)
      h_env->top_margin = (short)i;
  }
  if (auto value = this->getAttr(ATTR_BOTTOM_MARGIN)) {
    int i = stoi(*value);
    if ((short)i > h_env->bottom_margin)
      h_env->bottom_margin = (short)i;
  }
  if (auto value = this->getAttr(ATTR_HSEQ)) {
    h_env->input_alt.hseq = stoi(*value);
  }
  if (auto value = this->getAttr(ATTR_FID)) {
    h_env->input_alt.fid = stoi(*value);
  }
  if (auto value = this->getAttr(ATTR_TYPE)) {
    h_env->input_alt.type = *value;
  }
  if (auto value = this->getAttr(ATTR_VALUE)) {
    h_env->input_alt.value = *value;
  }
  if (auto value = this->getAttr(ATTR_NAME)) {
    h_env->input_alt.name = *value;
  }
  h_env->input_alt.in = 1;
  return 0;
}

int HtmlTag::HTML_INPUT_ALT_exit(html_feed_environ *h_env) {
  if (h_env->input_alt.in) {
    if (!h_env->close_effect0(HTML_INPUT_ALT))
      h_env->push_tag("</input_alt>", HTML_N_INPUT_ALT);
    h_env->input_alt.hseq = 0;
    h_env->input_alt.fid = -1;
    h_env->input_alt.in = 0;
    h_env->input_alt.type = {};
    h_env->input_alt.name = {};
    h_env->input_alt.value = {};
  }
  return 1;
}

int HtmlTag::HTML_B_enter(html_feed_environ *h_env) {
  if (h_env->fontstat.in_bold < FONTSTAT_MAX)
    h_env->fontstat.in_bold++;
  if (h_env->fontstat.in_bold > 1)
    return 1;
  return 0;
}

int HtmlTag::HTML_B_exit(html_feed_environ *h_env) {
  if (h_env->fontstat.in_bold == 1 && h_env->close_effect0(HTML_B))
    h_env->fontstat.in_bold = 0;
  if (h_env->fontstat.in_bold > 0) {
    h_env->fontstat.in_bold--;
    if (h_env->fontstat.in_bold == 0)
      return 0;
  }
  return 1;
}

int HtmlTag::HTML_I_enter(html_feed_environ *h_env) {
  if (h_env->fontstat.in_italic < FONTSTAT_MAX)
    h_env->fontstat.in_italic++;
  if (h_env->fontstat.in_italic > 1)
    return 1;
  return 0;
}

int HtmlTag::HTML_I_exit(html_feed_environ *h_env) {
  if (h_env->fontstat.in_italic == 1 && h_env->close_effect0(HTML_I))
    h_env->fontstat.in_italic = 0;
  if (h_env->fontstat.in_italic > 0) {
    h_env->fontstat.in_italic--;
    if (h_env->fontstat.in_italic == 0)
      return 0;
  }
  return 1;
}

int HtmlTag::HTML_U_enter(html_feed_environ *h_env) {
  if (h_env->fontstat.in_under < FONTSTAT_MAX)
    h_env->fontstat.in_under++;
  if (h_env->fontstat.in_under > 1)
    return 1;
  return 0;
}

int HtmlTag::HTML_U_exit(html_feed_environ *h_env) {
  if (h_env->fontstat.in_under == 1 && h_env->close_effect0(HTML_U))
    h_env->fontstat.in_under = 0;
  if (h_env->fontstat.in_under > 0) {
    h_env->fontstat.in_under--;
    if (h_env->fontstat.in_under == 0)
      return 0;
  }
  return 1;
}

int HtmlTag::HTML_PRE_INT_enter(html_feed_environ *h_env) {
  int i = h_env->line.size();
  h_env->append_tags();
  if (!(h_env->flag & RB_SPECIAL)) {
    h_env->set_breakpoint(h_env->line.size() - i);
  }
  h_env->flag |= RB_PRE_INT;
  return 0;
}

int HtmlTag::HTML_PRE_INT_exit(html_feed_environ *h_env) {
  h_env->push_tag("</pre_int>", HTML_N_PRE_INT);
  h_env->flag &= ~RB_PRE_INT;
  if (!(h_env->flag & RB_SPECIAL) && h_env->pos > h_env->bp.pos) {
    h_env->prevchar = "";
    h_env->prev_ctype = PC_CTRL;
  }
  return 1;
}

int HtmlTag::HTML_NOBR_enter(html_feed_environ *h_env) {
  h_env->flag |= RB_NOBR;
  h_env->nobr_level++;
  return 0;
}

int HtmlTag::HTML_NOBR_exit(html_feed_environ *h_env) {
  if (h_env->nobr_level > 0)
    h_env->nobr_level--;
  if (h_env->nobr_level == 0)
    h_env->flag &= ~RB_NOBR;
  return 0;
}
