#include "html_renderer.h"
#include "html_feed_env.h"
#include "line_data.h"
#include "symbol.h"
#include "myctype.h"
#include "entity.h"
#include "html_tag.h"
#include "url_quote.h"
#include "quote.h"
#include "cmp.h"
#include "html_meta.h"

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

Line HtmlRenderer::renderLine(
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
        if (form_id < 0 || form_id >= (int)h_env->forms.size() ||
            h_env->forms[form_id] == nullptr) {
          break; /* outside of <form>..</form> */
        }
        auto form = h_env->forms[form_id];
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
        if (h_env->a_textarea.size()) {
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
          while (textareanumber >= (int)h_env->a_textarea.size()) {
            h_env->textarea_str.push_back({});
            h_env->a_textarea.push_back({});
          }
          h_env->a_textarea[textareanumber] = a_form;
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
          h_env->process_form_int(tag, stoi(*value));
        }
        break;
      }
      case HTML_TEXTAREA_INT:
        if (auto value = tag->getAttr(ATTR_TEXTAREANUMBER)) {
          h_env->n_textarea = stoi(*value);
          if (h_env->n_textarea >= 0) {
          } else {
            h_env->n_textarea = -1;
          }
        }
        break;
      case HTML_N_TEXTAREA_INT:
        if (h_env->n_textarea >= 0) {
          auto item = h_env->a_textarea[h_env->n_textarea]->formItem;
          item->init_value = item->value =
              h_env->textarea_str[h_env->n_textarea];
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

std::shared_ptr<LineData>
HtmlRenderer::render(const Url &currentUrl, html_feed_environ *h_env,
                    const std::shared_ptr<AnchorList<FormAnchor>> &old) {

  auto formated = std::make_shared<LineData>();
  formated->clear(h_env->width());
  formated->baseURL = currentUrl;
  formated->title = h_env->title();

  auto feed = h_env->feed();

  for (int nlines = 0; auto _str = feed.textlist_feed(); ++nlines) {
    auto &str = *_str;
    if (h_env->n_textarea >= 0 && str.size() && str[0] != '<') {
      h_env->textarea_str[h_env->n_textarea] += str;
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

  formated->formlist = h_env->forms;
  if (h_env->n_textarea) {
    formated->addMultirowsForm();
  }

  return formated;
}
