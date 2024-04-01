#include "html_parser.h"
#include "cmp.h"
#include "push_symbol.h"
#include "option_param.h"
#include "html_feed_env.h"
#include "entity.h"
#include "anchorlist.h"
#include "line_data.h"
#include "line_layout.h"
#include "symbol.h"
#include "url_quote.h"
#include "form.h"
#include "html_tag.h"
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

#define IMG_SYMBOL UL_SYMBOL(12)
#define HR_SYMBOL 26
#define TEXTAREA_ATTR_COL_MAX 4096
#define TEXTAREA_ATTR_ROWS_MAX 4096
#define INITIAL_FORM_SIZE 10

HtmlParser::HtmlParser(int width) : _width(width) {}

void HtmlParser::HTMLlineproc1(const std::string &x,
                               struct html_feed_environ *y) {
  parse(x, y, true);
}

void HtmlParser::CLOSE_DT(readbuffer *obuf, html_feed_environ *h_env) {
  if (obuf->flag & RB_IN_DT) {
    obuf->flag &= ~RB_IN_DT;
    HTMLlineproc1("</b>", h_env);
  }
}

void HtmlParser::CLOSE_A(readbuffer *obuf, html_feed_environ *h_env) {
  do {
    obuf->CLOSE_P(h_env);
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

std::string HtmlParser::getLinkNumberStr(int correction) const {
  std::stringstream ss;
  ss << "[" << (cur_hseq + correction) << "]";
  return ss.str();
}

void HtmlParser::push_render_image(const std::string &str, int width, int limit,
                                   struct html_feed_environ *h_env) {
  struct readbuffer *obuf = &h_env->obuf;
  int indent = h_env->envs[h_env->envc].indent;

  obuf->push_spaces(1, (limit - width) / 2);
  obuf->push_str(width, str, PC_ASCII);
  obuf->push_spaces(1, (limit - width + 1) / 2);
  if (width > 0)
    obuf->flushline(h_env->buf, indent, 0, h_env->limit);
}

void HtmlParser::close_anchor(struct html_feed_environ *h_env) {
  auto obuf = &h_env->obuf;
  if (obuf->anchor.url.size()) {
    const char *p = nullptr;
    int is_erased = 0;

    auto found = obuf->find_stack([](auto tag) { return tag->cmd == HTML_A; });

    if (found == obuf->tag_stack.end() && obuf->anchor.hseq > 0 &&
        obuf->line.back() == ' ') {
      Strshrink(obuf->line, 1);
      obuf->pos--;
      is_erased = 1;
    }

    if (found != obuf->tag_stack.end() || (p = obuf->has_hidden_link(HTML_A))) {
      if (obuf->anchor.hseq > 0) {
        this->HTMLlineproc1(ANSP, h_env);
        obuf->prevchar = " ";
      } else {
        if (found != obuf->tag_stack.end()) {
          obuf->tag_stack.erase(found);
        } else {
          obuf->passthrough(p, 1);
        }
        obuf->anchor = {};
        return;
      }
      is_erased = 0;
    }
    if (is_erased) {
      obuf->line.push_back(' ');
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
  if (pre_title.size()) {
    return;
  }
  cur_title = "";
}

std::string HtmlParser::process_n_title(const std::shared_ptr<HtmlTag> &tag) {
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

void HtmlParser::feed_title(const std::string &_str) {
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

std::string HtmlParser::process_textarea(const std::shared_ptr<HtmlTag> &tag,
                                         int width) {
  std::stringstream tmp;

  if (cur_form_id() < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp << process_form(HtmlTag::parse(&s, true));
  }

  cur_textarea = "";
  if (auto value = tag->parsedtag_get_value(ATTR_NAME)) {
    cur_textarea = *value;
  }
  cur_textarea_size = 20;
  if (auto value = tag->parsedtag_get_value(ATTR_COLS)) {
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
  if (auto value = tag->parsedtag_get_value(ATTR_ROWS)) {
    cur_textarea_rows = stoi(*value);
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

std::string HtmlParser::process_form_int(const std::shared_ptr<HtmlTag> &tag,
                                         int fid) {
  std::string p = "get";
  if (auto value = tag->parsedtag_get_value(ATTR_METHOD)) {
    p = *value;
  }
  std::string q = "!CURRENT_URL!";
  if (auto value = tag->parsedtag_get_value(ATTR_ACTION)) {
    q = url_quote(remove_space(*value));
  }
  std::string s = "";
  if (auto value = tag->parsedtag_get_value(ATTR_ENCTYPE)) {
    s = *value;
  }
  std::string tg = "";
  if (auto value = tag->parsedtag_get_value(ATTR_TARGET)) {
    tg = *value;
  }
  std::string n = "";
  if (auto value = tag->parsedtag_get_value(ATTR_NAME)) {
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

std::shared_ptr<LineLayout>
HtmlParser::render(const std::shared_ptr<LineLayout> &layout,
                   const Url &currentUrl, html_feed_environ *h_env) {

  auto old = *layout->formated->_formitem;
  layout->formated->clear(_width);
  layout->formated->baseURL = currentUrl;
  layout->formated->title = h_env->title;

  LineFeed feed(h_env->buf);
  for (int nlines = 0; auto _str = feed.textlist_feed(); ++nlines) {
    auto &str = *_str;
    if (n_textarea >= 0 && str.size() && str[0] != '<') {
      textarea_str[n_textarea] += str;
      continue;
    }

    auto line =
        renderLine(currentUrl, h_env, layout->formated, nlines, str.c_str(), &old);

    /* end of processing for one line */
    if (!internal) {
      line.PPUSH(0, 0);
      layout->formated->lines.push_back(line);
    }
    if (internal == HTML_N_INTERNAL) {
      internal = {};
    }
  }

  layout->formated->formlist = forms;
  if (n_textarea) {
    layout->formated->addMultirowsForm();
  }

  layout->fix_size(h_env->limit);
  layout->formResetBuffer(layout->formated->_formitem.get());

  return layout;
}

Line HtmlParser::renderLine(const Url &url, html_feed_environ *h_env,
                            const std::shared_ptr<LineData> &data, int nlines, const char *str,
                            AnchorList<FormAnchor> *old) {

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
        // const char *s = nullptr;
        // auto q = res->baseTarget;
        // char *id = nullptr;
        if (auto value = tag->parsedtag_get_value(ATTR_NAME)) {
          auto _id = url_quote(*value);
          data->registerName(_id.c_str(), nlines, line.len());
        }
        std::string p;
        if (auto value = tag->parsedtag_get_value(ATTR_HREF)) {
          p = url_quote(remove_space(*value));
        }
        std::string q;
        if (auto value = tag->parsedtag_get_value(ATTR_TARGET)) {
          q = url_quote(*value);
        }
        std::string r;
        if (auto value = tag->parsedtag_get_value(ATTR_REFERER)) {
          r = url_quote(*value);
        }
        std::string s;
        if (auto value = tag->parsedtag_get_value(ATTR_TITLE)) {
          s = *value;
        }
        std::string t;
        if (auto value = tag->parsedtag_get_value(ATTR_ACCESSKEY)) {
          t = *value;
        } else {
          t.push_back(0);
        }
        auto hseq = 0;
        if (auto value = tag->parsedtag_get_value(ATTR_HSEQ)) {
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
        if (auto value = tag->parsedtag_get_value(ATTR_SRC)) {
          std::string s;
          if (auto value = tag->parsedtag_get_value(ATTR_TITLE)) {
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
        if (auto value = tag->parsedtag_get_value(ATTR_HSEQ)) {
          hseq = stoi(*value);
        }
        auto form_id = -1;
        if (auto value = tag->parsedtag_get_value(ATTR_FID)) {
          form_id = stoi(*value);
        }
        int top = 0;
        if (auto value = tag->parsedtag_get_value(ATTR_TOP_MARGIN)) {
          top = stoi(*value);
        }
        int bottom = 0;
        if (auto value = tag->parsedtag_get_value(ATTR_BOTTOM_MARGIN)) {
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
          if (auto value = tag->parsedtag_get_value(ATTR_TEXTAREANUMBER)) {
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
        if (auto value = tag->parsedtag_get_value(ATTR_HREF)) {
          data->baseURL = {url_quote(remove_space(*value)), url};
        }
        if (auto value = tag->parsedtag_get_value(ATTR_TARGET)) {
          data->baseTarget = url_quote(*value);
        }
        break;
      }

      case HTML_META: {
        std::string p;
        if (auto value = tag->parsedtag_get_value(ATTR_HTTP_EQUIV)) {
          p = *value;
        }
        std::string q;
        if (auto value = tag->parsedtag_get_value(ATTR_CONTENT)) {
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
        if (auto value = tag->parsedtag_get_value(ATTR_FID)) {
          process_form_int(tag, stoi(*value));
        }
        break;
      }
      case HTML_TEXTAREA_INT:
        if (auto value = tag->parsedtag_get_value(ATTR_TEXTAREANUMBER)) {
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
        if (auto value = tag->parsedtag_get_value(ATTR_TITLE)) {
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

std::string HtmlParser::process_img(const std::shared_ptr<HtmlTag> &tag,
                                    int width) {
  int pre_int = false, ext_pre_int = false;

  auto value = tag->parsedtag_get_value(ATTR_SRC);
  if (!value)
    return {};
  auto _p = url_quote(remove_space(*value));

  std::string _q;
  if (auto value = tag->parsedtag_get_value(ATTR_ALT)) {
    _q = *value;
  }
  if (!pseudoInlines && (_q.empty() && ignore_null_img_alt)) {
    return {};
  }

  auto t = _q;
  if (auto value = tag->parsedtag_get_value(ATTR_TITLE)) {
    t = *value;
  }

  int w = -1;
  if (auto value = tag->parsedtag_get_value(ATTR_WIDTH)) {
    w = stoi(*value);
    if (w < 0) {
      if (width > 0)
        w = (int)(-width * pixel_per_char * w / 100 + 0.5);
      else
        w = -1;
    }
  }

  int i = -1;
  if (auto value = tag->parsedtag_get_value(ATTR_HEIGHT)) {
    i = stoi(*value);
  }

  std::string r;
  if (auto value = tag->parsedtag_get_value(ATTR_USEMAP)) {
    r = *value;
  }

  if (tag->parsedtag_exists(ATTR_PRE_INT))
    ext_pre_int = true;

  std::stringstream tmp;
  if (r.size()) {
    auto r2 = strchr(r.c_str(), '#');
    auto s = "<form_int method=internal action=map>";
    tmp << this->process_form(HtmlTag::parse(&s, true));
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

std::string HtmlParser::process_anchor(const std::shared_ptr<HtmlTag> &tag,
                                       const std::string &tagbuf) {
  if (tag->parsedtag_need_reconstruct()) {
    std::stringstream ss;
    ss << this->cur_hseq++;
    tag->parsedtag_set_value(ATTR_HSEQ, ss.str());
    return tag->parsedtag2str();
  } else {
    std::stringstream tmp;
    tmp << "<a hseq=\"" << this->cur_hseq++ << "\"";
    tmp << tagbuf.substr(2);
    return tmp.str();
  }
}

std::string HtmlParser::process_input(const std::shared_ptr<HtmlTag> &tag) {
  int v, x, y, z;
  int qlen = 0;

  std::stringstream tmp;
  if (this->cur_form_id() < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp << this->process_form(HtmlTag::parse(&s, true));
  }

  std::string p = "text";
  if (auto value = tag->parsedtag_get_value(ATTR_TYPE)) {
    p = *value;
  }

  std::string q;
  if (auto value = tag->parsedtag_get_value(ATTR_VALUE)) {
    q = *value;
  }

  std::string r = "";
  if (auto value = tag->parsedtag_get_value(ATTR_NAME)) {
    r = *value;
  }

  int size = 20;
  if (auto value = tag->parsedtag_get_value(ATTR_SIZE)) {
    size = stoi(*value);
  }
  if (size > MAX_INPUT_SIZE)
    size = MAX_INPUT_SIZE;

  int i = 20;
  if (auto value = tag->parsedtag_get_value(ATTR_MAXLENGTH)) {
    i = stoi(*value);
  }

  std::string p2;
  if (auto value = tag->parsedtag_get_value(ATTR_ALT)) {
    p2 = *value;
  }

  x = tag->parsedtag_exists(ATTR_CHECKED);
  y = tag->parsedtag_exists(ATTR_ACCEPT);
  z = tag->parsedtag_exists(ATTR_READONLY);

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
      if (auto value = tag->parsedtag_get_value(ATTR_SRC)) {
        s = *value;
      }
      if (s.size()) {
        tmp << "<img src=\"" << html_quote(s) << "\"";
        if (p2.size())
          tmp << " alt=\"" << html_quote(p2) << "\"";
        if (auto value = tag->parsedtag_get_value(ATTR_WIDTH))
          tmp << " width=\"" << stoi(*value) << "\"";
        if (auto value = tag->parsedtag_get_value(ATTR_HEIGHT))
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

std::string HtmlParser::process_button(const std::shared_ptr<HtmlTag> &tag) {
  int v;

  std::stringstream tmp;
  if (this->cur_form_id() < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp << this->process_form(HtmlTag::parse(&s, true));
  }

  std::string p = "submit";
  if (auto value = tag->parsedtag_get_value(ATTR_TYPE)) {
    p = *value;
  }

  std::string q;
  if (auto value = tag->parsedtag_get_value(ATTR_VALUE)) {
    q = *value;
  }

  std::string r = "";
  if (auto value = tag->parsedtag_get_value(ATTR_NAME)) {
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

std::string HtmlParser::process_hr(const std::shared_ptr<HtmlTag> &tag,
                                   int width, int indent_width) {
  std::stringstream tmp;
  tmp << "<nobr>";

  if (width > indent_width)
    width -= indent_width;

  int w = 0;
  if (auto value = tag->parsedtag_get_value(ATTR_WIDTH)) {
    w = stoi(*value);
    if (w > HR_ATTR_WIDTH_MAX) {
      w = HR_ATTR_WIDTH_MAX;
    }
    w = REAL_WIDTH(w, width);
  } else {
    w = width;
  }

  int x = ALIGN_CENTER;
  if (auto value = tag->parsedtag_get_value(ATTR_ALIGN)) {
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
    return h_env->obuf.HTML_B_enter();
  case HTML_N_B:
    return h_env->obuf.HTML_B_exit();
  case HTML_I:
    return h_env->obuf.HTML_I_enter();
  case HTML_N_I:
    return h_env->obuf.HTML_I_exit();
  case HTML_U:
    return h_env->obuf.HTML_U_enter();
  case HTML_N_U:
    return h_env->obuf.HTML_U_exit();
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
    h_env->obuf.flushline(h_env->buf, envs[h_env->envc].indent, 1,
                          h_env->limit);
    h_env->obuf.blank_lines = 0;
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
    return h_env->obuf.HTML_PRE_INT_enter();
  case HTML_N_PRE_INT:
    return h_env->obuf.HTML_PRE_INT_exit();
  case HTML_NOBR:
    return h_env->obuf.HTML_NOBR_enter();
  case HTML_N_NOBR:
    return h_env->obuf.HTML_NOBR_exit();
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
    h_env->obuf.end_tag = HTML_UNKNOWN;
    return 1;
  case HTML_N_STYLE:
    h_env->obuf.flag &= ~RB_STYLE;
    h_env->obuf.end_tag = HTML_UNKNOWN;
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
    return h_env->obuf.HTML_INPUT_ALT_enter(tag);
  case HTML_N_INPUT_ALT:
    return h_env->obuf.HTML_INPUT_ALT_exit();
  case HTML_TABLE:
    return h_env->HTML_TABLE_enter(tag, _width);
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

  if (ech == -1) {
    *str_return = str;
    obuf->proc_mchar(obuf->flag & RB_SPECIAL, 1, str_return, PC_ASCII);
    return;
  }
  mode = IS_CNTRL(ech) ? PC_CTRL : PC_ASCII;

  auto estr = conv_entity(ech);
  obuf->check_breakpoint(obuf->flag & RB_SPECIAL, estr.c_str());
  auto width = get_strwidth(estr.c_str());
  if (width == 1 && ech == (char32_t)estr[0] && ech != '&' && ech != '<' &&
      ech != '>') {
    if (IS_CNTRL(ech))
      mode = PC_CTRL;
    obuf->push_charp(width, estr.c_str(), mode);
  } else {
    obuf->push_nchars(width, str, n_add, mode);
  }
  obuf->prevchar = estr;
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
  if (obuf->flag & RB_PRE_INT) {
    if (obuf->pos > h_env->limit)
      return 1;
    else
      return 0;
  }

  auto ch = obuf->line.back();
  /* if (ch == ' ' && obuf->tag_sp > 0) */
  if (ch == ' ')
    return 0;

  if (obuf->pos > h_env->limit)
    return 1;

  return 0;
}

void HtmlParser::parse(std::string_view _line, struct html_feed_environ *h_env,
                       bool internal) {

  std::string line{_line.begin(), _line.end()};

  TableStatus t;

  while (line.size()) {
    int is_tag = false;
    auto pre_mode = t.pre_mode(h_env);
    int end_tag = t.end_tag(h_env);
    std::string str;
    if (line[0] == '<' || h_env->obuf.status != R_ST_NORMAL) {
      /*
       * Tag processing
       */
      if (h_env->obuf.status == R_ST_EOL)
        h_env->obuf.status = R_ST_NORMAL;
      else {
        if (h_env->obuf.status != R_ST_NORMAL) {
          auto p = line.c_str();
          append_token(h_env->tagbuf, &p, &h_env->obuf.status,
                       pre_mode & RB_PREMODE);
          line = p;
        } else {
          auto p = line.c_str();
          if (auto buf =
                  read_token(&p, &h_env->obuf.status, pre_mode & RB_PREMODE)) {
            h_env->tagbuf = *buf;
          }
          line = p;
        }
        if (h_env->obuf.status != R_ST_NORMAL)
          return;
      }
      if (h_env->tagbuf.empty())
        continue;
      str = h_env->tagbuf;
      if (str[0] == '<') {
        if (str[1] && REALLY_THE_BEGINNING_OF_A_TAG(str))
          is_tag = true;
        else if (!(pre_mode & (RB_PLAIN | RB_INTXTA | RB_INSELECT | RB_SCRIPT |
                               RB_STYLE | RB_TITLE))) {
          line = str.substr(1) + line;
          str = "&lt;";
        }
      }
    } else {
      std::string tokbuf;
      auto p = line.c_str();
      if (auto value =
              read_token(&p, &h_env->obuf.status, pre_mode & RB_PREMODE)) {
        tokbuf = *value;
      }
      line = p;
      if (h_env->obuf.status != R_ST_NORMAL) /* R_ST_AMP ? */
        h_env->obuf.status = R_ST_NORMAL;
      str = tokbuf;
    }

    if (pre_mode & (RB_PLAIN | RB_INTXTA | RB_INSELECT | RB_SCRIPT | RB_STYLE |
                    RB_TITLE)) {
      bool goto_proc_normal = false;
      if (is_tag) {
        auto p = str.c_str();
        if (auto tag = HtmlTag::parse(&p, internal)) {
          if (tag->tagid == end_tag ||
              (pre_mode & RB_INSELECT && tag->tagid == HTML_N_FORM) ||
              (pre_mode & RB_TITLE &&
               (tag->tagid == HTML_N_HEAD || tag->tagid == HTML_BODY))) {
            goto_proc_normal = true;
          }
        }
      }
      if (goto_proc_normal) {
        // goto proc_normal;
      } else {
        /* title */
        if (pre_mode & RB_TITLE) {
          feed_title(str);
          continue;
        }
        /* select */
        if (pre_mode & RB_INSELECT) {
          if (h_env->obuf.table_level >= 0) {
            // goto proc_normal;
          } else {
            this->feed_select(str);
            continue;
          }
        } else {
          if (is_tag) {
            const char *p;
            if (strncmp(str.c_str(), "<!--", 4) &&
                (p = strchr(str.c_str() + 1, '<'))) {
              str = std::string(str, p - str.c_str());
              line = std::string(p) + line;
            }
            is_tag = false;
            continue;
          }
          if (h_env->obuf.table_level >= 0) {
            // goto proc_normal;
          } else {
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
        }
      }
    }

    if (t.is_active(h_env)) {
      /*
       * within table: in <table>..</table>, all input tokens
       * are fed to the table renderer, and then the renderer
       * makes HTML output.
       */
      switch (t.feed(this, str, internal)) {
      case 0:
        /* </table> tag */
        h_env->obuf.table_level--;
        if (h_env->obuf.table_level >= MAX_TABLE - 1)
          continue;
        t.tbl->end_table();
        if (h_env->obuf.table_level >= 0) {
          auto tbl0 = tables[h_env->obuf.table_level];
          std::stringstream ss;
          ss <<"<table_alt tid=" << tbl0->ntable << ">";
          str = ss.str();
          if (tbl0->row < 0)
            continue;
          tbl0->pushTable(t.tbl);
          t.tbl = tbl0;
          t.tbl_mode = &table_mode[h_env->obuf.table_level];
          t.tbl_width = table_width(h_env, h_env->obuf.table_level);
          t.feed(this, str, true);
          continue;
          /* continue to the next */
        }
        if (h_env->obuf.flag & RB_DEL)
          continue;
        /* all tables have been read */
        if (t.tbl->vspace > 0 && !(h_env->obuf.flag & RB_IGNORE_P)) {
          int indent = h_env->envs[h_env->envc].indent;
          h_env->obuf.flushline(h_env->buf, indent, 0, h_env->limit);
          h_env->obuf.do_blankline(h_env->buf, indent, 0, h_env->limit);
        }
        save_fonteffect(h_env);
        initRenderTable();
        t.tbl->renderTable(this, t.tbl_width, h_env);
        restore_fonteffect(h_env);
        h_env->obuf.flag &= ~RB_IGNORE_P;
        if (t.tbl->vspace > 0) {
          int indent = h_env->envs[h_env->envc].indent;
          h_env->obuf.do_blankline(h_env->buf, indent, 0, h_env->limit);
          h_env->obuf.flag |= RB_IGNORE_P;
        }
        h_env->obuf.prevchar = " ";
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
      auto p = str.c_str();
      if ((tag = HtmlTag::parse(&p, internal)))
        cmd = tag->tagid;
      else
        continue;
      str = p;
      /* process tags */
      if (pushHtmlTag(tag, h_env) == 0) {
        /* preserve the tag for second-stage processing */
        if (tag->parsedtag_need_reconstruct())
          h_env->tagbuf = tag->parsedtag2str();
        h_env->obuf.push_tag(h_env->tagbuf, cmd);
      }
      h_env->obuf.bp.init_flag = 1;
      clear_ignore_p_flag(cmd, &h_env->obuf);
      if (cmd == HTML_TABLE) {
        if (h_env->obuf.table_level >= 0) {
          int level = std::min<short>(h_env->obuf.table_level, MAX_TABLE - 1);
          t.tbl = tables[level];
          t.tbl_mode = &table_mode[level];
          t.tbl_width = table_width(h_env, level);
        }
      }
      continue;
    }

    if (h_env->obuf.flag & (RB_DEL | RB_S))
      continue;

    auto pp = str.c_str();
    while (*pp) {
      auto mode = get_mctype(pp);
      int delta = get_mcwidth(pp);
      if (h_env->obuf.flag & (RB_SPECIAL & ~RB_NOBR)) {
        char ch = *pp;
        if (!(h_env->obuf.flag & RB_PLAIN) && (*pp == '&')) {
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
          h_env->obuf.flag &= ~RB_IGNORE_P;
        if (ch == '\n') {
          pp++;
          if (h_env->obuf.flag & RB_IGNORE_P) {
            h_env->obuf.flag &= ~RB_IGNORE_P;
            continue;
          }
          if (h_env->obuf.flag & RB_PRE_INT)
            h_env->obuf.push_char(h_env->obuf.flag & RB_SPECIAL, ' ');
          else
            h_env->obuf.flushline(h_env->buf, h_env->envs[h_env->envc].indent,
                                  1, h_env->limit);
        } else if (ch == '\t') {
          do {
            h_env->obuf.push_char(h_env->obuf.flag & RB_SPECIAL, ' ');
          } while ((h_env->envs[h_env->envc].indent + h_env->obuf.pos) %
                       Tabstop !=
                   0);
          pp++;
        } else if (h_env->obuf.flag & RB_PLAIN) {
          auto p = html_quote_char(*pp);
          if (p) {
            h_env->obuf.push_charp(1, p, PC_ASCII);
            pp++;
          } else {
            h_env->obuf.proc_mchar(1, delta, &pp, mode);
          }
        } else {
          if (*pp == '&')
            proc_escape(&h_env->obuf, &pp);
          else
            h_env->obuf.proc_mchar(1, delta, &pp, mode);
        }
        if (h_env->obuf.flag & (RB_SPECIAL & ~RB_PRE_INT))
          continue;
      } else {
        if (!IS_SPACE(*pp))
          h_env->obuf.flag &= ~RB_IGNORE_P;
        if ((mode == PC_ASCII || mode == PC_CTRL) && IS_SPACE(*pp)) {
          if (h_env->obuf.prevchar[0] != ' ') {
            h_env->obuf.push_char(h_env->obuf.flag & RB_SPECIAL, ' ');
          }
          pp++;
        } else {
          if (*pp == '&')
            proc_escape(&h_env->obuf, &pp);
          else
            h_env->obuf.proc_mchar(h_env->obuf.flag & RB_SPECIAL, delta, &pp,
                                   mode);
        }
      }
      if (need_flushline(h_env, &h_env->obuf, mode)) {
        auto bp = h_env->obuf.line.c_str() + h_env->obuf.bp.len;
        auto tp = bp - h_env->obuf.bp.tlen;
        int i = 0;

        if (tp > h_env->obuf.line.c_str() && tp[-1] == ' ')
          i = 1;

        int indent = h_env->envs[h_env->envc].indent;
        if (h_env->obuf.bp.pos - i > indent) {
          h_env->obuf.append_tags(); /* may reallocate the buffer */
          bp = h_env->obuf.line.c_str() + h_env->obuf.bp.len;
          std::string line = bp;
          Strshrink(h_env->obuf.line,
                    h_env->obuf.line.size() - h_env->obuf.bp.len);
          if (h_env->obuf.pos - i > h_env->limit)
            h_env->obuf.flag |= RB_FILL;
          h_env->obuf.back_to_breakpoint();
          h_env->obuf.flushline(h_env->buf, indent, 0, h_env->limit);
          h_env->obuf.flag &= ~RB_FILL;
          HTMLlineproc1(line, h_env);
        }
      }
    }
  }
  if (!(h_env->obuf.flag & (RB_SPECIAL | RB_INTXTA | RB_INSELECT))) {
    char *tp;
    int i = 0;

    if (h_env->obuf.bp.pos == h_env->obuf.pos) {
      tp = &h_env->obuf.line[h_env->obuf.bp.len - h_env->obuf.bp.tlen];
    } else {
      tp = &h_env->obuf.line[h_env->obuf.line.size()];
    }

    if (tp > h_env->obuf.line.c_str() && tp[-1] == ' ')
      i = 1;
    int indent = h_env->envs[h_env->envc].indent;
    if (h_env->obuf.pos - i > h_env->limit) {
      h_env->obuf.flag |= RB_FILL;
      h_env->obuf.flushline(h_env->buf, indent, 0, h_env->limit);
      h_env->obuf.flag &= ~RB_FILL;
    }
  }
}

std::string HtmlParser::process_n_button() { return "</input_alt>"; }

std::string HtmlParser::process_select(const std::shared_ptr<HtmlTag> &tag) {
  std::stringstream tmp;

  if (cur_form_id() < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp << process_form(HtmlTag::parse(&s, true));
  }

  std::string p = "";
  if (auto value = tag->parsedtag_get_value(ATTR_NAME)) {
    p = *value;
  }
  cur_select = p;
  select_is_multiple = tag->parsedtag_exists(ATTR_MULTIPLE);

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

      if (!(tag = HtmlTag::parse(&p, false)))
        continue;
      switch (tag->tagid) {
      case HTML_OPTION:
        this->process_option();
        cur_option = {};
        if (auto value = tag->parsedtag_get_value(ATTR_VALUE))
          cur_option_value = *value;
        else
          cur_option_value = {};
        if (auto value = tag->parsedtag_get_value(ATTR_LABEL))
          cur_option_label = *value;
        else
          cur_option_label = {};
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
  auto obuf = &h_env->obuf;
  this->close_anchor(h_env);
  if (obuf->img_alt.size()) {
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
