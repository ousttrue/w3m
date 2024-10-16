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

#define ENABLE_REMOVE_TRAILINGSPACES

bool MetaRefresh = false;
int symbol_width;
int symbol_width0;

#define PSIZE                                                                  \
  if (out_size <= pos + 1) {                                                   \
    out_size = pos * 3 / 2;                                                    \
    outc = New_Reuse(char, outc, out_size);                                    \
    outp = New_Reuse(Lineprop, outp, out_size);                                \
  }

#define PPUSH(p, c)                                                            \
  {                                                                            \
    outp[pos] = (p);                                                           \
    outc[pos] = (c);                                                           \
    pos++;                                                                     \
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

struct Document *HTMLlineproc2body(int cols, struct Url currentURL,
                                   struct Url *base, GetLineFunc feed) {
  static char *outc = NULL;
  static Lineprop *outp = NULL;
  static int out_size = 0;
  struct Anchor *a_href = NULL, *a_img = NULL, *a_form = NULL;
  const char *p, *q, *r, *s, *t;
  const char *str;
  Lineprop mode, effect, ex_effect;
  int pos;
  int nlines;
  int frameset_sp = -1;
  const char *id = NULL;
  int hseq, form_id;
  Str line;
  const char *endp;
  char symbol = '\0';
  int internal = 0;
  struct Anchor **a_textarea = NULL;

  if (out_size == 0) {
    out_size = LINELEN;
    outc = NewAtom_N(char, out_size);
    outp = NewAtom_N(Lineprop, out_size);
  }

  auto doc = newDocument(cols);

  prerender_textarea();
  a_textarea = New_N(struct Anchor *, max_textarea);

  effect = 0;
  ex_effect = 0;
  nlines = 0;
  while ((line = feed()) != NULL) {
    if (n_textarea >= 0 && *(line->ptr) != '<') { /* halfload */
      Strcat(textarea_str[n_textarea], line);
      continue;
    }
  proc_again:
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
        PPUSH(mode | effect | ex_efct(ex_effect), *(str++));
      } else if (*str == '&') {
        /*
         * & escape processing
         */
        p = getescapecmd(&str);
        while (*p) {
          PSIZE;
          mode = get_mctype((unsigned char *)p);
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
        if (!(tag = parse_tag(&str)))
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
          q = doc->baseTarget;
          t = "";
          hseq = 0;
          id = NULL;
          if (parsedtag_get_value(tag, ATTR_NAME, &id)) {
            id = url_quote(id);
            registerName(doc, id, currentLn(doc), pos);
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
            doc->hmarklist =
                putHmarker(doc->hmarklist, currentLn(doc), pos, hseq - 1);
          else if (hseq < 0) {
            int h = -hseq - 1;
            if (doc->hmarklist && h < doc->hmarklist->nmark &&
                doc->hmarklist->marks[h].invalid) {
              doc->hmarklist->marks[h].pos = pos;
              doc->hmarklist->marks[h].line = currentLn(doc);
              doc->hmarklist->marks[h].invalid = 0;
              hseq = -hseq;
            }
          }
          if (p) {
            effect |= PE_ANCHOR;
            a_href = registerHref(doc, p, q, r, s, *t, currentLn(doc), pos);
            a_href->hseq = ((hseq > 0) ? hseq : -hseq) - 1;
            a_href->slave = (hseq > 0) ? false : true;
          }
          break;
        case HTML_N_A:
          effect &= ~PE_ANCHOR;
          if (a_href) {
            a_href->end.line = currentLn(doc);
            a_href->end.pos = pos;
            if (a_href->start.line == a_href->end.line &&
                a_href->start.pos == a_href->end.pos) {
              if (doc->hmarklist && a_href->hseq >= 0 &&
                  a_href->hseq < doc->hmarklist->nmark)
                doc->hmarklist->marks[a_href->hseq].invalid = 1;
              a_href->hseq = -1;
            }
            a_href = NULL;
          }
          break;

        case HTML_LINK:
          addLink(doc, tag);
          break;

        case HTML_IMG_ALT:
          if (parsedtag_get_value(tag, ATTR_SRC, &p)) {
            s = NULL;
            parsedtag_get_value(tag, ATTR_TITLE, &s);
            p = url_quote(remove_space(p));
            a_img = registerImg(doc, p, s, currentLn(doc), pos);
          }
          effect |= PE_IMAGE;
          break;
        case HTML_N_IMG_ALT:
          effect &= ~PE_IMAGE;
          if (a_img) {
            a_img->end.line = currentLn(doc);
            a_img->end.pos = pos;
          }
          a_img = NULL;
          break;
        case HTML_INPUT_ALT: {
          struct FormList *form;
          int top = 0, bottom = 0;
          int textareanumber = -1;
          hseq = 0;
          form_id = -1;

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
            doc->hmarklist =
                putHmarker(doc->hmarklist, currentLn(doc), hpos, hseq - 1);
          } else if (hseq < 0) {
            int h = -hseq - 1;
            int hpos = pos;
            if (*str == '[')
              hpos++;
            if (doc->hmarklist && h < doc->hmarklist->nmark &&
                doc->hmarklist->marks[h].invalid) {
              doc->hmarklist->marks[h].pos = hpos;
              doc->hmarklist->marks[h].line = currentLn(doc);
              doc->hmarklist->marks[h].invalid = 0;
              hseq = -hseq;
            }
          }

          if (!form->target)
            form->target = doc->baseTarget;
          if (a_textarea &&
              parsedtag_get_value(tag, ATTR_TEXTAREANUMBER, &textareanumber)) {
            if (textareanumber >= max_textarea) {
              max_textarea = 2 * textareanumber;
              textarea_str = New_Reuse(Str, textarea_str, max_textarea);
              a_textarea = New_Reuse(struct Anchor *, a_textarea, max_textarea);
            }
          }
          a_form = registerForm(doc, form, tag, currentLn(doc), pos);
          if (a_textarea && textareanumber >= 0)
            a_textarea[textareanumber] = a_form;
          if (a_form) {
            a_form->hseq = hseq - 1;
            a_form->y = currentLn(doc) - top;
            a_form->rows = 1 + top + bottom;
            if (!parsedtag_exists(tag, ATTR_NO_EFFECT))
              effect |= PE_FORM;
            break;
          }
        }
        case HTML_N_INPUT_ALT:
          effect &= ~PE_FORM;
          if (a_form) {
            a_form->end.line = currentLn(doc);
            a_form->end.pos = pos;
            if (a_form->start.line == a_form->end.line &&
                a_form->start.pos == a_form->end.pos)
              a_form->hseq = -1;
          }
          a_form = NULL;
          break;
        case HTML_MAP:
          if (parsedtag_get_value(tag, ATTR_NAME, &p)) {
            struct MapList *m = New(struct MapList);
            m->name = Strnew_charp(p);
            m->area = newGeneralList();
            m->next = doc->maplist;
            doc->maplist = m;
          }
          break;
        case HTML_N_MAP:
          /* nothing to do */
          break;
        case HTML_AREA:
          if (doc->maplist == NULL) /* outside of <map>..</map> */
            break;
          if (parsedtag_get_value(tag, ATTR_HREF, &p)) {
            struct MapArea *a;
            p = url_quote(remove_space(p));
            t = NULL;
            parsedtag_get_value(tag, ATTR_TARGET, &t);
            q = "";
            parsedtag_get_value(tag, ATTR_ALT, &q);
            r = NULL;
            s = NULL;
            a = newMapArea(p, t, q, r, s);
            pushValue(doc->maplist->area, (void *)a);
          }
          break;
        case HTML_FRAMESET:
          frameset_sp++;
          break;
        case HTML_N_FRAMESET:
          if (frameset_sp >= 0)
            frameset_sp--;
          break;
        case HTML_FRAME:
          break;
        case HTML_BASE:
          if (parsedtag_get_value(tag, ATTR_HREF, &p)) {
            p = url_quote(remove_space(p));
            if (!doc->baseURL)
              doc->baseURL = New(struct Url);
            parseURL2(p, doc->baseURL, &currentURL);
            base = baseURL;
          }
          if (parsedtag_get_value(tag, ATTR_TARGET, &p))
            doc->baseTarget = url_quote(p);
          break;
        case HTML_META:
          p = q = NULL;
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
        case HTML_INTERNAL:
          internal = HTML_INTERNAL;
          break;
        case HTML_N_INTERNAL:
          internal = HTML_N_INTERNAL;
          break;
        case HTML_FORM_INT:
          if (parsedtag_get_value(tag, ATTR_FID, &form_id))
            process_form_int(tag, form_id);
          break;
        case HTML_TEXTAREA_INT:
          if (parsedtag_get_value(tag, ATTR_TEXTAREANUMBER, &n_textarea) &&
              n_textarea >= 0 && n_textarea < max_textarea) {
            textarea_str[n_textarea] = Strnew();
          } else
            n_textarea = -1;
          break;
        case HTML_N_TEXTAREA_INT:
          if (a_textarea && n_textarea >= 0) {
            struct FormItemList *item =
                (struct FormItemList *)a_textarea[n_textarea]->url;
            item->init_value = item->value = textarea_str[n_textarea];
          }
          break;
        case HTML_TITLE_ALT:
          if (parsedtag_get_value(tag, ATTR_TITLE, &p))
            doc->title = html_unquote(p);
          break;
        case HTML_SYMBOL:
          effect |= PC_SYMBOL;
          if (parsedtag_get_value(tag, ATTR_TYPE, &p))
            symbol = (char)atoi(p);
          break;
        case HTML_N_SYMBOL:
          effect &= ~PC_SYMBOL;
          break;
        }
      }
    }
    /* end of processing for one line */
    if (!internal)
      addnewline(doc, outc, outp, pos, -1, nlines);
    if (internal == HTML_N_INTERNAL)
      internal = 0;
    if (str != endp) {
      line = Strsubstr(line, str - line->ptr, endp - str);
      goto proc_again;
    }
  }
  for (form_id = 1; form_id <= form_max; form_id++)
    if (forms[form_id])
      forms[form_id]->next = forms[form_id - 1];
  doc->formlist = (form_max >= 0) ? forms[form_max] : NULL;
  if (n_textarea)
    addMultirowsForm(doc, doc->formitem);

  return doc;
}
