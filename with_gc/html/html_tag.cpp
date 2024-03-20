#include "html_tag.h"
#include "html_quote.h"
#include "quote.h"
#include "html_command.h"
#include "myctype.h"
#include "Str.h"
#include "hash.h"
#include "table.h"
#include "alloc.h"
#include "html.c"
#include "textline.h"

/* parse HTML tag */

static int noConv(char *, char **);
static int toNumber(char *, int *);
static int toLength(char *, int *);
static int toVAlign(char *, int *);

/* *INDENT-OFF* */
using toValFuncType = int (*)(char *, int *);
static toValFuncType toValFunc[] = {
    (toValFuncType)noConv, /* VTYPE_NONE    */
    (toValFuncType)noConv, /* VTYPE_STR     */
    toNumber,              /* VTYPE_NUMBER  */
    toLength,              /* VTYPE_LENGTH  */
    [](char *p, int *r) -> int {
      return toAlign(p, (AlignMode *)r);
    },                     /* VTYPE_ALIGN   */
    toVAlign,              /* VTYPE_VALIGN  */
    (toValFuncType)noConv, /* VTYPE_ACTION  */
    (toValFuncType)noConv, /* VTYPE_ENCTYPE */
    (toValFuncType)noConv, /* VTYPE_METHOD  */
    (toValFuncType)noConv, /* VTYPE_MLENGTH */
    (toValFuncType)noConv, /* VTYPE_TYPE    */
};
/* *INDENT-ON* */

static int noConv(char *oval, char **str) {
  *str = oval;
  return 1;
}

static int toNumber(char *oval, int *num) {
  char *ep;
  int x;

  x = strtol(oval, &ep, 10);

  if (ep > oval) {
    *num = x;
    return 1;
  } else
    return 0;
}

static int toLength(char *oval, int *len) {
  int w;
  if (!IS_DIGIT(oval[0]))
    return 0;
  w = atoi(oval);
  if (w < 0)
    return 0;
  if (w == 0)
    w = 1;
  if (oval[strlen(oval) - 1] == '%')
    *len = -w;
  else
    *len = w;
  return 1;
}

static int toVAlign(char *oval, int *valign) {
  if (strcasecmp(oval, "top") == 0 || strcasecmp(oval, "baseline") == 0)
    *valign = VALIGN_TOP;
  else if (strcasecmp(oval, "bottom") == 0)
    *valign = VALIGN_BOTTOM;
  else if (strcasecmp(oval, "middle") == 0)
    *valign = VALIGN_MIDDLE;
  else
    return 0;
  return 1;
}

extern Hash_si tagtable;
#define MAX_TAG_LEN 64

std::shared_ptr<HtmlTag> HtmlTag::parse(const char **s, int internal) {

  /* Parse tag name */
  char tagname[MAX_TAG_LEN];
  tagname[0] = '\0';
  auto q = (*s) + 1;
  auto p = tagname;
  if (*q == '/') {
    *(char *)(p++) = *(q++);
    SKIP_BLANKS(q);
  }
  while (*q && !IS_SPACE(*q) && !(tagname[0] != '/' && *q == '/') &&
         *q != '>' && p - tagname < MAX_TAG_LEN - 1) {
    *(char *)(p++) = TOLOWER(*q);
    q++;
  }
  *(char *)p = '\0';
  while (*q && !IS_SPACE(*q) && !(tagname[0] != '/' && *q == '/') && *q != '>')
    q++;

  auto tag_id = (HtmlCommand)getHash_si(&tagtable, tagname, HTML_UNKNOWN);

  std::shared_ptr<HtmlTag> tag;
  int attr_id = 0;
  if (tag_id == HTML_UNKNOWN || (!internal && TagMAP[tag_id].flag & TFLG_INT))
    goto skip_parse_tagarg;

  tag = std::shared_ptr<HtmlTag>(new HtmlTag(tag_id));

  int nattr;
  if ((nattr = TagMAP[tag_id].max_attribute) > 0) {
    tag->attrid = (unsigned char *)NewAtom_N(unsigned char, nattr);
    tag->value = (char **)New_N(char *, nattr);
    tag->map = (unsigned char *)NewAtom_N(unsigned char, MAX_TAGATTR);
    memset(tag->map, MAX_TAGATTR, MAX_TAGATTR);
    memset(tag->attrid, ATTR_UNKNOWN, nattr);
    for (int i = 0; i < nattr; i++) {
      tag->map[TagMAP[tag_id].accept_attribute[i]] = i;
    }
  }

  /* Parse tag arguments */
  SKIP_BLANKS(q);
  char attrname[MAX_TAG_LEN];
  while (1) {
    Str *value = NULL, *value_tmp = NULL;
    if (*q == '>' || *q == '\0')
      goto done_parse_tag;
    p = attrname;
    while (*q && *q != '=' && !IS_SPACE(*q) && *q != '>' &&
           p - attrname < MAX_TAG_LEN - 1) {
      *(char *)(p++) = TOLOWER(*q);
      q++;
    }
    *(char *)p = '\0';
    while (*q && *q != '=' && !IS_SPACE(*q) && *q != '>')
      q++;
    SKIP_BLANKS(q);
    if (*q == '=') {
      /* get value */
      value_tmp = Strnew();
      q++;
      SKIP_BLANKS(q);
      if (*q == '"') {
        q++;
        while (*q && *q != '"') {
          Strcat_char(value_tmp, *q);
          if (!tag->need_reconstruct && is_html_quote(*q))
            tag->need_reconstruct = true;
          q++;
        }
        if (*q == '"')
          q++;
      } else if (*q == '\'') {
        q++;
        while (*q && *q != '\'') {
          Strcat_char(value_tmp, *q);
          if (!tag->need_reconstruct && is_html_quote(*q))
            tag->need_reconstruct = true;
          q++;
        }
        if (*q == '\'')
          q++;
      } else if (*q) {
        while (*q && !IS_SPACE(*q) && *q != '>') {
          Strcat_char(value_tmp, *q);
          if (!tag->need_reconstruct && is_html_quote(*q))
            tag->need_reconstruct = true;
          q++;
        }
      }
    }
    int i;
    for (i = 0; i < nattr; i++) {
      if ((tag)->attrid[i] == ATTR_UNKNOWN &&
          strcmp(AttrMAP[TagMAP[tag_id].accept_attribute[i]].name, attrname) ==
              0) {
        attr_id = TagMAP[tag_id].accept_attribute[i];
        break;
      }
    }

    if (value_tmp) {
      int hidden = false;
      for (int j = 0; j < i; j++) {
        if (tag->attrid[j] == ATTR_TYPE && tag->value[j] &&
            strcmp("hidden", tag->value[j]) == 0) {
          hidden = true;
          break;
        }
      }
      if ((tag_id == HTML_INPUT || tag_id == HTML_INPUT_ALT) &&
          attr_id == ATTR_VALUE && hidden) {
        value = value_tmp;
      } else {
        char *x;
        value = Strnew();
        for (x = value_tmp->ptr; *x; x++) {
          if (*x != '\n')
            Strcat_char(value, *x);
        }
      }
    }

    if (i != nattr) {
      if (!internal && ((AttrMAP[attr_id].flag & AFLG_INT) ||
                        (value && AttrMAP[attr_id].vtype == VTYPE_METHOD &&
                         !strcasecmp(value->ptr, "internal")))) {
        tag->need_reconstruct = true;
        continue;
      }
      tag->attrid[i] = attr_id;
      if (value)
        tag->value[i] = Strnew(html_unquote(value->ptr))->ptr;
      else
        tag->value[i] = NULL;
    } else {
      tag->need_reconstruct = true;
    }
  }

skip_parse_tagarg:
  while (*q != '>' && *q)
    q++;
done_parse_tag:
  if (*q == '>')
    q++;
  *s = q;
  return tag;
}

bool HtmlTag::parsedtag_set_value(HtmlTagAttr id, char *value) {
  if (!this->parsedtag_accepts(id))
    return false;
  auto i = this->map[id];
  this->attrid[i] = id;
  if (value)
    this->value[i] = allocStr(value, -1);
  else
    this->value[i] = NULL;
  this->need_reconstruct = true;
  return true;
}

bool HtmlTag::parsedtag_get_value(HtmlTagAttr id, void *value) const {
  if (!this->map) {
    return false;
  }
  int i = this->map[id];
  if (!this->parsedtag_exists(id) || !this->value[i]) {
    return false;
  }
  return toValFunc[AttrMAP[id].vtype](this->value[i], (int *)value);
}

Str *HtmlTag::parsedtag2str() const {
  int tag_id = this->tagid;
  int nattr = TagMAP[tag_id].max_attribute;
  Str *tagstr = Strnew();
  Strcat_char(tagstr, '<');
  Strcat_charp(tagstr, TagMAP[tag_id].name);
  for (int i = 0; i < nattr; i++) {
    if (this->attrid[i] != ATTR_UNKNOWN) {
      Strcat_char(tagstr, ' ');
      Strcat_charp(tagstr, AttrMAP[this->attrid[i]].name);
      if (this->value[i])
        Strcat(tagstr, Sprintf("=\"%s\"", html_quote(this->value[i])));
    }
  }
  Strcat_char(tagstr, '>');
  return tagstr;
}

int HtmlTag::ul_type(int default_type) const {
  char *p;
  if (this->parsedtag_get_value(ATTR_TYPE, &p)) {
    if (!strcasecmp(p, "disc"))
      return (int)'d';
    else if (!strcasecmp(p, "circle"))
      return (int)'c';
    else if (!strcasecmp(p, "square"))
      return (int)'s';
  }
  return default_type;
}
