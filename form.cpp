/*
 * HTML forms
 */
#include "form.h"
#include "etc.h"
#include "mimetypes.h"
#include "message.h"
#include "readbuffer.h"
#include "html.h"
#include "utf8.h"
#include "tmpfile.h"
#include "line.h"
#include "display.h"
#include "buffer.h"
#include "anchor.h"
#include "fm.h"
#include "parsetag.h"
#include "parsetagx.h"
#include "myctype.h"
#include "local_cgi.h"
#include "regex.h"
#include "proto.h"
#include "util.h"
#include "indep.h"
#include <unistd.h>

extern Str **textarea_str;
extern int max_textarea;

/* *INDENT-OFF* */
struct {
  const char *action;
  void (*rout)(struct parsed_tagarg *);
} internal_action[] = {
    {"option", panel_set_option},
    {"cookie", set_cookie_flag},
    {"download", download_action},
    {"none", NULL},
    {NULL, NULL},
};
/* *INDENT-ON* */

struct FormList *newFormList(const char *action, const char *method,
                             const char *charset, const char *enctype,
                             const char *target, const char *name,
                             FormList *_next) {
  FormList *l;
  Str *a = Strnew_charp(action);
  int m = FORM_METHOD_GET;
  int e = FORM_ENCTYPE_URLENCODED;

  if (method == NULL || !strcasecmp(method, "get"))
    m = FORM_METHOD_GET;
  else if (!strcasecmp(method, "post"))
    m = FORM_METHOD_POST;
  else if (!strcasecmp(method, "internal"))
    m = FORM_METHOD_INTERNAL;
  /* unknown method is regarded as 'get' */

  if (m != FORM_METHOD_GET && enctype != NULL &&
      !strcasecmp(enctype, "multipart/form-data")) {
    e = FORM_ENCTYPE_MULTIPART;
  }

  l = (FormList *)New(FormList);
  l->item = l->lastitem = NULL;
  l->action = a;
  l->method = m;
  l->enctype = e;
  l->target = target;
  l->name = name;
  l->next = _next;
  l->nitems = 0;
  l->body = NULL;
  l->length = 0;
  return l;
}

/*
 * add <input> element to FormList
 */
FormItemList *formList_addInput(FormList *fl, struct parsed_tag *tag) {
  FormItemList *item;
  char *p;
  int i;

  /* if not in <form>..</form> environment, just ignore <input> tag */
  if (fl == NULL)
    return NULL;

  item = (FormItemList *)New(FormItemList);
  item->type = FORM_UNKNOWN;
  item->size = -1;
  item->rows = 0;
  item->checked = item->init_checked = 0;
  item->accept = 0;
  item->name = NULL;
  item->value = item->init_value = NULL;
  item->readonly = 0;
  if (parsedtag_get_value(tag, ATTR_TYPE, &p)) {
    item->type = formtype(p);
    if (item->size < 0 &&
        (item->type == FORM_INPUT_TEXT || item->type == FORM_INPUT_FILE ||
         item->type == FORM_INPUT_PASSWORD))
      item->size = FORM_I_TEXT_DEFAULT_SIZE;
  }
  if (parsedtag_get_value(tag, ATTR_NAME, &p))
    item->name = Strnew_charp(p);
  if (parsedtag_get_value(tag, ATTR_VALUE, &p))
    item->value = item->init_value = Strnew_charp(p);
  item->checked = item->init_checked = parsedtag_exists(tag, ATTR_CHECKED);
  item->accept = parsedtag_exists(tag, ATTR_ACCEPT);
  parsedtag_get_value(tag, ATTR_SIZE, &item->size);
  parsedtag_get_value(tag, ATTR_MAXLENGTH, &item->maxlength);
  item->readonly = parsedtag_exists(tag, ATTR_READONLY);
  if (parsedtag_get_value(tag, ATTR_TEXTAREANUMBER, &i) && i >= 0 &&
      i < max_textarea)
    item->value = item->init_value = textarea_str[i];
  if (parsedtag_get_value(tag, ATTR_ROWS, &p))
    item->rows = atoi(p);
  if (item->type == FORM_UNKNOWN) {
    /* type attribute is missing. Ignore the tag. */
    return NULL;
  }
  if (item->type == FORM_INPUT_FILE && item->value && item->value->length) {
    /* security hole ! */
    return NULL;
  }
  item->parent = fl;
  item->next = NULL;
  if (fl->item == NULL) {
    fl->item = fl->lastitem = item;
  } else {
    fl->lastitem->next = item;
    fl->lastitem = item;
  }
  if (item->type == FORM_INPUT_HIDDEN)
    return NULL;
  fl->nitems++;
  return item;
}

static char *_formtypetbl[] = {
    "text",  "password", "checkbox", "radio",  "submit", "reset", "hidden",
    "image", "select",   "textarea", "button", "file",   NULL};

static char *_formmethodtbl[] = {"GET", "POST", "INTERNAL", "HEAD"};

char *form2str(FormItemList *fi) {
  Str *tmp = Strnew();

  if (fi->type != FORM_SELECT && fi->type != FORM_TEXTAREA)
    Strcat_charp(tmp, "input type=");
  Strcat_charp(tmp, _formtypetbl[fi->type]);
  if (fi->name && fi->name->length)
    Strcat_m_charp(tmp, " name=\"", fi->name->ptr, "\"", NULL);
  if ((fi->type == FORM_INPUT_RADIO || fi->type == FORM_INPUT_CHECKBOX ||
       fi->type == FORM_SELECT) &&
      fi->value)
    Strcat_m_charp(tmp, " value=\"", fi->value->ptr, "\"", NULL);
  Strcat_m_charp(tmp, " (", _formmethodtbl[fi->parent->method], " ",
                 fi->parent->action->ptr, ")", NULL);
  return tmp->ptr;
}

int formtype(const char *typestr) {
  int i;
  for (i = 0; _formtypetbl[i]; i++) {
    if (!strcasecmp(typestr, _formtypetbl[i]))
      return i;
  }
  return FORM_INPUT_TEXT;
}

void formRecheckRadio(Anchor *a, Buffer *buf, FormItemList *fi) {
  int i;
  Anchor *a2;
  FormItemList *f2;

  for (i = 0; i < buf->formitem->nanchor; i++) {
    a2 = &buf->formitem->anchors[i];
    f2 = (FormItemList *)a2->url;
    if (f2->parent == fi->parent && f2 != fi && f2->type == FORM_INPUT_RADIO &&
        Strcmp(f2->name, fi->name) == 0) {
      f2->checked = 0;
      formUpdateBuffer(a2, buf, f2);
    }
  }
  fi->checked = 1;
  formUpdateBuffer(a, buf, fi);
}

void formResetBuffer(Buffer *buf, AnchorList *formitem) {
  int i;
  Anchor *a;
  FormItemList *f1, *f2;

  if (buf == NULL || buf->formitem == NULL || formitem == NULL)
    return;
  for (i = 0; i < buf->formitem->nanchor && i < formitem->nanchor; i++) {
    a = &buf->formitem->anchors[i];
    if (a->y != a->start.line)
      continue;
    f1 = (FormItemList *)a->url;
    f2 = (FormItemList *)formitem->anchors[i].url;
    if (f1->type != f2->type ||
        strcmp(((f1->name == NULL) ? "" : f1->name->ptr),
               ((f2->name == NULL) ? "" : f2->name->ptr)))
      break; /* What's happening */
    switch (f1->type) {
    case FORM_INPUT_TEXT:
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_FILE:
    case FORM_TEXTAREA:
      f1->value = f2->value;
      f1->init_value = f2->init_value;
      break;
    case FORM_INPUT_CHECKBOX:
    case FORM_INPUT_RADIO:
      f1->checked = f2->checked;
      f1->init_checked = f2->init_checked;
      break;
    case FORM_SELECT:
      break;
    default:
      continue;
    }
    formUpdateBuffer(a, buf, f1);
  }
}

static int form_update_line(Line *line, char **str, int spos, int epos,
                            int width, int newline, int password) {
  int c_len = 1, c_width = 1, w, i, len, pos;
  char *p, *buf;
  Lineprop c_type, effect, *prop;

  for (p = *str, w = 0, pos = 0; *p && w < width;) {
    c_type = get_mctype(p);
    if (c_type == PC_CTRL) {
      if (newline && *p == '\n')
        break;
      if (*p != '\r') {
        w++;
        pos++;
      }
    } else if (password) {
      w += c_width;
      pos += c_width;
      w += c_width;
      pos += c_len;
    }
    p += c_len;
  }
  pos += width - w;

  len = line->len + pos + spos - epos;
  buf = (char *)New_N(char, len + 1);
  buf[len] = '\0';
  prop = (Lineprop *)New_N(Lineprop, len);
  bcopy((void *)line->lineBuf, (void *)buf, spos * sizeof(char));
  bcopy((void *)line->propBuf, (void *)prop, spos * sizeof(Lineprop));

  effect = CharEffect(line->propBuf[spos]);
  for (p = *str, w = 0, pos = spos; *p && w < width;) {
    c_type = get_mctype(p);
    if (c_type == PC_CTRL) {
      if (newline && *p == '\n')
        break;
      if (*p != '\r') {
        buf[pos] = password ? '*' : ' ';
        prop[pos] = effect | PC_ASCII;
        pos++;
        w++;
      }
    } else if (password) {
      for (i = 0; i < c_width; i++) {
        buf[pos] = '*';
        prop[pos] = effect | PC_ASCII;
        pos++;
        w++;
      }
    } else {
      buf[pos] = *p;
      prop[pos] = effect | c_type;
      pos++;
      w += c_width;
    }
    p += c_len;
  }
  for (; w < width; w++) {
    buf[pos] = ' ';
    prop[pos] = effect | PC_ASCII;
    pos++;
  }
  if (newline) {
    if (!FoldTextarea) {
      while (*p && *p != '\r' && *p != '\n')
        p++;
    }
    if (*p == '\r')
      p++;
    if (*p == '\n')
      p++;
  }
  *str = p;

  bcopy((void *)&line->lineBuf[epos], (void *)&buf[pos],
        (line->len - epos) * sizeof(char));
  bcopy((void *)&line->propBuf[epos], (void *)&prop[pos],
        (line->len - epos) * sizeof(Lineprop));
  line->lineBuf = buf;
  line->propBuf = prop;
  line->len = len;
  line->size = len;

  return pos;
}

void formUpdateBuffer(Anchor *a, Buffer *buf, FormItemList *form) {
  Buffer save;
  char *p;
  int spos, epos, rows, c_rows, pos, col = 0;
  Line *l;

  copyBuffer(&save, buf);
  gotoLine(buf, a->start.line);
  switch (form->type) {
  case FORM_TEXTAREA:
  case FORM_INPUT_TEXT:
  case FORM_INPUT_FILE:
  case FORM_INPUT_PASSWORD:
  case FORM_INPUT_CHECKBOX:
  case FORM_INPUT_RADIO:
    spos = a->start.pos;
    epos = a->end.pos;
    break;
  default:
    spos = a->start.pos + 1;
    epos = a->end.pos - 1;
  }
  switch (form->type) {
  case FORM_INPUT_CHECKBOX:
  case FORM_INPUT_RADIO:
    if (buf->currentLine == NULL || spos >= buf->currentLine->len || spos < 0)
      break;
    if (form->checked)
      buf->currentLine->lineBuf[spos] = '*';
    else
      buf->currentLine->lineBuf[spos] = ' ';
    break;
  case FORM_INPUT_TEXT:
  case FORM_INPUT_FILE:
  case FORM_INPUT_PASSWORD:
  case FORM_TEXTAREA: {
    if (!form->value)
      break;
    p = form->value->ptr;
  }
    l = buf->currentLine;
    if (!l)
      break;
    if (form->type == FORM_TEXTAREA) {
      int n = a->y - buf->currentLine->linenumber;
      if (n > 0)
        for (; l && n; l = l->prev, n--)
          ;
      else if (n < 0)
        for (; l && n; l = l->prev, n++)
          ;
      if (!l)
        break;
    }
    rows = form->rows ? form->rows : 1;
    col = COLPOS(l, a->start.pos);
    for (c_rows = 0; c_rows < rows; c_rows++, l = l->next) {
      if (l == NULL)
        break;
      if (rows > 1) {
        pos = columnPos(l, col);
        a = retrieveAnchor(buf->formitem, l->linenumber, pos);
        if (a == NULL)
          break;
        spos = a->start.pos;
        epos = a->end.pos;
      }
      if (a->start.line != a->end.line || spos > epos || epos >= l->len ||
          spos < 0 || epos < 0 || COLPOS(l, epos) < col)
        break;
      pos = form_update_line(l, &p, spos, epos, COLPOS(l, epos) - col, rows > 1,
                             form->type == FORM_INPUT_PASSWORD);
      if (pos != epos) {
        shiftAnchorPosition(buf->href, buf->hmarklist, a->start.line, spos,
                            pos - epos);
        shiftAnchorPosition(buf->name, buf->hmarklist, a->start.line, spos,
                            pos - epos);
        shiftAnchorPosition(buf->img, buf->hmarklist, a->start.line, spos,
                            pos - epos);
        shiftAnchorPosition(buf->formitem, buf->hmarklist, a->start.line, spos,
                            pos - epos);
      }
    }
    break;
  }
  copyBuffer(buf, &save);
  arrangeLine(buf);
}

Str *textfieldrep(Str *s, int width) {
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

static void form_fputs_decode(Str *s, FILE *f) {
  char *p;
  Str *z = Strnew();

  for (p = s->ptr; *p;) {
    switch (*p) {

    case '\r':
      if (*(p + 1) == '\n')
        p++;
    default:
      Strcat_char(z, *p);
      p++;
      break;
    }
  }
  Strfputs(z, f);
}

void input_textarea(FormItemList *fi) {
  char *tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
  Str *tmp;
  FILE *f;

  f = fopen(tmpf, "w");
  if (f == NULL) {
    /* FIXME: gettextize? */
    disp_err_message("Can't open temporary file", FALSE);
    return;
  }
  if (fi->value)
    form_fputs_decode(fi->value, f);
  fclose(f);

  if (exec_cmd(myEditor(Editor, tmpf, 1)->ptr))
    goto input_end;

  if (fi->readonly)
    goto input_end;
  f = fopen(tmpf, "r");
  if (f == NULL) {
    /* FIXME: gettextize? */
    disp_err_message("Can't open temporary file", FALSE);
    goto input_end;
  }
  fi->value = Strnew();
  while (tmp = Strfgets(f), tmp->length > 0) {
    if (tmp->length == 1 && tmp->ptr[tmp->length - 1] == '\n') {
      /* null line with bare LF */
      tmp = Strnew_charp("\r\n");
    } else if (tmp->length > 1 && tmp->ptr[tmp->length - 1] == '\n' &&
               tmp->ptr[tmp->length - 2] != '\r') {
      Strshrink(tmp, 1);
      Strcat_charp(tmp, "\r\n");
    }
    tmp = convertLine(NULL, tmp, RAW_MODE, &charset, DisplayCharset);
    Strcat(fi->value, tmp);
  }
  fclose(f);
input_end:
  unlink(tmpf);
}

void do_internal(char *action, char *data) {
  int i;

  for (i = 0; internal_action[i].action; i++) {
    if (strcasecmp(internal_action[i].action, action) == 0) {
      if (internal_action[i].rout)
        internal_action[i].rout(cgistr2tagarg(data));
      return;
    }
  }
}

void form_write_data(FILE *f, char *boundary, char *name, char *value) {
  fprintf(f, "--%s\r\n", boundary);
  fprintf(f, "Content-Disposition: form-data; name=\"%s\"\r\n\r\n", name);
  fprintf(f, "%s\r\n", value);
}

void form_write_from_file(FILE *f, char *boundary, char *name, char *filename,
                          char *file) {
  FILE *fd;
  struct stat st;
  int c;

  fprintf(f, "--%s\r\n", boundary);
  fprintf(f, "Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n",
          name, mybasename(filename));
  auto type = guessContentType(file);
  fprintf(f, "Content-Type: %s\r\n\r\n",
          type ? type : "application/octet-stream");

  if (lstat(file, &st) < 0)
    goto write_end;
  if (S_ISDIR(st.st_mode))
    goto write_end;
  fd = fopen(file, "r");
  if (fd != NULL) {
    while ((c = fgetc(fd)) != EOF)
      fputc(c, f);
    fclose(fd);
  }
write_end:
  fprintf(f, "\r\n");
}

struct pre_form_item {
  int type;
  char *name;
  char *value;
  int checked;
  struct pre_form_item *next;
};

struct pre_form {
  char *url;
  Regex *re_url;
  char *name;
  char *action;
  struct pre_form_item *item;
  struct pre_form *next;
};

static struct pre_form *PreForm = NULL;

static struct pre_form *add_pre_form(struct pre_form *prev, char *url,
                                     Regex *re_url, char *name, char *action) {
  ParsedURL pu;
  struct pre_form *_new;

  if (prev)
    _new = prev->next = (struct pre_form *)New(struct pre_form);
  else
    _new = PreForm = (struct pre_form *)New(struct pre_form);
  if (url && !re_url) {
    parseURL2(url, &pu, NULL);
    _new->url = parsedURL2Str(&pu)->ptr;
  } else
    _new->url = url;
  _new->re_url = re_url;
  _new->name = (name && *name) ? name : NULL;
  _new->action = (action && *action) ? action : NULL;
  _new->item = NULL;
  _new->next = NULL;
  return _new;
}

static struct pre_form_item *add_pre_form_item(struct pre_form *pf,
                                               struct pre_form_item *prev,
                                               int type, char *name,
                                               char *value, char *checked) {
  struct pre_form_item *_new;

  if (!pf)
    return NULL;
  if (prev)
    _new = prev->next = (struct pre_form_item *)New(struct pre_form_item);
  else
    _new = pf->item = (struct pre_form_item *)New(struct pre_form_item);
  _new->type = type;
  _new->name = name;
  _new->value = value;
  if (checked && *checked &&
      (!strcmp(checked, "0") || !strcasecmp(checked, "off") ||
       !strcasecmp(checked, "no")))
    _new->checked = 0;
  else
    _new->checked = 1;
  _new->next = NULL;
  return _new;
}

/*
 * url <url>|/<re-url>/
 * form [<name>] <action>
 * text <name> <value>
 * file <name> <value>
 * passwd <name> <value>
 * checkbox <name> <value> [<checked>]
 * radio <name> <value>
 * select <name> <value>
 * submit [<name> [<value>]]
 * image [<name> [<value>]]
 * textarea <name>
 * <value>
 * /textarea
 */

void loadPreForm(void) {
  FILE *fp;
  Str *line = NULL;
  Str *textarea = NULL;
  struct pre_form *pf = NULL;
  struct pre_form_item *pi = NULL;
  int type = -1;
  char *name = NULL;

  PreForm = NULL;
  fp = openSecretFile(pre_form_file);
  if (fp == NULL)
    return;
  while (1) {
    char *p, *s, *arg;
    Regex *re_arg;

    line = Strfgets(fp);
    if (line->length == 0)
      break;
    if (textarea &&
        !(!strncmp(line->ptr, "/textarea", 9) && IS_SPACE(line->ptr[9]))) {
      Strcat(textarea, line);
      continue;
    }
    Strchop(line);
    Strremovefirstspaces(line);
    p = line->ptr;
    if (*p == '#' || *p == '\0')
      continue; /* comment or empty line */
    s = getWord(&p);

    if (!strcmp(s, "url")) {
      arg = getRegexWord((const char **)&p, &re_arg);
      if (!arg || !*arg)
        continue;
      p = getQWord(&p);
      pf = add_pre_form(pf, arg, re_arg, NULL, p);
      pi = pf->item;
      continue;
    }
    if (!pf)
      continue;

    arg = getWord(&p);
    if (!strcmp(s, "form")) {
      if (!arg || !*arg)
        continue;
      s = getQWord(&p);
      p = getQWord(&p);
      if (!p || !*p) {
        p = s;
        s = NULL;
      }
      if (pf->item) {
        struct pre_form *prev = pf;
        pf = add_pre_form(prev, "", NULL, s, p);
        /* copy previous URL */
        pf->url = prev->url;
        pf->re_url = prev->re_url;
      } else {
        pf->name = s;
        pf->action = (p && *p) ? p : NULL;
      }
      pi = pf->item;
      continue;
    }
    if (!strcmp(s, "text"))
      type = FORM_INPUT_TEXT;
    else if (!strcmp(s, "file"))
      type = FORM_INPUT_FILE;
    else if (!strcmp(s, "passwd") || !strcmp(s, "password"))
      type = FORM_INPUT_PASSWORD;
    else if (!strcmp(s, "checkbox"))
      type = FORM_INPUT_CHECKBOX;
    else if (!strcmp(s, "radio"))
      type = FORM_INPUT_RADIO;
    else if (!strcmp(s, "submit"))
      type = FORM_INPUT_SUBMIT;
    else if (!strcmp(s, "image"))
      type = FORM_INPUT_IMAGE;
    else if (!strcmp(s, "select"))
      type = FORM_SELECT;
    else if (!strcmp(s, "textarea")) {
      type = FORM_TEXTAREA;
      name = Strnew_charp(arg)->ptr;
      textarea = Strnew();
      continue;
    } else if (textarea && name && !strcmp(s, "/textarea")) {
      pi = add_pre_form_item(pf, pi, type, name, textarea->ptr, NULL);
      textarea = NULL;
      name = NULL;
      continue;
    } else
      continue;
    s = getQWord(&p);
    pi = add_pre_form_item(pf, pi, type, arg, s, getQWord(&p));
  }
  fclose(fp);
}

void preFormUpdateBuffer(Buffer *buf) {
  struct pre_form *pf;
  struct pre_form_item *pi;
  int i;
  Anchor *a;
  FormList *fl;
  FormItemList *fi;

  if (!buf || !buf->formitem || !PreForm)
    return;

  for (pf = PreForm; pf; pf = pf->next) {
    if (pf->re_url) {
      Str *url = parsedURL2Str(&buf->currentURL);
      if (!RegexMatch(pf->re_url, url->ptr, url->length, 1))
        continue;
    } else if (pf->url) {
      if (Strcmp_charp(parsedURL2Str(&buf->currentURL), pf->url))
        continue;
    } else
      continue;
    for (i = 0; i < buf->formitem->nanchor; i++) {
      a = &buf->formitem->anchors[i];
      fi = (FormItemList *)a->url;
      fl = fi->parent;
      if (pf->name && (!fl->name || strcmp(fl->name, pf->name)))
        continue;
      if (pf->action && (!fl->action || Strcmp_charp(fl->action, pf->action)))
        continue;
      for (pi = pf->item; pi; pi = pi->next) {
        if (pi->type != fi->type)
          continue;
        if (pi->type == FORM_INPUT_SUBMIT || pi->type == FORM_INPUT_IMAGE) {
          if ((!pi->name || !*pi->name ||
               (fi->name && !Strcmp_charp(fi->name, pi->name))) &&
              (!pi->value || !*pi->value ||
               (fi->value && !Strcmp_charp(fi->value, pi->value))))
            buf->submit = a;
          continue;
        }
        if (!pi->name || !fi->name || Strcmp_charp(fi->name, pi->name))
          continue;
        switch (pi->type) {
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
        case FORM_INPUT_PASSWORD:
        case FORM_TEXTAREA:
          fi->value = Strnew_charp(pi->value);
          formUpdateBuffer(a, buf, fi);
          break;
        case FORM_INPUT_CHECKBOX:
          if (pi->value && fi->value && !Strcmp_charp(fi->value, pi->value)) {
            fi->checked = pi->checked;
            formUpdateBuffer(a, buf, fi);
          }
          break;
        case FORM_INPUT_RADIO:
          if (pi->value && fi->value && !Strcmp_charp(fi->value, pi->value))
            formRecheckRadio(a, buf, fi);
          break;
        }
      }
    }
  }
}
