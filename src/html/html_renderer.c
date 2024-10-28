#include "html/html_renderer.h"
#include "alloc.h"
#include "buffer/buffer.h"
#include "buffer/document.h"
#include "buffer/line.h"
#include "file/file.h"
#include "html/anchor.h"
#include "html/html_parser.h"
#include "html/html_readbuffer.h"
#include "html/html_tag.h"
#include "html/html_textarea.h"
#include "html/html_types.h"
#include "html/map.h"
#include "text/myctype.h"
#include "text/text.h"
#include "text/utf8.h"
#include <string.h>

bool MetaRefresh = false;
int symbol_width;
int symbol_width0;

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

static void addLink(struct Document *doc, struct HtmlTag *tag) {
  const char *href = NULL, *title = NULL, *ctype = NULL, *rel = NULL,
             *rev = NULL;

  parsedtag_get_value(tag, ATTR_HREF, &href);
  if (href)
    href = url_quote(remove_space(href));
  parsedtag_get_value(tag, ATTR_TITLE, &title);
  parsedtag_get_value(tag, ATTR_TYPE, &ctype);
  parsedtag_get_value(tag, ATTR_REL, &rel);

  char type = LINK_TYPE_NONE;
  if (rel != NULL) {
    /* forward link type */
    type = LINK_TYPE_REL;
    if (title == NULL)
      title = rel;
  }
  parsedtag_get_value(tag, ATTR_REV, &rev);
  if (rev != NULL) {
    /* reverse link type */
    type = LINK_TYPE_REV;
    if (title == NULL)
      title = rev;
  }

  struct LinkList *l = New(struct LinkList);
  l->url = href;
  l->title = title;
  l->ctype = ctype;
  l->type = type;
  l->next = NULL;
  if (doc->linklist) {
    struct LinkList *i;
    for (i = doc->linklist; i->next; i = i->next)
      ;
    i->next = l;
  } else {
    doc->linklist = l;
  }
}

struct LineProcStatus {
  uint8_t *outc;
  Lineprop *outp;
  int out_size;
  struct Document *doc;

  struct Anchor *a_href;
  struct Anchor *a_img;
  struct Anchor *a_form;
  // const char *p, *q, *r, *s, *t;
  const char *str;
  // Lineprop mode;
  Lineprop effect;
  Lineprop ex_effect;
  // int pos;
  int nlines;
  int frameset_sp;
  // const char *id = NULL;
  // int hseq, form_id;
  // const char *endp;
  char symbol;
  int internal;
  struct Anchor **a_textarea;
};

void PSIZE(struct LineProcStatus *st, int pos) {
  if (st->out_size <= pos + 1) {
    st->out_size = pos * 3 / 2;
    st->outc = New_Reuse(char, st->outc, st->out_size);
    st->outp = New_Reuse(Lineprop, st->outp, st->out_size);
  }
}

void PPUSH(struct LineProcStatus *st, int *pos, Lineprop p, char c) {
  st->outp[(*pos)] = p;
  st->outc[(*pos)] = c;
  (*pos)++;
}

int PPUSH_utf8(struct LineProcStatus *st, int *pos, Lineprop prop,
               const uint8_t **utf8) {
  int len = utf8sequence_len((const uint8_t *)*utf8);
  Lineprop *p = st->outp + *pos;
  uint8_t *c = st->outc + *pos;
  for (int i = 0; i < len; ++i, ++p, ++c, ++(*pos), ++(*utf8)) {
    p[0] = prop;
    c[0] = **utf8;
  }
  return len;
}

void _proc_tag(struct LineProcStatus *st, struct Url currentURL,
               const char *str, struct HtmlTag *tag, int pos) {
  switch (tag->tagid) {
  case HTML_B:
    st->effect |= PE_BOLD;
    break;
  case HTML_N_B:
    st->effect &= ~PE_BOLD;
    break;
  case HTML_I:
    st->ex_effect |= PE_EX_ITALIC;
    break;
  case HTML_N_I:
    st->ex_effect &= ~PE_EX_ITALIC;
    break;
  case HTML_INS:
    st->ex_effect |= PE_EX_INSERT;
    break;
  case HTML_N_INS:
    st->ex_effect &= ~PE_EX_INSERT;
    break;
  case HTML_U:
    st->effect |= PE_UNDER;
    break;
  case HTML_N_U:
    st->effect &= ~PE_UNDER;
    break;
  case HTML_S:
    st->ex_effect |= PE_EX_STRIKE;
    break;
  case HTML_N_S:
    st->ex_effect &= ~PE_EX_STRIKE;
    break;
  case HTML_A: {
    const char *p = nullptr;
    const char *r = nullptr;
    const char *s = nullptr;
    auto q = st->doc->baseTarget;
    auto t = "";
    auto hseq = 0;
    const char *id = NULL;
    if (parsedtag_get_value(tag, ATTR_NAME, &id)) {
      id = url_quote(id);
      registerName(st->doc, id, currentLn(st->doc), pos);
    }
    if (parsedtag_get_value(tag, ATTR_HREF, &p))
      p = url_quote(remove_space(p));
    if (parsedtag_get_value(tag, ATTR_TARGET, &q))
      q = url_quote(q);
    if (parsedtag_get_value(tag, ATTR_REFERER, &r))
      r = url_quote(r);
    parsedtag_get_value(tag, ATTR_TITLE, &s);
    parsedtag_get_value(tag, ATTR_ACCESSKEY, &t);
    parsedtag_get_value(tag, ATTR_HSEQ, &hseq);
    if (hseq > 0)
      st->doc->hmarklist =
          putHmarker(st->doc->hmarklist, currentLn(st->doc), pos, hseq - 1);
    else if (hseq < 0) {
      int h = -hseq - 1;
      if (st->doc->hmarklist && h < st->doc->hmarklist->nmark &&
          st->doc->hmarklist->marks[h].invalid) {
        st->doc->hmarklist->marks[h].pos = pos;
        st->doc->hmarklist->marks[h].line = currentLn(st->doc);
        st->doc->hmarklist->marks[h].invalid = 0;
        hseq = -hseq;
      }
    }
    if (p) {
      st->effect |= PE_ANCHOR;
      st->a_href =
          registerHref(st->doc, p, q, r, s, *t, currentLn(st->doc), pos);
      st->a_href->hseq = ((hseq > 0) ? hseq : -hseq) - 1;
      st->a_href->slave = (hseq > 0) ? false : true;
    }
    break;
  }

  case HTML_N_A: {
    st->effect &= ~PE_ANCHOR;
    if (st->a_href) {
      st->a_href->end.line = currentLn(st->doc);
      st->a_href->end.pos = pos;
      if (st->a_href->start.line == st->a_href->end.line &&
          st->a_href->start.pos == st->a_href->end.pos) {
        if (st->doc->hmarklist && st->a_href->hseq >= 0 &&
            st->a_href->hseq < st->doc->hmarklist->nmark)
          st->doc->hmarklist->marks[st->a_href->hseq].invalid = 1;
        st->a_href->hseq = -1;
      }
      st->a_href = NULL;
    }
    break;
  }

  case HTML_LINK:
    addLink(st->doc, tag);
    break;

  case HTML_IMG_ALT: {
    const char *p;
    if (parsedtag_get_value(tag, ATTR_SRC, &p)) {
      const char *s = NULL;
      parsedtag_get_value(tag, ATTR_TITLE, &s);
      p = url_quote(remove_space(p));
      st->a_img = registerImg(st->doc, p, s, currentLn(st->doc), pos);
    }
    st->effect |= PE_IMAGE;
    break;
  }

  case HTML_N_IMG_ALT: {
    st->effect &= ~PE_IMAGE;
    if (st->a_img) {
      st->a_img->end.line = currentLn(st->doc);
      st->a_img->end.pos = pos;
    }
    st->a_img = NULL;
    break;
  }

  case HTML_INPUT_ALT: {
    struct FormList *form;
    int top = 0, bottom = 0;
    int textareanumber = -1;
    auto hseq = 0;
    auto form_id = -1;

    parsedtag_get_value(tag, ATTR_HSEQ, &hseq);
    parsedtag_get_value(tag, ATTR_FID, &form_id);
    parsedtag_get_value(tag, ATTR_TOP_MARGIN, &top);
    parsedtag_get_value(tag, ATTR_BOTTOM_MARGIN, &bottom);
    if (form_id < 0 || form_id > form_max || forms == NULL ||
        forms[form_id] == NULL)
      break; /* outside of <form>..</form> */
    form = forms[form_id];
    if (hseq > 0) {
      int hpos = pos;
      if (*str == '[')
        hpos++;
      st->doc->hmarklist =
          putHmarker(st->doc->hmarklist, currentLn(st->doc), hpos, hseq - 1);
    } else if (hseq < 0) {
      int h = -hseq - 1;
      int hpos = pos;
      if (*str == '[')
        hpos++;
      if (st->doc->hmarklist && h < st->doc->hmarklist->nmark &&
          st->doc->hmarklist->marks[h].invalid) {
        st->doc->hmarklist->marks[h].pos = hpos;
        st->doc->hmarklist->marks[h].line = currentLn(st->doc);
        st->doc->hmarklist->marks[h].invalid = 0;
        hseq = -hseq;
      }
    }

    if (!form->target)
      form->target = st->doc->baseTarget;
    if (st->a_textarea &&
        parsedtag_get_value(tag, ATTR_TEXTAREANUMBER, &textareanumber)) {
      if (textareanumber >= max_textarea) {
        max_textarea = 2 * textareanumber;
        textarea_str = New_Reuse(Str, textarea_str, max_textarea);
        st->a_textarea =
            New_Reuse(struct Anchor *, st->a_textarea, max_textarea);
      }
    }
    st->a_form = registerForm(st->doc, form, tag, currentLn(st->doc), pos);
    if (st->a_textarea && textareanumber >= 0)
      st->a_textarea[textareanumber] = st->a_form;
    if (st->a_form) {
      st->a_form->hseq = hseq - 1;
      st->a_form->y = currentLn(st->doc) - top;
      st->a_form->rows = 1 + top + bottom;
      if (!parsedtag_exists(tag, ATTR_NO_EFFECT))
        st->effect |= PE_FORM;
      break;
    }
  }

  case HTML_N_INPUT_ALT: {
    st->effect &= ~PE_FORM;
    if (st->a_form) {
      st->a_form->end.line = currentLn(st->doc);
      st->a_form->end.pos = pos;
      if (st->a_form->start.line == st->a_form->end.line &&
          st->a_form->start.pos == st->a_form->end.pos)
        st->a_form->hseq = -1;
    }
    st->a_form = NULL;
    break;
  }

  case HTML_MAP: {
    const char *p;
    if (parsedtag_get_value(tag, ATTR_NAME, &p)) {
      struct MapList *m = New(struct MapList);
      m->name = Strnew_charp(p);
      m->area = newGeneralList();
      m->next = st->doc->maplist;
      st->doc->maplist = m;
    }
    break;
  }

  case HTML_N_MAP:
    /* nothing to do */
    break;

  case HTML_AREA: {
    if (st->doc->maplist == NULL) /* outside of <map>..</map> */
      break;
    const char *p;
    if (parsedtag_get_value(tag, ATTR_HREF, &p)) {
      struct MapArea *a;
      p = url_quote(remove_space(p));
      const char *t = NULL;
      parsedtag_get_value(tag, ATTR_TARGET, &t);
      auto q = "";
      parsedtag_get_value(tag, ATTR_ALT, &q);
      const char *r = NULL;
      const char *s = NULL;
      a = newMapArea(p, t, q, r, s);
      pushValue(st->doc->maplist->area, (void *)a);
    }
    break;
  }

  case HTML_FRAMESET:
    st->frameset_sp++;
    break;

  case HTML_N_FRAMESET:
    if (st->frameset_sp >= 0)
      st->frameset_sp--;
    break;

  case HTML_FRAME:
    break;

  case HTML_BASE: {
    const char *p;
    if (parsedtag_get_value(tag, ATTR_HREF, &p)) {
      p = url_quote(remove_space(p));
      if (!st->doc->baseURL)
        st->doc->baseURL = New(struct Url);
      parseURL2(p, st->doc->baseURL, &currentURL);
      // base = baseURL;
    }
    if (parsedtag_get_value(tag, ATTR_TARGET, &p))
      st->doc->baseTarget = url_quote(p);
    break;
  }

  case HTML_META: {
    const char *p = nullptr;
    const char *q = nullptr;
    parsedtag_get_value(tag, ATTR_HTTP_EQUIV, &p);
    parsedtag_get_value(tag, ATTR_CONTENT, &q);
    if (p && q && !strcasecmp(p, "refresh") && MetaRefresh) {
      Str tmp = NULL;
      int refresh_interval = getMetaRefreshParam(q, &tmp);
      if (tmp) {
        p = url_quote(remove_space(tmp->ptr));
      }
    }
    break;
  }

  case HTML_INTERNAL:
    st->internal = HTML_INTERNAL;
    break;

  case HTML_N_INTERNAL:
    st->internal = HTML_N_INTERNAL;
    break;

  case HTML_FORM_INT: {
    int form_id;
    if (parsedtag_get_value(tag, ATTR_FID, &form_id))
      process_form_int(tag, form_id);
    break;
  }

  case HTML_TEXTAREA_INT:
    if (parsedtag_get_value(tag, ATTR_TEXTAREANUMBER, &n_textarea) &&
        n_textarea >= 0 && n_textarea < max_textarea) {
      textarea_str[n_textarea] = Strnew();
    } else
      n_textarea = -1;
    break;

  case HTML_N_TEXTAREA_INT:
    if (st->a_textarea && n_textarea >= 0) {
      struct FormItemList *item =
          (struct FormItemList *)st->a_textarea[n_textarea]->url;
      item->init_value = item->value = textarea_str[n_textarea];
    }
    break;

  case HTML_TITLE_ALT: {
    const char *p;
    if (parsedtag_get_value(tag, ATTR_TITLE, &p))
      st->doc->title = html_unquote(p);
    break;
  }

  case HTML_SYMBOL: {
    st->effect |= PC_SYMBOL;
    const char *p;
    if (parsedtag_get_value(tag, ATTR_TYPE, &p))
      st->symbol = (char)atoi(p);
    break;
  }

  case HTML_N_SYMBOL:
    st->effect &= ~PC_SYMBOL;
    break;

  default:
    break;
  }
}

void proc_wrapped_line(struct LineProcStatus *st, struct Url currentURL,
                       Str line) {
  int pos = 0;
  Strremovetrailingspaces(line);
  const char *str = line->ptr;
  auto endp = str + line->length;
  bool error = false;
  while (str < endp) {
    PSIZE(st, pos);
    auto mode = get_mctype((const uint8_t *)str);
    if ((st->effect | ex_efct(st->ex_effect)) & PC_SYMBOL && *str != '<') {
      PPUSH(st, &pos, PC_ASCII | st->effect | ex_efct(st->ex_effect),
            SYMBOL_BASE + st->symbol);
      str += symbol_width;
    } else if (mode == PC_CTRL || IS_INTSPACE(*str)) {
      PPUSH(st, &pos, PC_ASCII | st->effect | ex_efct(st->ex_effect), ' ');
      str++;
    } else if (*str != '<' && *str != '&') {
      if (!PPUSH_utf8(st, &pos, mode | st->effect | ex_efct(st->ex_effect),
                      &str)) {
        error = true;
        break;
      }
    } else if (*str == '&') {
      /*
       * & escape processing
       */
      auto p = getescapecmd(&str);
      while (*p) {
        PSIZE(st, pos);
        mode = get_mctype((unsigned char *)p);
        if (mode == PC_CTRL || IS_INTSPACE(*str)) {
          PPUSH(st, &pos, PC_ASCII | st->effect | ex_efct(st->ex_effect), ' ');
          p++;
        } else {
          PPUSH(st, &pos, mode | st->effect | ex_efct(st->ex_effect), *(p++));
        }
      }
    } else {
      /* tag processing */
      struct HtmlTag *tag = parse_tag(&str);
      if (!tag) {
        continue;
      }
      _proc_tag(st, currentURL, str, tag, pos);
    }
  }

  /* end of processing for one line */
  if (!st->internal) {
    addnewline(st->doc, st->outc, st->outp, pos, -1, st->nlines);
  }
  if (st->internal == HTML_N_INTERNAL) {
    st->internal = 0;
  }
  if (!error && str != endp) {
    line = Strsubstr(line, str - line->ptr, endp - str);
    proc_wrapped_line(st, currentURL, line);
  }
}

struct Document *render_to_lines(int cols, struct Url currentURL,
                                 struct Url *base, struct TextLineList *lines) {
  static uint8_t *outc = NULL;
  static Lineprop *outp = NULL;
  static int out_size = 0;

  struct LineProcStatus st;
  memset(&st, 0, sizeof(struct LineProcStatus));

  if (!out_size) {
    out_size = 1024;
    outc = NewAtom_N(char, out_size);
    outp = NewAtom_N(Lineprop, out_size);
  }
  st.out_size = out_size;
  st.outc = outc;
  st.outp = outp;
  memset(st.outc, 0, sizeof(out_size));
  memset(st.outp, 0, sizeof(Lineprop) * out_size);

  st.doc = newDocument(cols);

  prerender_textarea();

  st.a_textarea = New_N(struct Anchor *, max_textarea);
  for (auto item = lines->first; item; item = item->next) {
    auto line = item->ptr->line;
    if (n_textarea >= 0 && *(line->ptr) != '<') { /* halfload */
      Strcat(textarea_str[n_textarea], line);
      continue;
    }
    proc_wrapped_line(&st, currentURL, line);
  }

  for (int form_id = 1; form_id <= form_max; form_id++)
    if (forms[form_id])
      forms[form_id]->next = forms[form_id - 1];

  st.doc->formlist = (form_max >= 0) ? forms[form_max] : NULL;
  if (n_textarea) {
    addMultirowsForm(st.doc, st.doc->formitem);
  }

  return st.doc;
}
