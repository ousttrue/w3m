/*
 * HTML forms
 */
#include "form.h"
#include "quote.h"
#include "html_parser.h"
#include "form_item.h"
#include "app.h"
#include "http_response.h"
#include "auth_pass.h"
#include "w3m.h"
#include "etc.h"
#include "mimetypes.h"
#include "readbuffer.h"
#include "alloc.h"
#include "html.h"
#include "utf8.h"
#include "line.h"
#include "buffer.h"
#include "keyvalue.h"
#include "html_tag.h"
#include "myctype.h"
#include "local_cgi.h"
#include "regex.h"
#include "proto.h"
#include <sys/stat.h>

/* *INDENT-OFF* */
struct {
  const char *action;
  void (*rout)(struct keyvalue *);
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
FormItemList *FormList ::formList_addInput(HtmlParser *parser,
                                           struct HtmlTag *tag) {
  /* if not in <form>..</form> environment, just ignore <input> tag */
  // if (fl == NULL)
  //   return NULL;

  auto item = (FormItemList *)New(FormItemList);
  item->type = FORM_UNKNOWN;
  item->size = -1;
  item->rows = 0;
  item->checked = item->init_checked = 0;
  item->accept = 0;
  item->name = NULL;
  item->value = item->init_value = NULL;
  item->readonly = 0;
  char *p;
  if (tag->parsedtag_get_value(ATTR_TYPE, &p)) {
    item->type = formtype(p);
    if (item->size < 0 &&
        (item->type == FORM_INPUT_TEXT || item->type == FORM_INPUT_FILE ||
         item->type == FORM_INPUT_PASSWORD))
      item->size = FORM_I_TEXT_DEFAULT_SIZE;
  }
  if (tag->parsedtag_get_value(ATTR_NAME, &p))
    item->name = Strnew_charp(p);
  if (tag->parsedtag_get_value(ATTR_VALUE, &p))
    item->value = item->init_value = Strnew_charp(p);
  item->checked = item->init_checked = tag->parsedtag_exists(ATTR_CHECKED);
  item->accept = tag->parsedtag_exists(ATTR_ACCEPT);
  tag->parsedtag_get_value(ATTR_SIZE, &item->size);
  tag->parsedtag_get_value(ATTR_MAXLENGTH, &item->maxlength);
  item->readonly = tag->parsedtag_exists(ATTR_READONLY);
  int i;
  if (tag->parsedtag_get_value(ATTR_TEXTAREANUMBER, &i) && i >= 0 &&
      i < parser->textarea_str.size()) {
    item->value = item->init_value = Strnew(parser->textarea_str[i]);
  }
  if (tag->parsedtag_get_value(ATTR_ROWS, &p))
    item->rows = atoi(p);
  if (item->type == FORM_UNKNOWN) {
    /* type attribute is missing. Ignore the tag. */
    return NULL;
  }
  if (item->type == FORM_INPUT_FILE && item->value && item->value->length) {
    /* security hole ! */
    return NULL;
  }
  item->parent = this;
  item->next = NULL;
  if (!this->item) {
    this->item = this->lastitem = item;
  } else {
    this->lastitem->next = item;
    this->lastitem = item;
  }
  if (item->type == FORM_INPUT_HIDDEN) {
    return NULL;
  }
  this->nitems++;
  return item;
}

void formRecheckRadio(FormAnchor *a, Buffer *buf, FormItemList *fi) {
  for (size_t i = 0; i < buf->layout.data._formitem->size(); i++) {
    auto a2 = &buf->layout.data._formitem->anchors[i];
    auto f2 = a2->formItem;
    if (f2->parent == fi->parent && f2 != fi && f2->type == FORM_INPUT_RADIO &&
        Strcmp(f2->name, fi->name) == 0) {
      f2->checked = 0;
      buf->layout.formUpdateBuffer(a2);
    }
  }
  fi->checked = 1;
  buf->layout.formUpdateBuffer(a);
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
  auto tmpf = App::instance().tmpfname(TMPF_DFL, {});
  auto f = fopen(tmpf.c_str(), "w");
  if (f == NULL) {
    App::instance().disp_err_message("Can't open temporary file");
    return;
  }
  if (fi->value) {
    form_fputs_decode(fi->value, f);
  }
  fclose(f);

  if (exec_cmd(App::instance().myEditor(tmpf.c_str(), 1).c_str())) {
    goto input_end;
  }

  if (fi->readonly) {
    goto input_end;
  }

  f = fopen(tmpf.c_str(), "r");
  if (f == NULL) {
    App::instance().disp_err_message("Can't open temporary file");
    goto input_end;
  }
  fi->value = Strnew();
  Str *tmp;
  while (tmp = Strfgets(f), tmp->length > 0) {
    if (tmp->length == 1 && tmp->ptr[tmp->length - 1] == '\n') {
      /* null line with bare LF */
      tmp = Strnew_charp("\r\n");
    } else if (tmp->length > 1 && tmp->ptr[tmp->length - 1] == '\n' &&
               tmp->ptr[tmp->length - 2] != '\r') {
      Strshrink(tmp, 1);
      Strcat_charp(tmp, "\r\n");
    }
    cleanup_line(tmp, RAW_MODE);
    Strcat(fi->value, tmp);
  }
  fclose(f);
input_end:
  unlink(tmpf.c_str());
}

void do_internal(char *action, char *data) {
  for (int i = 0; internal_action[i].action; i++) {
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
#ifdef _MSC_VER
#else
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
#endif
}

struct pre_form_item {
  int type;
  const char *name;
  const char *value;
  int checked;
  struct pre_form_item *next;
};

struct pre_form {
  const char *url;
  Regex *re_url;
  const char *name;
  const char *action;
  struct pre_form_item *item;
  struct pre_form *next;
};

static struct pre_form *PreForm = NULL;

static struct pre_form *add_pre_form(struct pre_form *prev, const char *url,
                                     Regex *re_url, const char *name,
                                     const char *action) {
  Url pu;
  struct pre_form *_new;

  if (prev)
    _new = prev->next = (struct pre_form *)New(struct pre_form);
  else
    _new = PreForm = (struct pre_form *)New(struct pre_form);
  if (url && !re_url) {
    pu = urlParse(url);
    _new->url = Strnew(pu.to_Str())->ptr;
  } else
    _new->url = url;
  _new->re_url = re_url;
  _new->name = (name && *name) ? name : NULL;
  _new->action = (action && *action) ? action : NULL;
  _new->item = NULL;
  _new->next = NULL;
  return _new;
}

static struct pre_form_item *
add_pre_form_item(struct pre_form *pf, struct pre_form_item *prev, int type,
                  const char *name, const char *value, const char *checked) {
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
  const char *name = NULL;

  PreForm = NULL;
  fp = openSecretFile(pre_form_file);
  if (fp == NULL)
    return;
  while (1) {
    const char *p, *s, *arg;
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

void preFormUpdateBuffer(const std::shared_ptr<Buffer> &buf) {

  if (!buf || !buf->layout.data._formitem || !PreForm)
    return;

  struct pre_form *pf;
  struct pre_form_item *pi;
  for (pf = PreForm; pf; pf = pf->next) {
    if (pf->re_url) {
      auto url = buf->res->currentURL.to_Str();
      if (!RegexMatch(pf->re_url, url.c_str(), url.size(), 1))
        continue;
    } else if (pf->url) {
      if (Strcmp(buf->res->currentURL.to_Str(), pf->url))
        continue;
    } else
      continue;
    for (size_t i = 0; i < buf->layout.data._formitem->size(); i++) {
      auto a = &buf->layout.data._formitem->anchors[i];
      auto fi = a->formItem;
      auto fl = fi->parent;
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
            buf->layout.data.submit = a;
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
          buf->layout.formUpdateBuffer(a);
          break;

        case FORM_INPUT_CHECKBOX:
          if (pi->value && fi->value && !Strcmp_charp(fi->value, pi->value)) {
            fi->checked = pi->checked;
            buf->layout.formUpdateBuffer(a);
          }
          break;
        case FORM_INPUT_RADIO:
          if (pi->value && fi->value && !Strcmp_charp(fi->value, pi->value))
            formRecheckRadio(a, buf.get(), fi);
          break;
        }
      }
    }
  }
}

Str *FormItemList::query_from_followform() {
  auto query = Strnew();
  for (auto f2 = this->parent->item; f2; f2 = f2->next) {
    if (f2->name == nullptr)
      continue;
    /* <ISINDEX> is translated into single text form */
    if (f2->name->length == 0 && f2->type != FORM_INPUT_TEXT)
      continue;
    switch (f2->type) {
    case FORM_INPUT_RESET:
      /* do nothing */
      continue;
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_IMAGE:
      if (f2 != this || f2->value == nullptr)
        continue;
      break;
    case FORM_INPUT_RADIO:
    case FORM_INPUT_CHECKBOX:
      if (!f2->checked)
        continue;
    }
    {
      /* not multipart */
      if (f2->type == FORM_INPUT_IMAGE) {
        int x = 0, y = 0;
        Strcat(query, Str_form_quote(f2->name));
        Strcat(query, Sprintf(".x=%d&", x));
        Strcat(query, Str_form_quote(f2->name));
        Strcat(query, Sprintf(".y=%d", y));
      } else {
        /* not IMAGE */
        if (f2->name && f2->name->length > 0) {
          Strcat(query, Str_form_quote(f2->name));
          Strcat_char(query, '=');
        }
        if (f2->value != nullptr) {
          if (this->parent->method == FORM_METHOD_INTERNAL)
            Strcat(query, Str_form_quote(f2->value));
          else {
            Strcat(query, Str_form_quote(f2->value));
          }
        }
      }
      if (f2->next)
        Strcat_char(query, '&');
    }
  }
  {
    /* remove trailing & */
    while (Strlastchar(query) == '&')
      Strshrink(query, 1);
  }
  return query;
}

void FormItemList ::query_from_followform_multipart() {
  auto tmpf = Strnew(App::instance().tmpfname(TMPF_DFL, {}));
  auto body = fopen(tmpf->ptr, "w");
  if (body == nullptr) {
    return;
  }
  this->parent->body = tmpf->ptr;
  this->parent->boundary = Sprintf("------------------------------%d%ld%ld%ld",
                                   App::instance().pid(), this->parent,
                                   this->parent->body, this->parent->boundary)
                               ->ptr;

  // auto query = Strnew();
  for (auto f2 = this->parent->item; f2; f2 = f2->next) {
    if (f2->name == nullptr)
      continue;
    /* <ISINDEX> is translated into single text form */
    if (f2->name->length == 0)
      continue;
    switch (f2->type) {
    case FORM_INPUT_RESET:
      /* do nothing */
      continue;
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_IMAGE:
      if (f2 != this || f2->value == nullptr)
        continue;
      break;
    case FORM_INPUT_RADIO:
    case FORM_INPUT_CHECKBOX:
      if (!f2->checked)
        continue;
    }
    {
      if (f2->type == FORM_INPUT_IMAGE) {
        int x = 0, y = 0;
        {
          auto query = f2->name->Strdup();
          Strcat_charp(query, ".x");
          form_write_data(body, this->parent->boundary, query->ptr,
                          Sprintf("%d", x)->ptr);
        }
        {
          auto query = f2->name->Strdup();
          Strcat_charp(query, ".y");
          form_write_data(body, this->parent->boundary, query->ptr,
                          Sprintf("%d", y)->ptr);
        }
      } else if (f2->name && f2->name->length > 0 && f2->value != nullptr) {
        /* not IMAGE */
        {
          auto query = f2->value;
          if (f2->type == FORM_INPUT_FILE)
            form_write_from_file(body, this->parent->boundary, f2->name->ptr,
                                 query->ptr, f2->value->ptr);
          else
            form_write_data(body, this->parent->boundary, f2->name->ptr,
                            query->ptr);
        }
      }
    }
  }
  {
    fprintf(body, "--%s--\r\n", this->parent->boundary);
    fclose(body);
  }
}

Str *Str_form_quote(Str *x) {
  Str *tmp = {};
  char *p = x->ptr, *ep = x->ptr + x->length;
  char buf[4];

  for (; p < ep; p++) {
    if (*p == ' ') {
      if (tmp == NULL)
        tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
      Strcat_char(tmp, '+');
    } else if (is_url_unsafe(*p)) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)*p);
      Strcat_charp(tmp, buf);
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (tmp)
    return tmp;
  return x;
}
