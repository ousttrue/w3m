#include "html/html_tag.h"
#include "alloc.h"
#include "hash.h"
#include "html/html_parser.h"
#include "html/html_text.h"
#include "html/html_types.h"
#include "input/url.h"
#include "text/Str.h"
#include "text/myctype.h"

/* parse HTML tag */

static int noConv(char *, char **);
static int toNumber(char *, int *);
static int toLength(char *, int *);

bool parsedtag_accepts(struct HtmlTag *tag, enum HtmlTagAttributeType id) {
  return ((tag)->map && (tag)->map[id] != MAX_TAGATTR);
}

bool parsedtag_exists(struct HtmlTag *tag, enum HtmlTagAttributeType id) {
  return (parsedtag_accepts(tag, id) &&
          ((tag)->attrid[(tag)->map[id]] != ATTR_UNKNOWN));
}

static int toAlign(char *oval, enum AlignMode *align) {
  if (strcasecmp(oval, "left") == 0)
    *align = ALIGN_LEFT;
  else if (strcasecmp(oval, "right") == 0)
    *align = ALIGN_RIGHT;
  else if (strcasecmp(oval, "center") == 0)
    *align = ALIGN_CENTER;
  else if (strcasecmp(oval, "top") == 0)
    *align = ALIGN_TOP;
  else if (strcasecmp(oval, "bottom") == 0)
    *align = ALIGN_BOTTOM;
  else if (strcasecmp(oval, "middle") == 0)
    *align = ALIGN_MIDDLE;
  else
    return 0;
  return 1;
}

static int toVAlign(const char *, int *);

typedef int (*ToValFunc)(const char *, void *);
static ToValFunc toValFunc[] = {
    (ToValFunc)noConv,   /* VTYPE_NONE    */
    (ToValFunc)noConv,   /* VTYPE_STR     */
    (ToValFunc)toNumber, /* VTYPE_NUMBER  */
    (ToValFunc)toLength, /* VTYPE_LENGTH  */
    (ToValFunc)toAlign,  /* VTYPE_ALIGN   */
    (ToValFunc)toVAlign, /* VTYPE_VALIGN  */
    (ToValFunc)noConv,   /* VTYPE_ACTION  */
    (ToValFunc)noConv,   /* VTYPE_ENCTYPE */
    (ToValFunc)noConv,   /* VTYPE_METHOD  */
    (ToValFunc)noConv,   /* VTYPE_MLENGTH */
    (ToValFunc)noConv,   /* VTYPE_TYPE    */
};

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

static int toVAlign(const char *oval, int *valign) {
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

struct HtmlTag *parse_tag(const char **s) {
  /* Parse tag name */
  char tagname[MAX_TAG_LEN];
  tagname[0] = '\0';
  auto q = (*s) + 1;
  auto p = tagname;
  if (*q == '/') {
    *(p++) = *(q++);
    SKIP_BLANKS(q);
  }
  while (*q && !IS_SPACE(*q) && !(tagname[0] != '/' && *q == '/') &&
         *q != '>' && p - tagname < MAX_TAG_LEN - 1) {
    *(p++) = TOLOWER(*q);
    q++;
  }
  *p = '\0';
  while (*q && !IS_SPACE(*q) && !(tagname[0] != '/' && *q == '/') && *q != '>')
    q++;

  enum HtmlTagType tag_id = getHash_si(&tagtable, tagname, HTML_UNKNOWN);

  if (tag_id == HTML_UNKNOWN)
    goto skip_parse_tagarg;

  auto tag = New(struct HtmlTag);
  memset(tag, 0, sizeof(struct HtmlTag));
  tag->tagid = tag_id;

  auto nattr = TagMAP[tag_id].max_attribute;
  if (nattr > 0) {
    tag->attrid = NewAtom_N(enum HtmlTagAttributeType, nattr);
    tag->value = New_N(const char *, nattr);
    tag->map = NewAtom_N(unsigned char, MAX_TAGATTR);
    memset(tag->map, MAX_TAGATTR, MAX_TAGATTR);
    memset(tag->attrid, ATTR_UNKNOWN, nattr * sizeof(enum HtmlTagAttributeType));
    for (int i = 0; i < nattr; i++)
      tag->map[TagMAP[tag_id].accept_attribute[i]] = i;
  }

  /* Parse tag arguments */
  SKIP_BLANKS(q);
  char attrname[MAX_TAG_LEN];
  while (1) {
    Str value = NULL, value_tmp = NULL;
    if (*q == '>' || *q == '\0')
      goto done_parse_tag;
    p = attrname;
    while (*q && *q != '=' && !IS_SPACE(*q) && *q != '>' &&
           p - attrname < MAX_TAG_LEN - 1) {
      *(p++) = TOLOWER(*q);
      q++;
    }
    *p = '\0';
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
    int i = 0;
    auto attr_id = ATTR_UNKNOWN;
    for (; i < nattr; i++) {
      if ((tag)->attrid[i] == ATTR_UNKNOWN &&
          strcmp(AttrMAP[TagMAP[tag_id].accept_attribute[i]].name, attrname) ==
              0) {
        attr_id = TagMAP[tag_id].accept_attribute[i];
        break;
      }
    }

    if (value_tmp) {
      int j, hidden = false;
      for (j = 0; j < i; j++) {
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
      // if (!internal && ((AttrMAP[attr_id].flag & AFLG_INT) ||
      //                   (value && AttrMAP[attr_id].vtype == VTYPE_METHOD &&
      //                    !strcasecmp(value->ptr, "internal")))) {
      //   tag->need_reconstruct = true;
      //   continue;
      // }
      tag->attrid[i] = attr_id;
      if (value)
        tag->value[i] = html_unquote(value->ptr);
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

bool parsedtag_set_value(struct HtmlTag *tag, enum HtmlTagAttributeType id,
                         const char *value) {
  if (!parsedtag_accepts(tag, id))
    return 0;

  int i = tag->map[id];
  tag->attrid[i] = id;
  if (value)
    tag->value[i] = allocStr(value, -1);
  else
    tag->value[i] = NULL;
  tag->need_reconstruct = true;
  return 1;
}

bool parsedtag_get_value(struct HtmlTag *tag, enum HtmlTagAttributeType id,
                         void *value) {
  int i;
  if (!parsedtag_exists(tag, id) || !tag->value[i = tag->map[id]])
    return 0;
  return toValFunc[AttrMAP[id].vtype](tag->value[i], value);
}

Str parsedtag2str(struct HtmlTag *tag) {
  int nattr = TagMAP[tag->tagid].max_attribute;
  Str tagstr = Strnew();
  Strcat_char(tagstr, '<');
  Strcat_charp(tagstr, TagMAP[tag->tagid].name);
  for (int i = 0; i < nattr; i++) {
    auto attr = tag->attrid[i];
    if (attr != ATTR_UNKNOWN) {
      Strcat_char(tagstr, ' ');
      Strcat_charp(tagstr, AttrMAP[attr].name);
      if (tag->value[i]) {
        Strcat(tagstr, Sprintf("=\"%s\"", html_quote(tag->value[i])));
      }
    }
  }
  Strcat_char(tagstr, '>');
  return tagstr;
}
