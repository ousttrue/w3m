#include "html_parser.h"
#include "cmp.h"
#include "push_symbol.h"
#include "option_param.h"
#include "html_feed_env.h"
#include "entity.h"
#include "anchorlist.h"
#include "line_data.h"
#include "symbol.h"
#include "url_quote.h"
#include "form.h"
#include "html_tag.h"
#include "html_tag_parse.h"
#include "html_token.h"
#include "quote.h"
#include "ctrlcode.h"
#include "myctype.h"
#include "table.h"
#include "utf8.h"
#include "cmp.h"
#include "readtoken.h"
#include <sstream>

#define HR_ATTR_WIDTH_MAX 65535
#define MAX_INPUT_SIZE 80 /* TODO - max should be screen line length */

#define IMG_SYMBOL 32 + (12)
#define HR_SYMBOL 26
#define TEXTAREA_ATTR_COL_MAX 4096
#define TEXTAREA_ATTR_ROWS_MAX 4096
#define INITIAL_FORM_SIZE 10

HtmlParser::HtmlParser(int width) : _width(width) {}

void HtmlParser::HTMLlineproc1(const std::string &x,
                               struct html_feed_environ *y) {
  parse(x, y, true);
}

void HtmlParser::CLOSE_DT(html_feed_environ *h_env) {
  if (h_env->flag & RB_IN_DT) {
    h_env->flag &= ~RB_IN_DT;
    HTMLlineproc1("</b>", h_env);
  }
}

void HtmlParser::CLOSE_A(html_feed_environ *h_env) {
  do {
    h_env->CLOSE_P(h_env);
    if (!(h_env->flag & RB_HTML5)) {
      this->close_anchor(h_env);
    }
  } while (0);
}

void HtmlParser::HTML5_CLOSE_A(html_feed_environ *h_env) {
  do {
    if (h_env->flag & RB_HTML5) {
      this->close_anchor(h_env);
    }
  } while (0);
}

std::string HtmlParser::getLinkNumberStr(int correction) const {
  std::stringstream ss;
  ss << "[" << (cur_hseq + correction) << "]";
  return ss.str();
}

void HtmlParser::push_render_image(const std::string &str, int width, int limit,
                                   struct html_feed_environ *h_env) {
  int indent = h_env->envs[h_env->envc].indent;

  h_env->push_spaces(1, (limit - width) / 2);
  h_env->push_str(width, str, PC_ASCII);
  h_env->push_spaces(1, (limit - width + 1) / 2);
  if (width > 0)
    h_env->flushline(h_env->buf, indent, 0, h_env->limit);
}

void HtmlParser::close_anchor(struct html_feed_environ *h_env) {
  if (h_env->anchor.url.size()) {
    const char *p = nullptr;
    int is_erased = 0;

    auto found = h_env->find_stack([](auto tag) { return tag->cmd == HTML_A; });

    if (found == h_env->tag_stack.end() && h_env->anchor.hseq > 0 &&
        h_env->line.back() == ' ') {
      Strshrink(h_env->line, 1);
      h_env->pos--;
      is_erased = 1;
    }

    if (found != h_env->tag_stack.end() ||
        (p = h_env->has_hidden_link(HTML_A))) {
      if (h_env->anchor.hseq > 0) {
        this->HTMLlineproc1(ANSP, h_env);
        h_env->prevchar = " ";
      } else {
        if (found != h_env->tag_stack.end()) {
          h_env->tag_stack.erase(found);
        } else {
          h_env->passthrough(p, 1);
        }
        h_env->anchor = {};
        return;
      }
      is_erased = 0;
    }
    if (is_erased) {
      h_env->line.push_back(' ');
      h_env->pos++;
    }

    h_env->push_tag("</a>", HTML_N_A);
  }
  h_env->anchor = {};
}

void HtmlParser::save_fonteffect(struct html_feed_environ *h_env) {
  if (h_env->fontstat_sp < FONT_STACK_SIZE) {
    h_env->fontstat_stack[h_env->fontstat_sp] = h_env->fontstat;
  }
  if (h_env->fontstat_sp < INT_MAX) {
    h_env->fontstat_sp++;
  }

  if (h_env->fontstat.in_bold)
    h_env->push_tag("</b>", HTML_N_B);
  if (h_env->fontstat.in_italic)
    h_env->push_tag("</i>", HTML_N_I);
  if (h_env->fontstat.in_under)
    h_env->push_tag("</u>", HTML_N_U);
  if (h_env->fontstat.in_strike)
    h_env->push_tag("</s>", HTML_N_S);
  if (h_env->fontstat.in_ins)
    h_env->push_tag("</ins>", HTML_N_INS);
  h_env->fontstat = {};
}

void HtmlParser::restore_fonteffect(struct html_feed_environ *h_env) {
  if (h_env->fontstat_sp > 0)
    h_env->fontstat_sp--;
  if (h_env->fontstat_sp < FONT_STACK_SIZE) {
    h_env->fontstat = h_env->fontstat_stack[h_env->fontstat_sp];
  }

  if (h_env->fontstat.in_bold)
    h_env->push_tag("<b>", HTML_B);
  if (h_env->fontstat.in_italic)
    h_env->push_tag("<i>", HTML_I);
  if (h_env->fontstat.in_under)
    h_env->push_tag("<u>", HTML_U);
  if (h_env->fontstat.in_strike)
    h_env->push_tag("<s>", HTML_S);
  if (h_env->fontstat.in_ins)
    h_env->push_tag("<ins>", HTML_INS);
}

std::string HtmlParser::process_textarea(const HtmlTag *tag, int width) {
  std::stringstream tmp;

  if (cur_form_id() < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp << process_form(parseHtmlTag(&s, true).get());
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

std::string HtmlParser::process_n_textarea() {
  if (cur_textarea.empty()) {
    return nullptr;
  }

  std::stringstream tmp;
  tmp << "<pre_int>[<input_alt hseq=\"" << this->cur_hseq << "\" fid=\""
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
  this->cur_hseq++;
  textarea_str.push_back({});
  a_textarea.push_back({});
  n_textarea++;
  cur_textarea = nullptr;

  return tmp.str();
}

void HtmlParser::feed_textarea(const std::string &_str) {
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

std::string HtmlParser::process_form_int(const HtmlTag *tag, int fid) {
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

std::string HtmlParser::process_n_form(void) {
  if (form_sp >= 0)
    form_sp--;
  return {};
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

std::shared_ptr<LineData>
HtmlParser::render(const Url &currentUrl, html_feed_environ *h_env,
                   const std::shared_ptr<AnchorList<FormAnchor>> &old) {

  auto formated = std::make_shared<LineData>();
  formated->clear(_width);
  formated->baseURL = currentUrl;
  formated->title = h_env->title;

  LineFeed feed(h_env->buf);
  for (int nlines = 0; auto _str = feed.textlist_feed(); ++nlines) {
    auto &str = *_str;
    if (n_textarea >= 0 && str.size() && str[0] != '<') {
      textarea_str[n_textarea] += str;
      continue;
    }

    auto line =
        renderLine(currentUrl, h_env, formated, nlines, str.c_str(), old);

    /* end of processing for one line */
    if (!internal) {
      line.PPUSH(0, 0);
      formated->lines.push_back(line);
    }
    if (internal == HTML_N_INTERNAL) {
      internal = {};
    }
  }

  formated->formlist = forms;
  if (n_textarea) {
    formated->addMultirowsForm();
  }

  return formated;
}

Line HtmlParser::renderLine(
    const Url &url, html_feed_environ *h_env,
    const std::shared_ptr<LineData> &data, int nlines, const char *str,
    const std::shared_ptr<AnchorList<FormAnchor>> &old) {

  Line line;

  auto endp = str + strlen(str);
  while (str < endp) {
    auto mode = get_mctype(str);
    if ((effect | ex_efct(ex_effect)) & PC_SYMBOL && *str != '<') {
      line.PPUSH(PC_ASCII | effect | ex_efct(ex_effect), SYMBOL_BASE + symbol);
      str += 1;
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
      if (!(tag = parseHtmlTag(&str, true)))
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
        // const char *s = nullptr;
        // auto q = res->baseTarget;
        // char *id = nullptr;
        if (auto value = tag->getAttr(ATTR_NAME)) {
          auto _id = url_quote(*value);
          data->registerName(_id.c_str(), nlines, line.len());
        }
        std::string p;
        if (auto value = tag->getAttr(ATTR_HREF)) {
          p = url_quote(remove_space(*value));
        }
        std::string q;
        if (auto value = tag->getAttr(ATTR_TARGET)) {
          q = url_quote(*value);
        }
        std::string r;
        if (auto value = tag->getAttr(ATTR_REFERER)) {
          r = url_quote(*value);
        }
        std::string s;
        if (auto value = tag->getAttr(ATTR_TITLE)) {
          s = *value;
        }
        std::string t;
        if (auto value = tag->getAttr(ATTR_ACCESSKEY)) {
          t = *value;
        } else {
          t.push_back(0);
        }
        auto hseq = 0;
        if (auto value = tag->getAttr(ATTR_HSEQ)) {
          hseq = stoi(*value);
        }
        if (hseq > 0) {
          data->_hmarklist->putHmarker(nlines, line.len(), hseq - 1);
        } else if (hseq < 0) {
          int h = -hseq - 1;
          if (data->_hmarklist->tryMark(h, nlines, line.len())) {
            hseq = -hseq;
          }
        }
        if (p.size()) {
          effect |= PE_ANCHOR;
          auto bp = BufferPoint{
              .line = nlines,
              .pos = line.len(),
          };
          a_href = data->_href->putAnchor(Anchor{
              .url = p,
              .target = q,
              .option = {.referer = r},
              .title = s,
              .accesskey = (unsigned char)t[0],
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
        if (auto value = tag->getAttr(ATTR_SRC)) {
          std::string s;
          if (auto value = tag->getAttr(ATTR_TITLE)) {
            s = *value;
          }
          auto pp = url_quote(remove_space(*value));
          a_img = data->registerImg(pp, s, nlines, line.len());
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
        auto hseq = 0;
        if (auto value = tag->getAttr(ATTR_HSEQ)) {
          hseq = stoi(*value);
        }
        auto form_id = -1;
        if (auto value = tag->getAttr(ATTR_FID)) {
          form_id = stoi(*value);
        }
        int top = 0;
        if (auto value = tag->getAttr(ATTR_TOP_MARGIN)) {
          top = stoi(*value);
        }
        int bottom = 0;
        if (auto value = tag->getAttr(ATTR_BOTTOM_MARGIN)) {
          bottom = stoi(*value);
        }
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

        int textareanumber = -1;
        if (a_textarea.size()) {
          if (auto value = tag->getAttr(ATTR_TEXTAREANUMBER)) {
            textareanumber = stoi(*value);
          }
        }

        BufferPoint bp{
            .line = nlines,
            .pos = line.len(),
        };

        a_form = data->registerForm(h_env->createFormItem(tag), form, bp);
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
          if (!tag->existsAttr(ATTR_NO_EFFECT))
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
        if (auto value = tag->getAttr(ATTR_HREF)) {
          data->baseURL = {url_quote(remove_space(*value)), url};
        }
        if (auto value = tag->getAttr(ATTR_TARGET)) {
          data->baseTarget = url_quote(*value);
        }
        break;
      }

      case HTML_META: {
        std::string p;
        if (auto value = tag->getAttr(ATTR_HTTP_EQUIV)) {
          p = *value;
        }
        std::string q;
        if (auto value = tag->getAttr(ATTR_CONTENT)) {
          q = *value;
        }
        if (p.size() && q.size() && !strcasecmp(p, "refresh") && MetaRefresh) {
          auto meta = getMetaRefreshParam(q);
          if (meta.url.size()) {
            // p = Strnew(url_quote(remove_space(meta.url)))->ptr;
            // TODO:
            // App::instance().task(refresh_interval, FUNCNAME_gorURL, p);
          } else if (meta.interval > 0) {
            data->refresh_interval = meta.interval;
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
        if (auto value = tag->getAttr(ATTR_FID)) {
          process_form_int(tag.get(), stoi(*value));
        }
        break;
      }
      case HTML_TEXTAREA_INT:
        if (auto value = tag->getAttr(ATTR_TEXTAREANUMBER)) {
          n_textarea = stoi(*value);
          if (n_textarea >= 0) {
          } else {
            n_textarea = -1;
          }
        }
        break;
      case HTML_N_TEXTAREA_INT:
        if (n_textarea >= 0) {
          auto item = a_textarea[n_textarea]->formItem;
          item->init_value = item->value = textarea_str[n_textarea];
        }
        break;

      case HTML_TITLE_ALT: {
        if (auto value = tag->getAttr(ATTR_TITLE)) {
          data->title = html_unquote(*value);
        }
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

MetaRefreshInfo getMetaRefreshParam(const std::string &_q) {
  if (_q.empty()) {
    return {};
  }

  auto refresh_interval = stoi(_q);
  if (refresh_interval < 0) {
    return {};
  }

  std::string s_tmp;
  auto q = _q.data();
  while (q != _q.data() + _q.size()) {
    if (!strncasecmp(&*q, "url=", 4)) {
      q += 4;
      if (*q == '\"' || *q == '\'') /* " or ' */
        q++;
      auto r = q;
      while (*r && !IS_SPACE(*r) && *r != ';')
        r++;

      s_tmp = std::string(q, r - q);

      if (s_tmp.size() > 0 && (s_tmp.back() == '\"' ||  /* " */
                               s_tmp.back() == '\'')) { /* ' */
        s_tmp.pop_back();
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

  return {
      .interval = refresh_interval,
      .url = s_tmp,
  };
}

std::string HtmlParser::process_img(const HtmlTag *tag, int width) {
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
    tmp << this->process_form(parseHtmlTag(&s, true).get());
    tmp << "<input_alt fid=\"" << this->cur_form_id() << "\" "
        << "type=hidden name=link value=\"";
    tmp << html_quote((r2) ? r2 + 1 : r);
    tmp << "\"><input_alt hseq=\"" << this->cur_hseq++ << "\" fid=\""
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

std::string HtmlParser::process_anchor(HtmlTag *tag,
                                       const std::string &tagbuf) {
  if (tag->needRreconstruct()) {
    std::stringstream ss;
    ss << this->cur_hseq++;
    tag->setAttr(ATTR_HSEQ, ss.str());
    return tag->to_str();
  } else {
    std::stringstream tmp;
    tmp << "<a hseq=\"" << this->cur_hseq++ << "\"";
    tmp << tagbuf.substr(2);
    return tmp.str();
  }
}

std::string HtmlParser::process_input(const HtmlTag *tag) {
  int v, x, y, z;
  int qlen = 0;

  std::stringstream tmp;
  if (this->cur_form_id() < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp << this->process_form(parseHtmlTag(&s, true).get());
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
  tmp << "<input_alt hseq=\"" << this->cur_hseq++ << "\" fid=\""
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

std::string HtmlParser::process_button(const HtmlTag *tag) {
  int v;

  std::stringstream tmp;
  if (this->cur_form_id() < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp << this->process_form(parseHtmlTag(&s, true).get());
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
  tmp << "<input_alt hseq=\"" << this->cur_hseq++ << "\" fid=\""
      << this->cur_form_id() << "\" type=\"" << html_quote(p) << "\" "
      << "name=\"" << html_quote(r) << "\" value=\"" << qq << "\">";
  return tmp.str();
}

std::string HtmlParser::process_hr(const HtmlTag *tag, int width,
                                   int indent_width) {
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

void HtmlParser::proc_escape(html_feed_environ *h_env,
                             const char **str_return) {
  const char *str = *str_return;
  char32_t ech = getescapechar(str_return);
  int n_add = *str_return - str;
  Lineprop mode = PC_ASCII;

  if (ech == -1) {
    *str_return = str;
    h_env->proc_mchar(h_env->flag & RB_SPECIAL, 1, str_return, PC_ASCII);
    return;
  }
  mode = IS_CNTRL(ech) ? PC_CTRL : PC_ASCII;

  auto estr = conv_entity(ech);
  h_env->check_breakpoint(h_env->flag & RB_SPECIAL, estr.c_str());
  auto width = get_strwidth(estr.c_str());
  if (width == 1 && ech == (char32_t)estr[0] && ech != '&' && ech != '<' &&
      ech != '>') {
    if (IS_CNTRL(ech))
      mode = PC_CTRL;
    h_env->push_charp(width, estr.c_str(), mode);
  } else {
    h_env->push_nchars(width, str, n_add, mode);
  }
  h_env->prevchar = estr;
  h_env->prev_ctype = mode;
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

static int need_flushline(struct html_feed_environ *h_env, Lineprop mode) {
  if (h_env->flag & RB_PRE_INT) {
    if (h_env->pos > h_env->limit)
      return 1;
    else
      return 0;
  }

  auto ch = h_env->line.back();
  /* if (ch == ' ' && h_env->tag_sp > 0) */
  if (ch == ' ')
    return 0;

  if (h_env->pos > h_env->limit)
    return 1;

  return 0;
}

void HtmlParser::parse(std::string_view html, struct html_feed_environ *h_env,
                       bool internal) {

  Tokenizer tokenizer(html);
  TableStatus t;
  while (auto token = tokenizer.getToken(h_env, t, internal)) {
    process_token(t, *token, h_env);
  }

  h_env->parse_end(h_env->buf, h_env->limit, h_env->envs[h_env->envc].indent);
}

void HtmlParser::process_token(TableStatus &t, const Token &token,
                               html_feed_environ *h_env) {
  if (t.is_active(h_env)) {
    /*
     * within table: in <table>..</table>, all input tokens
     * are fed to the table renderer, and then the renderer
     * makes HTML output.
     */
    switch (t.feed(this, token.str, internal)) {
    case 0:
      /* </table> tag */
      h_env->table_level--;
      if (h_env->table_level >= MAX_TABLE - 1)
        return;
      t.tbl->end_table();
      if (h_env->table_level >= 0) {
        auto tbl0 = tables[h_env->table_level];
        std::stringstream ss;
        ss << "<table_alt tid=" << tbl0->ntable << ">";
        if (tbl0->row < 0)
          return;
        tbl0->pushTable(t.tbl);
        t.tbl = tbl0;
        t.tbl_mode = &table_mode[h_env->table_level];
        t.tbl_width = table_width(h_env, h_env->table_level);
        t.feed(this, ss.str(), true);
        return;
        /* continue to the next */
      }
      if (h_env->flag & RB_DEL)
        return;
      /* all tables have been read */
      if (t.tbl->vspace > 0 && !(h_env->flag & RB_IGNORE_P)) {
        int indent = h_env->envs[h_env->envc].indent;
        h_env->flushline(h_env->buf, indent, 0, h_env->limit);
        h_env->do_blankline(h_env->buf, indent, 0, h_env->limit);
      }
      save_fonteffect(h_env);
      initRenderTable();
      t.tbl->renderTable(this, t.tbl_width, h_env);
      restore_fonteffect(h_env);
      h_env->flag &= ~RB_IGNORE_P;
      if (t.tbl->vspace > 0) {
        int indent = h_env->envs[h_env->envc].indent;
        h_env->do_blankline(h_env->buf, indent, 0, h_env->limit);
        h_env->flag |= RB_IGNORE_P;
      }
      h_env->prevchar = " ";
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
    if (tag->process(h_env) == 0) {
      // preserve the tag for second-stage processing
      if (tag->needRreconstruct())
        h_env->tagbuf = tag->to_str();
      h_env->push_tag(h_env->tagbuf, tag->tagid);
    }
    h_env->bp.init_flag = 1;
    clear_ignore_p_flag(h_env, tag->tagid);
    if (tag->tagid == HTML_TABLE) {
      if (h_env->table_level >= 0) {
        int level = std::min<short>(h_env->table_level, MAX_TABLE - 1);
        t.tbl = tables[level];
        t.tbl_mode = &table_mode[level];
        t.tbl_width = table_width(h_env, level);
      }
    }
    return;
  }

  if (h_env->flag & (RB_DEL | RB_S)) {
    return;
  }

  auto pp = token.str.c_str();
  while (*pp) {
    auto mode = get_mctype(pp);
    int delta = get_mcwidth(pp);
    if (h_env->flag & (RB_SPECIAL & ~RB_NOBR)) {
      char ch = *pp;
      if (!(h_env->flag & RB_PLAIN) && (*pp == '&')) {
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
        h_env->flag &= ~RB_IGNORE_P;
      if (ch == '\n') {
        pp++;
        if (h_env->flag & RB_IGNORE_P) {
          h_env->flag &= ~RB_IGNORE_P;
          return;
        }
        if (h_env->flag & RB_PRE_INT)
          h_env->push_char(h_env->flag & RB_SPECIAL, ' ');
        else
          h_env->flushline(h_env->buf, h_env->envs[h_env->envc].indent, 1,
                           h_env->limit);
      } else if (ch == '\t') {
        do {
          h_env->push_char(h_env->flag & RB_SPECIAL, ' ');
        } while ((h_env->envs[h_env->envc].indent + h_env->pos) % Tabstop != 0);
        pp++;
      } else if (h_env->flag & RB_PLAIN) {
        auto p = html_quote_char(*pp);
        if (p) {
          h_env->push_charp(1, p, PC_ASCII);
          pp++;
        } else {
          h_env->proc_mchar(1, delta, &pp, mode);
        }
      } else {
        if (*pp == '&')
          proc_escape(h_env, &pp);
        else
          h_env->proc_mchar(1, delta, &pp, mode);
      }
      if (h_env->flag & (RB_SPECIAL & ~RB_PRE_INT))
        return;
    } else {
      if (!IS_SPACE(*pp))
        h_env->flag &= ~RB_IGNORE_P;
      if ((mode == PC_ASCII || mode == PC_CTRL) && IS_SPACE(*pp)) {
        if (h_env->prevchar[0] != ' ') {
          h_env->push_char(h_env->flag & RB_SPECIAL, ' ');
        }
        pp++;
      } else {
        if (*pp == '&')
          proc_escape(h_env, &pp);
        else
          h_env->proc_mchar(h_env->flag & RB_SPECIAL, delta, &pp, mode);
      }
    }
    if (need_flushline(h_env, mode)) {
      auto bp = h_env->line.c_str() + h_env->bp.len;
      auto tp = bp - h_env->bp.tlen;
      int i = 0;

      if (tp > h_env->line.c_str() && tp[-1] == ' ')
        i = 1;

      int indent = h_env->envs[h_env->envc].indent;
      if (h_env->bp.pos - i > indent) {
        h_env->append_tags(); /* may reallocate the buffer */
        bp = h_env->line.c_str() + h_env->bp.len;
        std::string line = bp;
        Strshrink(h_env->line, h_env->line.size() - h_env->bp.len);
        if (h_env->pos - i > h_env->limit)
          h_env->flag |= RB_FILL;
        h_env->back_to_breakpoint();
        h_env->flushline(h_env->buf, indent, 0, h_env->limit);
        h_env->flag &= ~RB_FILL;
        HTMLlineproc1(line, h_env);
      }
    }
  }
}

std::string HtmlParser::process_n_button() { return "</input_alt>"; }

std::string HtmlParser::process_select(const HtmlTag *tag) {
  std::stringstream tmp;

  if (cur_form_id() < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp << process_form(parseHtmlTag(&s, true).get());
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

std::string HtmlParser::process_n_select() {
  if (cur_select.empty())
    return {};
  this->process_option();
  select_str += "<br>";
  cur_select = {};
  n_selectitem = 0;
  return select_str;
}

void HtmlParser::feed_select(const std::string &_str) {
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

void HtmlParser::process_option() {
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
     << this->cur_hseq++
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

void HtmlParser::completeHTMLstream(struct html_feed_environ *h_env) {
  this->close_anchor(h_env);
  if (h_env->img_alt.size()) {
    h_env->push_tag("</img_alt>", HTML_N_IMG_ALT);
    h_env->img_alt = nullptr;
  }
  if (h_env->input_alt.in) {
    h_env->push_tag("</input_alt>", HTML_N_INPUT_ALT);
    h_env->input_alt.hseq = 0;
    h_env->input_alt.fid = -1;
    h_env->input_alt.in = 0;
    h_env->input_alt.type = nullptr;
    h_env->input_alt.name = nullptr;
    h_env->input_alt.value = nullptr;
  }
  if (h_env->fontstat.in_bold) {
    h_env->push_tag("</b>", HTML_N_B);
    h_env->fontstat.in_bold = 0;
  }
  if (h_env->fontstat.in_italic) {
    h_env->push_tag("</i>", HTML_N_I);
    h_env->fontstat.in_italic = 0;
  }
  if (h_env->fontstat.in_under) {
    h_env->push_tag("</u>", HTML_N_U);
    h_env->fontstat.in_under = 0;
  }
  if (h_env->fontstat.in_strike) {
    h_env->push_tag("</s>", HTML_N_S);
    h_env->fontstat.in_strike = 0;
  }
  if (h_env->fontstat.in_ins) {
    h_env->push_tag("</ins>", HTML_N_INS);
    h_env->fontstat.in_ins = 0;
  }
  if (h_env->flag & RB_INTXTA)
    this->HTMLlineproc1("</textarea>", h_env);
  /* for unbalanced select tag */
  if (h_env->flag & RB_INSELECT)
    this->HTMLlineproc1("</select>", h_env);
  if (h_env->flag & RB_TITLE)
    this->HTMLlineproc1("</title>", h_env);

  /* for unbalanced table tag */
  if (h_env->table_level >= MAX_TABLE)
    h_env->table_level = MAX_TABLE - 1;

  while (h_env->table_level >= 0) {
    int tmp = h_env->table_level;
    table_mode[h_env->table_level].pre_mode &=
        ~(TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN);
    this->HTMLlineproc1("</table>", h_env);
    if (h_env->table_level >= tmp)
      break;
  }
}
